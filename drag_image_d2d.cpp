#include "stdafx.h"

#include "drag_image_d2d.h"

#include "d2d_bitmap_renderer.h"
#include "direct_2d.h"
#include "direct_3d.h"

namespace uih {

namespace {

constexpr auto drag_image_width = 80.f;
constexpr auto drag_image_height = 75.f;
constexpr auto drag_image_padding = 5.f;

} // namespace

class D2DDragImageCreator::D2DDragImageCreatorImpl {
public:
    explicit D2DDragImageCreatorImpl(float dpi, float width_dip = 80.f, float height_dip = 75.f, float margin_dip = 5.f)
        : m_d2d_bitmap_renderer(dpi, width_dip, height_dip, true)
        , m_margin_dip(margin_dip)
    {
    }

    SizedHbitmap create_drag_image(bool is_dark, IDWriteTextLayout* text_layout, bool use_aliased_text)
    {
        return m_d2d_bitmap_renderer.render(
            [this, is_dark, text_layout, use_aliased_text](const auto& device_context, const auto& _) {
                const auto is_high_contrast = is_high_contrast_active();

                device_context->SetTextAntialiasMode(
                    use_aliased_text ? D2D1_TEXT_ANTIALIAS_MODE_ALIASED : D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

                const auto make_brush = [&](const D2D1_COLOR_F& colour) {
                    wil::com_ptr<ID2D1SolidColorBrush> brush;
                    THROW_IF_FAILED(device_context->CreateSolidColorBrush(colour, &brush));
                    return brush;
                };

                const auto border_brush = make_brush(border_colour(is_dark, is_high_contrast));
                const auto main_background_brush = make_brush(background_colour(is_dark, is_high_contrast));
                const auto text_background_brush = make_brush(text_background_colour(is_dark, is_high_contrast));
                const auto text_brush = make_brush(text_colour(is_dark, is_high_contrast));

                DWRITE_TEXT_METRICS metrics{};

                if (text_layout)
                    THROW_IF_FAILED(text_layout->GetMetrics(&metrics));

                constexpr auto left_right_padding = 3.f;
                constexpr auto top_bottom_padding = 2.f;

                device_context->BeginDraw();
                device_context->Clear();
                const auto size = device_context->GetSize();
                const auto background_rect = D2D1::RectF(0.5f, 0.5f, size.width - .5f, size.height - .5f);
                const auto background_rounded_rect = D2D1::RoundedRect(background_rect, 4.f, 4.f);
                device_context->FillRoundedRectangle(background_rounded_rect, main_background_brush.get());
                device_context->DrawRoundedRectangle(background_rounded_rect, border_brush.get());

                if (text_layout) {
                    const auto text_background_rect = D2D1::RectF(metrics.left - left_right_padding + m_margin_dip,
                        metrics.top - top_bottom_padding + m_margin_dip,
                        metrics.left + metrics.width + left_right_padding + m_margin_dip,
                        metrics.top + metrics.height + top_bottom_padding + m_margin_dip);

                    const auto text_rounded_rect = D2D1::RoundedRect(text_background_rect, 3.f, 3.f);
                    device_context->FillRoundedRectangle(text_rounded_rect, text_background_brush.get());

                    device_context->DrawTextLayout({m_margin_dip, m_margin_dip}, text_layout, text_brush.get());
                }

                THROW_IF_FAILED(device_context->EndDraw());
            });
    }

private:
    static constexpr D2D1_COLOR_F border_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOWTEXT));

        return is_dark ? D2D1_COLOR_F{.7f, .7f, .7f, 0.75f}
                       : D2D1_COLOR_F{90.f / 255.f, 150.f / 255.f, 222.f / 255.f, 0.75f};
    }

    static constexpr D2D1_COLOR_F background_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOW));

        return is_dark ? D2D1_COLOR_F{.6f, .6f, .6f, 0.6f}
                       : D2D1_COLOR_F{201.f / 255.f, 235.f / 255.f, 252.f / 255.f, 0.4f};
    }

    static constexpr D2D1_COLOR_F text_background_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOW));

        return is_dark ? D2D1_COLOR_F{.15f, .15f, .15f, 1.f}
                       : D2D1_COLOR_F{0.f / 255.f, 116.f / 255.f, 204.f / 255.f, 1.f};
    }

    static constexpr D2D1_COLOR_F text_colour(bool is_dark, bool is_high_contrast)
    {
        if (is_high_contrast)
            return d2d::colorref_to_d2d_color(GetSysColor(COLOR_WINDOWTEXT));

        return is_dark ? D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f} : D2D1_COLOR_F{1.f, 1.f, 1.f, 1.f};
    }

    D2DBitmapRenderer m_d2d_bitmap_renderer;
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
