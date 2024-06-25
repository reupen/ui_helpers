#pragma once

namespace uih::direct_write {

int text_out_columns_and_colours(const TextFormat& text_format, HDC dc, std::string_view text, int x_offset, int border,
    const RECT& rect, bool selected, COLORREF default_colour, bool enable_colour_codes, bool enable_tab_columns,
    alignment align = uih::ALIGN_LEFT, bool enable_ellipses = true);

} // namespace uih::direct_write
