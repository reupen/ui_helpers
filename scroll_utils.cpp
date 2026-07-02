#include "stdafx.h"

#include "scroll_utils.h"

namespace uih {

int set_scroll_position(HWND wnd, ScrollAxis axis, int old_position, int new_position)
{
    if (old_position == new_position)
        return old_position;

    const auto obj_id = axis == ScrollAxis::Vertical ? OBJID_VSCROLL : OBJID_HSCROLL;

    SCROLLBARINFO old_sbi{};
    old_sbi.cbSize = sizeof(old_sbi);
    GetScrollBarInfo(wnd, obj_id, &old_sbi);

    SCROLLINFO position_si{};
    position_si.cbSize = sizeof(SCROLLINFO);
    position_si.fMask = SIF_POS;
    position_si.nPos = new_position;

    const auto actual_new_position = SetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &position_si, FALSE);

    if (actual_new_position == old_position)
        return old_position;

    SCROLLBARINFO new_sbi{};
    new_sbi.cbSize = sizeof(new_sbi);
    GetScrollBarInfo(wnd, obj_id, &new_sbi);

    if (old_sbi.xyThumbTop != new_sbi.xyThumbTop) {
        SCROLLINFO redraw_si{};
        redraw_si.cbSize = sizeof(SCROLLINFO);
        SetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &redraw_si, TRUE);
    }

    return actual_new_position;
}

int clamp_scroll_position(HWND wnd, ScrollAxis axis, int position)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE;
    GetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &si);

    if (position < si.nMin)
        return si.nMin;

    const auto page_size = gsl::narrow_cast<int>(si.nPage);
    const auto adjusted_max = si.nMax - page_size + (page_size > 0 ? 1 : 0);

    if (position > adjusted_max)
        return adjusted_max;

    return position;
}

bool is_scrollable(HWND wnd, ScrollAxis axis)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE;
    GetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &si);

    return si.nMax > si.nMin && std::cmp_greater_equal(si.nMax - si.nMin, si.nPage);
}

} // namespace uih
