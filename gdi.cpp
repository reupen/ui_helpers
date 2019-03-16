#include "stdafx.h"

namespace uih {

void draw_rect_outline(HDC dc, const RECT& rc, COLORREF colour, int width)
{
    gdi_object_t<HPEN>::ptr_t pen(CreatePen(PS_SOLID | PS_INSIDEFRAME, width, colour));
    HBRUSH old_brush = SelectBrush(dc, GetStockObject(NULL_BRUSH));
    HPEN old_pen = SelectPen(dc, pen.get());
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectBrush(dc, old_brush);
    SelectPen(dc, pen.get());
}

} // namespace uih
