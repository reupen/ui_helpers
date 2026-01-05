#pragma once

namespace uih::direct_write {

DWRITE_TEXT_ALIGNMENT get_text_alignment(alignment alignment_);

struct TextOutOptions {
    wil::com_ptr<IDWriteBitmapRenderTarget> bitmap_render_target;
    bool is_selected{};
    alignment align{ALIGN_LEFT};
    bool enable_ellipses{true};
    bool enable_style_codes{true};
    bool enable_tab_columns{true};
};

int text_out_columns_and_styles(TextFormat& text_format, HWND wnd, HDC dc, std::wstring_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, TextOutOptions options = {});

int text_out_columns_and_styles(TextFormat& text_format, HWND wnd, HDC dc, std::string_view text, int x_offset,
    int border, const RECT& rect, COLORREF default_colour, TextOutOptions options = {});

int measure_text_width_columns_and_styles(
    const TextFormat& text_format, std::wstring_view text, int x_offset, int border);

} // namespace uih::direct_write
