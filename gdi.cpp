#include "stdafx.h"

namespace uih {

void paint_subclassed_window_with_buffering(HWND wnd, WNDPROC window_proc)
{
    PAINTSTRUCT ps{};
    const auto paint_dc = wil::BeginPaint(wnd, &ps);
    const auto buffered_dc = BufferedDC(paint_dc.get(), ps.rcPaint);

    const auto print_client_flags = PRF_CLIENT | (ps.fErase ? PRF_ERASEBKGND : 0);
    CallWindowProc(window_proc, wnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(buffered_dc.get()), print_client_flags);
}

void draw_rect_outline(HDC dc, const RECT& rc, COLORREF colour, int width)
{
    wil::unique_hpen pen(CreatePen(PS_SOLID | PS_INSIDEFRAME, width, colour));
    HBRUSH old_brush = SelectBrush(dc, GetStockObject(NULL_BRUSH));
    HPEN old_pen = SelectPen(dc, pen.get());
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectBrush(dc, old_brush);
    SelectPen(dc, pen.get());
}

} // namespace uih
