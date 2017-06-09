#pragma once

namespace uih {
    void DrawDragImageBackground(HWND wnd, bool isThemed, HTHEME theme, HDC dc, COLORREF selectionBackgroundColour, const RECT & rc);
    void DrawDragImageLabel(HWND wnd, bool isThemed, HTHEME theme, HDC dc, const RECT & rc, COLORREF selectionTextColour, const char * text);
    void DrawDragImageIcon(HDC dc, const RECT & rc, HICON icon);
    BOOL CreateDragImage(HWND wnd, bool isThemed, HTHEME theme, COLORREF selectionBackgroundColour, COLORREF selectionTextColour, 
        HICON icon, const LPLOGFONT font, bool showText, const char * text, LPSHDRAGIMAGE lpsdi);
}