#include "stdafx.h"

#include "drag_image_d2d.h"

#include "direct_2d.h"
#include "direct_3d.h"

namespace uih {

namespace {

constexpr auto drag_image_width = 80.f;
constexpr auto drag_image_height = 75.f;
constexpr auto drag_image_padding = 5.f;

uint8_t divide_by_alpha(uint8_t value, uint8_t alpha)
{
    if (alpha == 0)
        return value;

    return gsl::narrow_cast<uint8_t>(
        std::clamp(std::lround(static_cast<float>(value) * 255.f / static_cast<float>(alpha)), 0l, 255l));
}

void copy_and_unpremultiply_bitmap_data(uint8_t* source, uint8_t* dest, int width, int height)
{
    for (const auto row_index : std::ranges::views::iota(0, height)) {
        for (const auto column_index : std::ranges::views::iota(0, width)) {
            const auto row_offset = row_index * width * 4;
            const auto column_offset = column_index * 4;

            const auto r = source[row_offset + column_offset];
            const auto g = source[row_offset + column_offset + 1];
            const auto b = source[row_offset + column_offset + 2];
            const auto a = source[row_offset + column_offset + 3];

            dest[row_offset + column_offset + 0] = divide_by_alpha(r, a);
            dest[row_offset + column_offset + 1] = divide_by_alpha(g, a);
            dest[row_offset + column_offset + 2] = divide_by_alpha(b, a);
            dest[row_offset + column_offset + 3] = a;
        }
    }
}

struct DragImage {
    wil::unique_hbitmap bitmap;
    long width{};
    long height{};
};

} // namespace

class D2DDragImageCreator::D2DDragImageCreatorImpl {
public:
    explicit D2DDragImageCreatorImpl(float dpi, float width_dip = 80.f, float height_dip = 75.f, float margin_dip = 5.f)
        : m_dpi{dpi}
        , m_width_dip(width_dip)
        , m_height_dip(height_dip)
        , m_margin_dip(margin_dip)
    {
    }

    DragImage create_drag_image(bool is_dark, IDWriteTextLayout* text_layout, bool use_aliased_text)
    {
        create_device();

        try {
            return try_create_drag_image(is_dark, text_layout, use_aliased_text);
        } catch (...) {
            if (d2d::is_device_reset_error(wil::ResultFromCaughtException())) {
                clear_resources();
                create_device();
                return try_create_drag_image(is_dark, text_layout, use_aliased_text);
            }

            throw;
        }
    }

private:
    static constexpr auto border_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOWTEXT));

        return is_dark ? D2D1_COLOR_F{.7f, .7f, .7f, 0.75f}
                       : D2D1_COLOR_F{90.f / 255.f, 150.f / 255.f, 222.f / 255.f, 0.75f};
    }

    static constexpr auto background_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOW));

        return is_dark ? D2D1_COLOR_F{.6f, .6f, .6f, 0.6f}
                       : D2D1_COLOR_F{201.f / 255.f, 235.f / 255.f, 252.f / 255.f, 0.4f};
    }

    static constexpr auto text_background_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOW));

        return is_dark ? D2D1_COLOR_F{.15f, .15f, .15f, 1.f}
                       : D2D1_COLOR_F{0.f / 255.f, 116.f / 255.f, 204.f / 255.f, 1.f};
    }

    static constexpr auto text_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOWTEXT));

        return is_dark ? D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f} : D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f};
    }

    DragImage try_create_drag_image(bool is_dark, IDWriteTextLayout* text_layout, bool use_aliased_text) const
    {
        const auto is_high_contrast = is_high_contrast_active();

        m_d2d_device_context->SetTextAntialiasMode(
            use_aliased_text ? D2D1_TEXT_ANTIALIAS_MODE_ALIASED : D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        const auto border_brush = make_brush(border_colour(is_dark, is_high_contrast));
        const auto main_background_brush = make_brush(background_colour(is_dark, is_high_contrast));
        const auto text_background_brush = make_brush(text_background_colour(is_dark, is_high_contrast));
        const auto text_brush = make_brush(text_colour(is_dark, is_high_contrast));

        DWRITE_TEXT_METRICS metrics{};

        if (text_layout)
            THROW_IF_FAILED(text_layout->GetMetrics(&metrics));

        constexpr auto left_right_padding = 3.f;
        constexpr auto top_bottom_padding = 2.f;

        m_d2d_device_context->BeginDraw();
        m_d2d_device_context->Clear();
        const auto background_rect = D2D1::RectF(0.5f, 0.5f, m_width_dip - .5f, m_height_dip - .5f);
        const auto background_rounded_rect = D2D1::RoundedRect(background_rect, 4.f, 4.f);
        m_d2d_device_context->FillRoundedRectangle(background_rounded_rect, main_background_brush.get());
        m_d2d_device_context->DrawRoundedRectangle(background_rounded_rect, border_brush.get());

        if (text_layout) {
            const auto text_background_rect = D2D1::RectF(metrics.left - left_right_padding + m_margin_dip,
                metrics.top - top_bottom_padding + m_margin_dip,
                metrics.left + metrics.width + left_right_padding + m_margin_dip,
                metrics.top + metrics.height + top_bottom_padding + m_margin_dip);

            const auto text_rounded_rect = D2D1::RoundedRect(text_background_rect, 3.f, 3.f);
            m_d2d_device_context->FillRoundedRectangle(text_rounded_rect, text_background_brush.get());

            m_d2d_device_context->DrawTextLayout({m_margin_dip, m_margin_dip}, text_layout, text_brush.get());
        }

        THROW_IF_FAILED(m_d2d_device_context->EndDraw());

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

        GdiFlush();
        copy_and_unpremultiply_bitmap_data(
            mapped_rect.bits, static_cast<uint8_t*>(hbitmap_data), width_px(), height_px());

        return DragImage{std::move(hbitmap), width_px(), height_px()};
    }

    void create_device()
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

    void create_target() const
    {
        const auto bitmap_properties
            = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

        wil::com_ptr<ID2D1Bitmap1> target_bitmap;
        THROW_IF_FAILED(m_d2d_device_context->CreateBitmap(
            {width_upx(), height_upx()}, nullptr, 0, &bitmap_properties, &target_bitmap));

        m_d2d_device_context->SetTarget(target_bitmap.get());
        m_d2d_device_context->SetDpi(m_dpi, m_dpi);
    }

    void clear_resources()
    {
        m_d2d_device_context.reset();
        m_d2d_device.reset();
        m_d3d_device.reset();
    }

    wil::com_ptr<ID2D1SolidColorBrush> make_brush(const D2D1_COLOR_F& colour) const
    {
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(m_d2d_device_context->CreateSolidColorBrush(colour, &brush));
        return brush;
    }

    int32_t width_px() const { return std::lround(m_width_dip * m_dpi / 96.f); }
    int32_t height_px() const { return std::lround(m_height_dip * m_dpi / 96.f); }
    uint32_t width_upx() const { return gsl::narrow<uint32_t>(width_px()); }
    uint32_t height_upx() const { return gsl::narrow<uint32_t>(height_px()); }

    wil::com_ptr<ID3D11Device> m_d3d_device;
    d2d::MainThreadD2D1Factory m_d2d_factory;
    wil::com_ptr<ID2D1Device> m_d2d_device;
    wil::com_ptr<ID2D1DeviceContext> m_d2d_device_context;
    float m_dpi{};
    float m_width_dip{};
    float m_height_dip{};
    float m_margin_dip{};
};

D2DDragImageCreator::D2DDragImageCreator(float dpi) : m_impl{std::make_unique<D2DDragImageCreatorImpl>(dpi)} {}
D2DDragImageCreator::~D2DDragImageCreator() {}

bool D2DDragImageCreator::create_drag_image(
    HWND wnd, bool is_dark, direct_write::Context& context, std::wstring_view text, LPSHDRAGIMAGE lpsdi) const
{
    try {
        wil::com_ptr<IDWriteTextLayout> text_layout;
        BOOL are_fonts_smoothed{true};

        if (!text.empty()) {
            SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &are_fonts_smoothed, 0);

            const auto text_format = context.create_text_format(L"", DWRITE_FONT_WEIGHT_MEDIUM,
                DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, direct_write::pt_to_dip(9.f), {},
                are_fonts_smoothed ? DWRITE_RENDERING_MODE_DEFAULT : DWRITE_RENDERING_MODE_ALIASED);

            const auto layout_wrapper = text_format.create_text_layout(
                text, drag_image_width - drag_image_padding * 2, drag_image_height - drag_image_padding * 2);

            THROW_IF_FAILED(layout_wrapper.text_layout()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
            THROW_IF_FAILED(layout_wrapper.text_layout()->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));

            text_layout = layout_wrapper.text_layout();
        }

        auto [bitmap, width_px, height_px]
            = m_impl->create_drag_image(is_dark, text_layout.get(), are_fonts_smoothed == FALSE);
        lpsdi->sizeDragImage.cx = width_px;
        lpsdi->sizeDragImage.cy = height_px;
        lpsdi->ptOffset.x = width_px / 2;
        lpsdi->ptOffset.y = height_px - height_px / 10;
        lpsdi->hbmpDragImage = bitmap.release();
        lpsdi->crColorKey = 0xffffffff;
    } catch (...) {
        LOG_CAUGHT_EXCEPTION();
        return false;
    }

    return true;
}

} // namespace uih
