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
void remove_color_marks(const char* src, pfc::string_base& out, size_t len = pfc_infinite);

} // namespace uih
