#pragma once

namespace uih {

enum alignment {
    ALIGN_LEFT,
    ALIGN_CENTRE,
    ALIGN_RIGHT,
};

bool is_rect_null_or_reversed(const RECT* r);
void get_text_size(HDC dc, const char* src, int len, SIZE& sz);
int get_text_width(HDC dc, const char* src, int len);
int get_text_width_colour(HDC dc, const char* src, int len, bool b_ignore_tabs = false);
BOOL text_out_colours_ellipsis(HDC dc, const char* src, size_t len, int x_offset, int pos_y, const RECT* base_clip,
    bool selected, bool show_ellipsis, DWORD default_color, alignment align, unsigned* p_width = nullptr,
    bool b_set_default_colours = true, int* p_position = nullptr, bool enable_tabs = false, int tab_origin = 0);
BOOL text_out_colours_tab(HDC dc, const char* display, size_t display_len, int left_offset, int border,
    const RECT* base_clip, bool selected, DWORD default_color, bool enable_tab_columns, bool show_ellipsis,
    alignment align, unsigned* p_width = nullptr, bool b_set_default_colours = true,
    bool b_vertical_align_centre = true, int* p_position = nullptr, int tab_origin = 0);

void remove_color_marks(const char* src, pfc::string_base& out, size_t len = pfc_infinite);
} // namespace uih
