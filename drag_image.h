#pragma once

namespace uih {
	void DrawDragImageBackground(HWND wnd, HTHEME theme, HDC dc, const RECT & rc, const t_list_view::colour_data_t & colourData);
	void DrawDragImageLabel(HWND wnd, HTHEME theme, HDC dc, const RECT & rc, const t_list_view::colour_data_t & colourData, const char * text);
	void DrawDragImageIcon(HDC dc, const RECT & rc, HICON icon);
}