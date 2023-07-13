#include "stdafx.h"

namespace uih {

void paint_subclassed_window_with_buffering(HWND wnd, WNDPROC window_proc)
{
    PAINTSTRUCT ps{};
    const auto paint_dc = wil::BeginPaint(wnd, &ps);
    const auto buffered_dc = BufferedDC(paint_dc.get(), ps.rcPaint);

    CallWindowProc(window_proc, wnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(buffered_dc.get()), 0);
    CallWindowProc(window_proc, wnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(buffered_dc.get()), PRF_CLIENT);
}

void draw_rect_outline(HDC dc, const RECT& rc, COLORREF colour, int width)
{
    const wil::unique_hpen pen(CreatePen(PS_SOLID | PS_INSIDEFRAME, width, colour));
    auto _selected_brush = wil::SelectObject(dc, GetStockObject(NULL_BRUSH));
    auto _selected_pen = wil::SelectObject(dc, pen.get());
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
}

} // namespace uih
