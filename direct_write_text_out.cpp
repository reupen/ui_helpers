#include "stdafx.h"

using namespace std::string_view_literals;

namespace uih::direct_write {

namespace {

int text_out_colours(const TextFormat& text_format, HDC dc, std::string_view text, const RECT& rect, bool selected,
    DWORD default_color, alignment align, bool enable_colour_codes)
{
    if (is_rect_null_or_reversed(&rect) || rect.right <= rect.left)
        return 0;

    auto [render_text, segments] = enable_colour_codes
        ? process_colour_codes(mmh::to_utf16(text), selected)
        : std::tuple(mmh::to_utf16(text), std::vector<ColouredTextSegment>{});

    text_format.set_alignment(get_text_alignment(align));

    try {
        const auto scaling_factor = get_default_scaling_factor();

        const auto layout = text_format.create_text_layout(render_text,
            gsl::narrow_cast<float>(wil::rect_width(rect)) / scaling_factor,
            gsl::narrow_cast<float>(wil::rect_height(rect)) / scaling_factor);

        for (auto& [colour, start_character, character_count] : segments) {
            layout.set_colour(colour, {gsl::narrow<uint32_t>(start_character), gsl::narrow<uint32_t>(character_count)});
        }

        const auto metrics = layout.get_metrics();

        layout.render(dc, rect, default_color);

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

int text_out_columns_and_colours(const TextFormat& text_format, HDC dc, std::string_view text, int x_offset, int border,
    const RECT& rect, bool selected, COLORREF default_colour, bool enable_colour_codes, bool enable_tab_columns,
    alignment align, bool enable_ellipses)
{
    RECT adjusted_rect = rect;

    if (is_rect_null_or_reversed(&adjusted_rect))
        return 0;

    int tab_count = 0;

    if (enable_tab_columns) {
        for (size_t n = 0; n < text.length(); n++) {
            if (text[n] == '\t') {
                tab_count++;
            }
        }
    }

    if (tab_count == 0) {
        adjusted_rect.left += border + x_offset;
        adjusted_rect.right -= border;

        if (enable_ellipses)
            text_format.enable_trimming_sign();
        else
            text_format.disable_trimming_sign();

        return text_out_colours(
            text_format, dc, text, adjusted_rect, selected, default_colour, align, enable_colour_codes);
    }

    // Ellipses always disabled when using tab columns
    text_format.disable_trimming_sign();

    adjusted_rect.left += x_offset;
    const int total_width = adjusted_rect.right - adjusted_rect.left;

    auto position = text.length();
    int cell_index = 0;
    RECT cell_rect = adjusted_rect;

    do {
        const auto end = position;
        while (position > 0 && text[position - 1] != '\t')
            position--;

        const auto cell_text = text.substr(position, end - position);

        if (!cell_text.empty()) {
            cell_rect.right -= border;

            if (cell_index != 0)
                cell_rect.left = std::min(
                    adjusted_rect.right - MulDiv(cell_index, total_width, tab_count) + border, cell_rect.right);

            const int cell_render_width = text_out_colours(text_format, dc, cell_text, cell_rect, selected,
                default_colour, cell_index == 0 ? ALIGN_RIGHT : ALIGN_LEFT, enable_colour_codes);

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

} // namespace uih::direct_write
