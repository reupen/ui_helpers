#include "stdafx.h"

namespace uih {

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

} // namespace uih
