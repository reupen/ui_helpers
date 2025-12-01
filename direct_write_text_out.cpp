#include "stdafx.h"

#include "direct_write.h"
#include "direct_write_text_out.h"

using namespace std::string_view_literals;

namespace uih::direct_write {

namespace {

int text_out_colours(const TextFormat& text_format, HWND wnd, HDC dc, std::wstring_view text, const RECT& rect,
    bool selected, DWORD default_color, alignment align, bool enable_colour_codes, bool enable_ellipsis,
    wil::com_ptr<IDWriteBitmapRenderTarget> bitmap_render_target)
{
    if (is_rect_null_or_reversed(&rect) || rect.right <= rect.left)
        return 0;

    std::optional<std::wstring> processed_text;
    std::vector<ColouredTextSegment> segments;

    const auto scaling_factor = get_default_scaling_factor();
    const auto max_width = gsl::narrow_cast<float>(wil::rect_width(rect)) / scaling_factor;
    const auto max_height = gsl::narrow_cast<float>(wil::rect_height(rect)) / scaling_factor;
    const auto dwrite_alignment = get_text_alignment(align);

    auto layout = text_format.get_cached_text_layout(text, max_width, max_height, enable_ellipsis, dwrite_alignment);

    if (!layout) {
        if (enable_colour_codes)
            std::tie(processed_text, segments) = process_colour_codes(text);

        if (align != ALIGN_LEFT) {
            if (!processed_text)
                processed_text = text;

            // Work around DirectWrite not rendering trailing whitespace
            // for centre- and right-aligned text
            processed_text->push_back(L'\u200b');
        }
    }

    try {
        if (!layout) {
            const auto render_text = processed_text ? *processed_text : text;

            layout = text_format.create_cached_text_layout(
                render_text, text, max_width, max_height, enable_ellipsis, dwrite_alignment);

            for (auto& [colour, selected_colour, start_character, character_count] : segments) {
                layout->set_colour(colour, selected_colour,
                    {gsl::narrow<uint32_t>(start_character), gsl::narrow<uint32_t>(character_count)});
            }
        }

        const auto metrics = layout->get_metrics();

        layout->render_with_transparent_background(wnd, dc, rect, default_color, selected, 0.0f, bitmap_render_target);

        return gsl::narrow_cast<int>(metrics.width * scaling_factor + 1);
    }
    CATCH_LOG()

    return 0;
}

} // namespace

DWRITE_TEXT_ALIGNMENT get_text_alignment(alignment alignment_)
{
    switch (alignment_) {
    case ALIGN_CENTRE:
        return DWRITE_TEXT_ALIGNMENT_CENTER;
    case ALIGN_RIGHT:
        return DWRITE_TEXT_ALIGNMENT_TRAILING;
    default:
        return DWRITE_TEXT_ALIGNMENT_LEADING;
    }
}

int text_out_columns_and_colours(TextFormat& text_format, HWND wnd, HDC dc, std::wstring_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, TextOutOptions options)
{
    RECT adjusted_rect = rect;

    if (is_rect_null_or_reversed(&adjusted_rect))
        return 0;

    int tab_count = 0;

    if (options.enable_tab_columns)
        tab_count = gsl::narrow<int>(std::ranges::count(text, L'\t'));

    if (tab_count == 0) {
        adjusted_rect.left += border + x_offset;
        adjusted_rect.right -= border;

        return text_out_colours(text_format, wnd, dc, text, adjusted_rect, options.is_selected, default_colour,
            options.align, options.enable_colour_codes, options.enable_ellipses, options.bitmap_render_target);
    }

    adjusted_rect.left += x_offset;
    const int total_width = adjusted_rect.right - adjusted_rect.left;

    auto position = text.length();
    int cell_index = 0;
    RECT cell_rect = adjusted_rect;

    do {
        const auto end = position;
        while (position > 0 && text[position - 1] != L'\t')
            position--;

        const auto cell_text = text.substr(position, end - position);

        if (!cell_text.empty()) {
            cell_rect.right -= border;

            if (cell_index != 0)
                cell_rect.left = std::min(
                    adjusted_rect.right - MulDiv(cell_index, total_width, tab_count) + border, cell_rect.right);

            const int cell_render_width = text_out_colours(text_format, wnd, dc, cell_text, cell_rect,
                options.is_selected, default_colour, cell_index == 0 ? ALIGN_RIGHT : ALIGN_LEFT,
                options.enable_colour_codes, false, options.bitmap_render_target);

            if (cell_index == 0)
                cell_rect.left = cell_rect.right - cell_render_width;

            cell_rect.right = cell_rect.left - border;
        }

        if (position > 0) {
            position--;
            cell_index++;
        }
    } while (position > 0);

    return total_width;
}

int text_out_columns_and_colours(TextFormat& text_format, HWND wnd, HDC dc, std::string_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, TextOutOptions options)
{
    return text_out_columns_and_colours(
        text_format, wnd, dc, mmh::to_utf16(text), x_offset, border, rect, default_colour, std::move(options));
}

} // namespace uih::direct_write
