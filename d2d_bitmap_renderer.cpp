#include "stdafx.h"

#include "direct_3d.h"
#include "d2d_bitmap_renderer.h"

namespace uih {

namespace {

uint8_t divide_by_alpha(uint8_t value, uint8_t alpha)
{
    if (alpha == 0)
        return value;

    return gsl::narrow_cast<uint8_t>(
        std::clamp(std::lround(static_cast<float>(value) * 255.f / static_cast<float>(alpha)), 0l, 255l));
}

void copy_and_unpremultiply_bitmap_data(
    const uint8_t* source, uint32_t source_pitch, uint8_t* dest, int width, int height)
{
    for (const auto row_index : std::ranges::views::iota(0, height)) {
        for (const auto column_index : std::ranges::views::iota(0, width)) {
            const auto dest_row_offset = row_index * width * 4;
            const auto source_row_offset = row_index * source_pitch;
            const auto column_offset = column_index * 4;

            const auto r = source[source_row_offset + column_offset];
            const auto g = source[source_row_offset + column_offset + 1];
            const auto b = source[source_row_offset + column_offset + 2];
            const auto a = source[source_row_offset + column_offset + 3];

            dest[dest_row_offset + column_offset + 0] = divide_by_alpha(r, a);
            dest[dest_row_offset + column_offset + 1] = divide_by_alpha(g, a);
            dest[dest_row_offset + column_offset + 2] = divide_by_alpha(b, a);
            dest[dest_row_offset + column_offset + 3] = a;
        }
    }
}

void copy_bitmap_data(const uint8_t* source, uint32_t source_pitch, uint8_t* dest, int width, int height)
{
    for (const auto row_index : std::ranges::views::iota(0, height)) {
        const auto dest_row_offset = row_index * width * 4;
        const auto source_row_offset = row_index * source_pitch;
        memcpy(dest + dest_row_offset, source + source_row_offset, width * 4);
    }
}

} // namespace

SizedHbitmap D2DBitmapRenderer::render(const std::function<void(const wil::com_ptr<ID2D1DeviceContext>& device_context,
        const wil::com_ptr<ID2D1Factory1>& factory)>& render_callback)
{
    create_device();

    try {
        render_callback(m_d2d_device_context, *m_d2d_factory);
        return try_create_hbitmap();
    } catch (...) {
        if (d2d::is_device_reset_error(wil::ResultFromCaughtException())) {
            clear_resources();
            create_device();
            render_callback(m_d2d_device_context, *m_d2d_factory);
            return try_create_hbitmap();
        }

        throw;
    }
}

SizedHbitmap D2DBitmapRenderer::try_create_hbitmap() const
{
    wil::com_ptr<ID2D1Bitmap1> cpu_bitmap;
    THROW_IF_FAILED(m_d2d_device_context->CreateBitmap({width_upx(), height_upx()}, nullptr, 0,
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        &cpu_bitmap));

    const auto render_rect = D2D1::RectU(0, 0, width_upx(), height_upx());
    THROW_IF_FAILED(cpu_bitmap->CopyFromRenderTarget(nullptr, m_d2d_device_context.get(), &render_rect));

    D2D1_MAPPED_RECT mapped_rect{};
    THROW_IF_FAILED(cpu_bitmap->Map(D2D1_MAP_OPTIONS_READ, &mapped_rect));

    auto _ = gsl::finally([&] { THROW_IF_FAILED(cpu_bitmap->Unmap()); });

    BITMAPINFOHEADER bmi{};
    bmi.biSize = sizeof(bmi);
    bmi.biWidth = width_px();
    bmi.biHeight = -height_px();
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    std::array<uint8_t, sizeof(BITMAPINFOHEADER)> bm_data{};

    auto* bi = reinterpret_cast<BITMAPINFO*>(bm_data.data());
    bi->bmiHeader = bmi;

    void* hbitmap_data{};
    wil::unique_hbitmap hbitmap(CreateDIBSection(nullptr, bi, DIB_RGB_COLORS, &hbitmap_data, nullptr, 0));

    if (!hbitmap || !hbitmap_data)
        THROW_LAST_ERROR();

    if (m_use_straight_alpha) {
        copy_and_unpremultiply_bitmap_data(
            mapped_rect.bits, mapped_rect.pitch, static_cast<uint8_t*>(hbitmap_data), width_px(), height_px());
    } else if (mapped_rect.pitch == width_px() * 4) {
        memcpy(hbitmap_data, mapped_rect.bits, width_px() * height_px() * 4);
    } else {
        copy_bitmap_data(
            mapped_rect.bits, mapped_rect.pitch, static_cast<uint8_t*>(hbitmap_data), width_px(), height_px());
    }

    return SizedHbitmap{std::move(hbitmap), width_px(), height_px()};
}

void D2DBitmapRenderer::create_device()
{
    if (m_d2d_device_context)
        return;

    if (!m_d2d_factory)
        m_d2d_factory = d2d::create_main_thread_factory();

    constexpr std::array feature_levels
        = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    m_d3d_device = d3d::create_d3d_device(D3D_DRIVER_TYPE_WARP, feature_levels);

    const auto dxgi_device = m_d3d_device.query<IDXGIDevice1>();

    THROW_IF_FAILED((*m_d2d_factory)->CreateDevice(dxgi_device.get(), &m_d2d_device));
    THROW_IF_FAILED(m_d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2d_device_context));

    create_target();
}

void D2DBitmapRenderer::create_target() const
{
    const auto bitmap_properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    THROW_IF_FAILED(m_d2d_device_context->CreateBitmap(
        {width_upx(), height_upx()}, nullptr, 0, &bitmap_properties, &target_bitmap));

    m_d2d_device_context->SetTarget(target_bitmap.get());
    m_d2d_device_context->SetDpi(m_dpi, m_dpi);
}

void D2DBitmapRenderer::clear_resources()
{
    m_d2d_device_context.reset();
    m_d2d_device.reset();
    m_d3d_device.reset();
}

} // namespace uih
