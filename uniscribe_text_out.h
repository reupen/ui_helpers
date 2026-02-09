#pragma once

namespace uih {

enum alignment {
    ALIGN_LEFT,
    ALIGN_CENTRE,
    ALIGN_RIGHT,
};

int get_text_width(HDC dc, const char* src, int len);

} // namespace uih
