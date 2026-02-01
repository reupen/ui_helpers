#include "stdafx.h"

#include "direct_write.h"
#include "direct_write_text_out.h"

#include "direct_write_style_utils.h"
#include "text_style.h"

using namespace std::string_view_literals;

namespace uih::direct_write {

namespace {

int measure_text_width_styles(
    const TextFormat& text_format, std::wstring_view text, const text_style::FormatProperties& default_format)
{
    const auto text_without_newlines = text_style::remove_newlines(text);
    const auto text_without_newlines_view = text_without_newlines ? std::wstring_view(*text_without_newlines) : text;
    const auto [processed_text, _, font_segments]
        = text_style::process_colour_and_font_codes(text_without_newlines_view, default_format);

    try {
        auto text_layout = text_format.create_text_layout(processed_text, 65536.f, 65536.f, false);
        set_layout_font_segments(text_layout, font_segments);
        const auto metrics = text_layout.get_metrics();
        return gsl::narrow_cast<int>(metrics.widthIncludingTrailingWhitespace * get_default_scaling_factor() + 1);
    }
    CATCH_LOG()

    return 0;
}

std::shared_ptr<TextLayout> create_cached_text_layout_styles(const TextFormat& text_format, std::wstring_view text,
    const RECT& rect, const text_style::FormatProperties& initial_format, alignment align, bool enable_colour_codes,
    bool enable_ellipsis)
{
    std::optional<std::wstring> processed_text;
    std::vector<text_style::ColourSegment> colour_segments;
    std::vector<text_style::FontSegment> font_segments;

    const auto scaling_factor = get_default_scaling_factor();
    const auto max_width = gsl::narrow_cast<float>(wil::rect_width(rect)) / scaling_factor;
    const auto max_height = gsl::narrow_cast<float>(wil::rect_height(rect)) / scaling_factor;
    const auto dwrite_alignment = get_text_alignment(align);
    const auto serialised_initial_format = initial_format.serialise();

    auto layout = text_format.get_cached_text_layout(
        text, max_width, max_height, enable_ellipsis, dwrite_alignment, serialised_initial_format);

    if (!layout) {
        const auto text_without_newlines = text_style::remove_newlines(text);
        const auto text_without_newlines_view
            = text_without_newlines ? std::wstring_view(*text_without_newlines) : text;

        if (enable_colour_codes)
            std::tie(processed_text, colour_segments, font_segments)
                = text_style::process_colour_and_font_codes(text_without_newlines_view, initial_format);

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
                render_text, text, max_width, max_height, enable_ellipsis, dwrite_alignment, serialised_initial_format);

            for (auto& [colour, selected_colour, start_character, character_count] : colour_segments) {
                layout->set_colour(colour, selected_colour,
                    {gsl::narrow<uint32_t>(start_character), gsl::narrow<uint32_t>(character_count)});
            }

            set_layout_font_segments(*layout, font_segments);
        }

        return layout;
    }
    CATCH_LOG()

    return {};
}

int is_text_trimmed_styles(const TextFormat& text_format, std::wstring_view text, const RECT& rect,
    const text_style::FormatProperties& initial_format, alignment align, bool enable_colour_codes, bool enable_ellipsis)
{
    if (const auto text_layout = create_cached_text_layout_styles(
            text_format, text, rect, initial_format, align, enable_colour_codes, enable_ellipsis))
        return text_layout->is_trimmed();

    return false;
}

int text_out_styles(const TextFormat& text_format, HWND wnd, HDC dc, std::wstring_view text, const RECT& rect,
    bool selected, DWORD default_color, const text_style::FormatProperties& initial_format, alignment align,
    bool enable_colour_codes, bool enable_ellipsis, wil::com_ptr<IDWriteBitmapRenderTarget> bitmap_render_target)
{
    if (is_rect_null_or_reversed(&rect) || rect.right <= rect.left)
        return 0;

    if (const auto text_layout = create_cached_text_layout_styles(
            text_format, text, rect, initial_format, align, enable_colour_codes, enable_ellipsis)) {
        try {
            const auto metrics = text_layout->get_metrics();

            text_layout->render_with_transparent_background(
                wnd, dc, rect, default_color, selected, 0.0f, bitmap_render_target);

            const auto scaling_factor = get_default_scaling_factor();

            return gsl::narrow_cast<int>(metrics.width * scaling_factor + 1);
        }
        CATCH_LOG()
    }

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

int measure_text_width_columns_and_styles(const TextFormat& text_format, std::wstring_view text, int x_offset,
    int border, const text_style::FormatProperties& initial_format)
{
    if (text.empty())
        return 0;

    size_t segment_start{};
    int total_width{x_offset};

    while (segment_start != std::wstring_view::npos) {
        const auto segment_end = text.find_first_of('\t', segment_start);
        const auto segment = text.substr(segment_start,
            segment_end != std::wstring_view::npos ? segment_end - segment_start : std::wstring_view::npos);

        if (!segment.empty()) {
            total_width += measure_text_width_styles(text_format, segment, initial_format);
            total_width += 2 * border;
        }

        if (segment_end == std::wstring_view::npos)
            break;

        segment_start = text.find_first_not_of('\t', segment_end);
    }
    return total_width;
}

int is_text_trimmed_columns_and_styles(const TextFormat& text_format, std::wstring_view text, int x_offset, int border,
    const RECT& rect, const TextOutOptions& options)
{
    if (options.enable_tab_columns && text.find_first_of('\t') != std::wstring_view::npos) {
        return measure_text_width_columns_and_styles(text_format, text, x_offset, border, options.initial_format)
            > wil::rect_width(rect);
    }

    RECT adjusted_rect{rect};
    adjusted_rect.left += border + x_offset;
    adjusted_rect.right -= border;

    return is_text_trimmed_styles(text_format, text, adjusted_rect, options.initial_format, options.align,
        options.enable_style_codes, options.enable_ellipses);
}

int text_out_columns_and_styles(const TextFormat& text_format, HWND wnd, HDC dc, std::wstring_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, const TextOutOptions& options)
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

        return text_out_styles(text_format, wnd, dc, text, adjusted_rect, options.is_selected, default_colour,
            options.initial_format, options.align, options.enable_style_codes, options.enable_ellipses,
            options.bitmap_render_target);
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

            const int cell_render_width = text_out_styles(text_format, wnd, dc, cell_text, cell_rect,
                options.is_selected, default_colour, options.initial_format, cell_index == 0 ? ALIGN_RIGHT : ALIGN_LEFT,
                options.enable_style_codes, false, options.bitmap_render_target);

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

int text_out_columns_and_styles(TextFormat& text_format, HWND wnd, HDC dc, std::string_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, const TextOutOptions& options)
{
    return text_out_columns_and_styles(
        text_format, wnd, dc, mmh::to_utf16(text), x_offset, border, rect, default_colour, options);
}

} // namespace uih::direct_write
