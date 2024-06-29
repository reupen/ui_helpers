#include "stdafx.h"

using namespace std::string_view_literals;

namespace uih::direct_write {

namespace {

int text_out_colours(const TextFormat& text_format, HDC dc, std::string_view text, const RECT& rect, bool selected,
    DWORD default_color, alignment align, bool enable_colour_codes)
{
    struct ColouredTextSegment {
        COLORREF colour{};
        size_t character_count{};
    };

    if (is_rect_null_or_reversed(&rect) || rect.right <= rect.left)
        return 0;

    const pfc::stringcvt::string_wide_from_utf8 utf16_text(text.data(), text.size());
    const std::wstring_view utf16_text_view(utf16_text.get_ptr(), utf16_text.length());
    std::vector<ColouredTextSegment> segments;
    std::wstring text_without_colours;

    size_t offset{};
    COLORREF cr_current = default_color;
    while (enable_colour_codes) {
        const size_t index = utf16_text_view.find(L"\3"sv, offset);

        const auto fragment_length = index == std::wstring_view::npos ? std::wstring_view::npos : index - offset;
        const auto fragment = utf16_text_view.substr(offset, fragment_length);

        if (!fragment.empty()) {
            segments.emplace_back(cr_current, fragment.length());
            text_without_colours.append(fragment);
        }

        if (index == std::wstring_view::npos)
            break;

        offset = utf16_text_view.find(L"\3"sv, index + 1);

        if (offset == std::wstring_view::npos)
            break;

        const auto colour_code = utf16_text_view.substr(index + 1, offset - index - 1);
        const auto bar_index = colour_code.find(L'|');
        const auto non_selected_colour_hex = colour_code.substr(0, bar_index);

        ++offset;

        if (non_selected_colour_hex.empty()) {
            cr_current = default_color;
            continue;
        }

        COLORREF non_selected_colour = mmh::strtoul_n(
            non_selected_colour_hex.data(), std::min(size_t{6}, non_selected_colour_hex.length()), 0x10);

        if (!selected)
            cr_current = non_selected_colour;
        else {
            if (bar_index == std::wstring_view::npos) {
                cr_current = 0xffffff - non_selected_colour;
            } else {
                const auto selected_colour_hex = colour_code.substr(0, bar_index);
                cr_current = mmh::strtoul_n(
                    selected_colour_hex.data(), std::min(size_t{6}, selected_colour_hex.length()), 0x10);
            }
        }
    }

    std::wstring_view render_text = enable_colour_codes ? text_without_colours : utf16_text_view;

    if (align == ALIGN_CENTRE)
        text_format.set_alignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    else if (align == ALIGN_RIGHT)
        text_format.set_alignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    else
        text_format.set_alignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    try {
        const auto scaling_factor = TextLayout::s_default_scaling_factor();

        const auto layout = text_format.create_text_layout(render_text,
            gsl::narrow_cast<float>(wil::rect_width(rect)) / scaling_factor,
            gsl::narrow_cast<float>(wil::rect_height(rect)) / scaling_factor);

        size_t position{};

        for (auto& [colour, character_count] : segments) {
            layout.set_colour(colour, {gsl::narrow<uint32_t>(position), gsl::narrow<uint32_t>(character_count)});
            position += character_count;
        }

        const auto metrics = layout.get_metrics();

        layout.render(dc, rect, default_color);

        return gsl::narrow_cast<int>(metrics.width * scaling_factor + 1);
    }
    CATCH_LOG()

    return 0;
}

} // namespace

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
