#include "stdafx.h"

using namespace std::string_view_literals;

namespace uih {

namespace {
constexpr auto ellipsis = "…"sv;
}

bool check_colour_marks(const char* src, unsigned int len = -1)
{
    const char* ptr = src;
    while (*ptr && (unsigned)(ptr - src) < len) {
        if (*ptr == 3) {
            return true;
        }
        ptr++;
    }
    return false;
}

void remove_color_marks(const char* src, pfc::string_base& out, size_t len)
{
    out.reset();
    const char* ptr = src;
    while (*src && static_cast<size_t>(src - ptr) < len) {
        if (*src == 3) {
            src++;
            while (*src && *src != 3)
                src++;
            if (*src == 3)
                src++;
        } else
            out.add_byte(*src++);
    }
}

bool is_rect_null_or_reversed(const RECT* r)
{
    return r->right <= r->left || r->bottom <= r->top;
}

void get_text_size(HDC dc, const char* src, int len, SIZE& sz)
{
    sz.cx = 0;
    sz.cy = 0;
    if (!dc)
        return;
    if (len <= 0)
        return;

    pfc::stringcvt::string_wide_from_utf8 wstr(src, gsl::narrow<size_t>(len));
    size_t wlen = wstr.length();

    UniscribeTextRenderer p_ScriptString(dc, wstr, gsl::narrow<int>(wlen), NULL, false);
    p_ScriptString.get_output_size(sz);
}

int get_text_width(HDC dc, const char* src, int len)
{
    SIZE sz;
    get_text_size(dc, src, len, sz);
    return sz.cx;
}

int get_text_width_colour(HDC dc, const char* src, int len, bool b_ignore_tabs)
{
    if (!dc)
        return 0;
    int ptr = 0;
    int start = 0;
    int rv = 0;

    while (ptr < len) {
        if (src[ptr] == 3) {
            rv += get_text_width(dc, src + start, ptr - start);
            ptr++;
            while (ptr < len && src[ptr] != 3)
                ptr++;
            if (ptr < len)
                ptr++;
            start = ptr;
        } else if (b_ignore_tabs && src[ptr] == '\t') {
            rv += get_text_width(dc, src + start, ptr - start);
            while (ptr < len && src[ptr] == '\t')
                ptr++;
        } else
            ptr++;
    }
    rv += get_text_width(dc, src + start, ptr - start);
    return rv;
}

BOOL text_out_colours_ellipsis(HDC dc, const char* src_c, size_t src_c_len, int x_offset, int pos_y,
    const RECT* base_clip, bool selected, bool show_ellipsis, DWORD default_color, alignment align, unsigned* p_width,
    bool b_set_default_colours, int* p_position, bool enable_tabs, int tab_origin)
{
    struct ColouredTextSegment {
        std::wstring text;
        COLORREF colour{};
        int left_offset{};
        UniscribeTextRenderer renderer{};
        std::vector<int> character_right_offsets{};

        void analyse(HDC dc, bool enable_tabs, int tab_origin)
        {
            renderer.analyse(
                dc, text.data(), gsl::narrow<int>(text.size()), -1, false, enable_tabs, tab_origin + left_offset);
            character_right_offsets.resize(text.length());
            renderer.get_character_logical_extents(character_right_offsets.data(), left_offset);
        }
    };

    pfc::stringcvt::string_wide_from_utf8 wstr(src_c, src_c_len);

    if (!base_clip)
        return FALSE;

    if (base_clip) {
        if (is_rect_null_or_reversed(base_clip) || base_clip->right <= base_clip->left + x_offset
            || base_clip->bottom <= pos_y)
            return TRUE;
    }

    std::vector<ColouredTextSegment> segments;
    std::optional<COLORREF> cr_last;

    {
        const wchar_t* src = wstr.get_ptr();
        size_t wLen = wstr.length();

        const wchar_t* p_start = src;
        const wchar_t* p_block_start = src;
        const wchar_t* p_current = src;

        COLORREF cr_current = b_set_default_colours ? default_color : GetTextColor(dc);

        while (size_t(p_current - p_start) < wLen) {
            p_block_start = p_current;
            while (size_t(p_current - p_start) < wLen && p_current[0] != 3) {
                p_current++;
            }
            if (p_current > p_block_start) {
                ColouredTextSegment segment{
                    {p_block_start, gsl::narrow<size_t>(p_current - p_block_start)}, cr_current};
                segments.emplace_back(std::move(segment));

                cr_last.reset();
            }

            // need another at least char for valid colour code operation
            if (size_t(p_current + 1 - p_start) >= wLen)
                break;

            if (p_current[0] == 3) {
                COLORREF new_colour;
                COLORREF new_colour_selected;

                bool have_selected = false;

                if (p_current[1] == 3) {
                    new_colour = new_colour_selected = default_color;
                    have_selected = true;
                    p_current += 2;
                } else {
                    p_current++;
                    new_colour = mmh::strtoul_n(p_current, std::min(size_t{6}, wLen - (p_current - p_start)), 0x10);
                    while (size_t(p_current - p_start) < wLen && p_current[0] != 3 && p_current[0] != '|') {
                        p_current++;
                    }
                    if (size_t(p_current - p_start) < wLen && p_current[0] == '|') {
                        if (size_t(p_current + 1 - p_start) >= wLen)
                            // unexpected eos, malformed string
                            break;

                        p_current++;
                        new_colour_selected
                            = mmh::strtoul_n(p_current, std::min(size_t{6}, wLen - (p_current - p_start)), 0x10);
                        have_selected = true;

                        while (size_t(p_current - p_start) < wLen && p_current[0] != 3)
                            p_current++;
                    }
                    p_current++;
                }
                if (selected)
                    new_colour = have_selected ? new_colour_selected : 0xFFFFFF - new_colour;

                cr_last = (cr_current = new_colour);
            }
        }
    }

    RECT clip = *base_clip;

    int available_width = clip.right - clip.left - x_offset;

    int cumulative_width{};
    for (auto iter = segments.begin(); iter != segments.end(); ++iter) {
        auto& segment = *iter;

        segment.left_offset = cumulative_width;
        segment.analyse(dc, enable_tabs, tab_origin);
        cumulative_width += segment.renderer.get_output_width();

        if (cumulative_width > available_width) {
            segments.resize(std::distance(segments.begin(), iter + 1));
            break;
        }
    }

    int position = clip.left;

    if (show_ellipsis && cumulative_width > available_width) {
        const auto ellipsis_width = get_text_width(dc, ellipsis.data(), gsl::narrow_cast<int>(ellipsis.size())) + 2;

        if (ellipsis_width <= clip.right - clip.left - x_offset) {
            for (auto segment_iter = segments.rbegin(); segment_iter != segments.rend(); ++segment_iter) {
                auto& segment = *segment_iter;

                const auto truncated = [&] {
                    for (auto text_iter = segment.text.rbegin(); text_iter != segment.text.rend(); ++text_iter) {
                        const auto index = std::distance(text_iter, segment.text.rend()) - 1;

                        if (segment.character_right_offsets[index] + ellipsis_width > available_width)
                            continue;

                        segment.text.resize(index + 1);
                        segment.text.push_back(0x2026);
                        segment.analyse(dc, enable_tabs, tab_origin);

                        segments.resize(std::distance(segment_iter, segments.rend()));
                        cumulative_width = std::accumulate(segments.begin(), segments.end(), 0,
                            [](int value, auto&& segment) { return value + segment.renderer.get_output_width(); });

                        return true;
                    }
                    return false;
                }();

                if (truncated)
                    break;
            }
        }
    }

    if (align == ALIGN_CENTRE) {
        position += (clip.right - clip.left - cumulative_width) / 2;
    } else if (align == ALIGN_RIGHT) {
        position += (clip.right - clip.left - cumulative_width - x_offset);
    } else {
        position += x_offset;
    }

    if (p_width)
        *p_width = NULL;

    COLORREF cr_old = GetTextColor(dc);
    SetTextAlign(dc, TA_LEFT);
    SetBkMode(dc, TRANSPARENT);

    for (auto&& segment : segments) {
        SetTextColor(dc, segment.colour);
        segment.renderer.text_out(position, pos_y, ETO_CLIPPED, &clip);
        int thisWidth = segment.renderer.get_output_width();
        position += thisWidth;
        if (p_width)
            *p_width += thisWidth;
    }

    if (p_position)
        *p_position = position;

    if (b_set_default_colours)
        SetTextColor(dc, cr_old);
    else if (cr_last)
        SetTextColor(dc, *cr_last);

    return TRUE;
}

BOOL text_out_colours_tab(HDC dc, const char* display, size_t display_len, int left_offset, int border,
    const RECT* base_clip, bool selected, DWORD default_color, bool enable_tab_columns, bool show_ellipsis,
    alignment align, unsigned* p_width, bool b_set_default_colours, bool b_vertical_align_centre, int* p_position,
    int tab_origin)
{
    if (!base_clip)
        return FALSE;

    RECT clip = *base_clip;

    if (is_rect_null_or_reversed(&clip))
        return TRUE;

    int extra = (clip.bottom - clip.top - uih::get_dc_font_height(dc));

    int pos_y = clip.top + (b_vertical_align_centre ? (extra / 2) : extra);

    int n_tabs = 0;

    display_len = pfc::strlen_max(display, display_len);

    if (enable_tab_columns) {
        for (size_t n = 0; n < display_len; n++) {
            if (display[n] == '\t') {
                n_tabs++;
            }
        }
    }

    if (n_tabs == 0) {
        clip.left += border;
        clip.right -= border;
        return text_out_colours_ellipsis(dc, display, display_len, left_offset, pos_y, &clip, selected, show_ellipsis,
            default_color, align, p_width, b_set_default_colours, p_position, !enable_tab_columns, tab_origin);
    }

    if (p_width)
        *p_width = clip.right;
    if (p_position)
        *p_position = clip.right;

    clip.left += left_offset;
    int tab_total = clip.right - clip.left;

    auto ptr = display_len;
    int tab_ptr = 0;
    RECT t_clip = clip;

    do {
        auto ptr_end = ptr;
        while (ptr > 0 && display[ptr - 1] != '\t')
            ptr--;
        const char* t_string = display + ptr;
        const auto t_length = ptr_end - ptr;
        if (t_length > 0) {
            t_clip.right -= border;

            if (tab_ptr)
                t_clip.left = clip.right - MulDiv(tab_ptr, tab_total, n_tabs) + border;

            unsigned p_written = NULL;
            text_out_colours_ellipsis(dc, t_string, t_length, 0, pos_y, &t_clip, selected, show_ellipsis, default_color,
                tab_ptr ? ALIGN_LEFT : ALIGN_RIGHT, &p_written, b_set_default_colours);

            if (tab_ptr == 0)
                t_clip.left = t_clip.right - p_written;

            t_clip.right = t_clip.left - border;
        }

        if (ptr > 0) {
            ptr--; // tab char
            tab_ptr++;
        }
    } while (ptr > 0);

    return TRUE;
}
} // namespace uih
