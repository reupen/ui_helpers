#include "../stdafx.h"
#include "../../foobar2000/SDK/foobar2000.h"

namespace uih {

void ListView::ensure_visible(t_size index, EnsureVisibleMode mode)
{
    if (index > m_items.get_count())
        return;

    const auto item_visibility = get_item_visibility(index);

    const auto item_area_height = get_item_area_height();
    const auto item_height = get_item_height(index);
    const auto item_start_position = get_item_position(index);
    const auto item_end_position = get_item_position_bottom(index);

    switch (mode) {
    case EnsureVisibleMode::PreferCentringItem:
        switch (item_visibility) {
        case ItemVisibility::ObscuredAbove:
            scroll(item_start_position);
            break;
        case ItemVisibility::ObscuredBelow:
            scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        default:
            scroll(item_start_position - (item_area_height + item_height) / 2);
        }
        break;
    case EnsureVisibleMode::PreferMinimalScrolling:
        switch (item_visibility) {
        case ItemVisibility::ObscuredAbove:
        case ItemVisibility::AboveViewport:
            scroll(item_start_position);
            break;
        case ItemVisibility::ObscuredBelow:
        case ItemVisibility::BelowViewport:
            scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        }
    }
}

void ListView::scroll(int position, bool b_horizontal, bool update_display)
{
    const INT scroll_bar_type = b_horizontal ? SB_HORZ : SB_VERT;
    int& scroll_position = b_horizontal ? m_horizontal_scroll_position : m_scroll_position;
    const int original_scroll_position = scroll_position;

    SCROLLINFO scroll_info{};
    scroll_info.cbSize = sizeof(SCROLLINFO);
    scroll_info.fMask = SIF_POS;
    scroll_info.nPos = position;

    if (scroll_position == scroll_info.nPos)
        return;

    destroy_tooltip();
    scroll_position = SetScrollInfo(get_wnd(), scroll_bar_type, &scroll_info, true);

    RECT playlist{};
    get_items_rect(&playlist);
    int dx = 0;
    int dy = 0;
    (b_horizontal ? dx : dy) = original_scroll_position - scroll_position;

    if (update_display) {
        ScrollWindowEx(get_wnd(), dx, dy, &playlist, &playlist, nullptr, nullptr, SW_INVALIDATE);
        RedrawWindow(get_wnd(), nullptr, nullptr, RDW_UPDATENOW);
    }

    if (b_horizontal)
        reposition_header();
}

void ListView::scroll_from_scroll_bar(short scroll_bar_command, bool b_horizontal)
{
    const int scroll_bar_type = b_horizontal ? SB_HORZ : SB_VERT;
    int& scroll_position = b_horizontal ? m_horizontal_scroll_position : m_scroll_position;

    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(get_wnd(), scroll_bar_type, &si);

    int pos{};
    if (scroll_bar_command == SB_LINEDOWN)
        pos = (std::min)(scroll_position + m_item_height, si.nMax);
    else if (scroll_bar_command == SB_LINEUP)
        pos = (std::max)(scroll_position - m_item_height, si.nMin);
    else if (scroll_bar_command == SB_PAGEUP)
        pos = scroll_position - si.nPage;
    else if (scroll_bar_command == SB_PAGEDOWN)
        pos = scroll_position + si.nPage;
    else if (scroll_bar_command == SB_THUMBTRACK)
        pos = si.nTrackPos;
    else if (scroll_bar_command == SB_THUMBPOSITION)
        pos = si.nTrackPos;
    else if (scroll_bar_command == SB_BOTTOM)
        pos = si.nMax;
    else if (scroll_bar_command == SB_TOP)
        pos = si.nMin;
    else // SB_ENDSCROLL
        return;

    scroll(pos, b_horizontal);
}

void ListView::_update_scroll_info_vertical()
{
    RECT rc;
    get_items_rect(&rc);

    t_size old_scroll_position = m_scroll_position;
    SCROLLINFO scroll;
    memset(&scroll, 0, sizeof(SCROLLINFO));
    scroll.cbSize = sizeof(SCROLLINFO);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;
    t_size count = m_items.get_count();
    scroll.nMax = count ? get_item_group_bottom(count - 1) : 0;
    scroll.nPage = RECT_CY(rc);
    scroll.nPos = m_scroll_position;
    bool b_old_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_VSCROLL) != 0;
    ;
    m_scroll_position = SetScrollInfo(get_wnd(), SB_VERT, &scroll, true);
    GetScrollInfo(get_wnd(), SB_VERT, &scroll);
    bool b_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_VSCROLL) != 0; // scroll.nPage < (UINT)scroll.nMax;
    // if (b_old_show != b_show)
    // BOOL ret = ShowScrollBar(get_wnd(), SB_VERT, b_show);
    if (m_scroll_position != old_scroll_position /* || b_old_show != b_show*/)
        invalidate_all(false);
}

void ListView::_update_scroll_info_horizontal()
{
    RECT rc;
    get_items_rect(&rc);

    t_size old_scroll_position = m_horizontal_scroll_position;
    t_size cx = get_columns_display_width() + get_total_indentation();

    SCROLLINFO scroll;
    memset(&scroll, 0, sizeof(SCROLLINFO));
    scroll.cbSize = sizeof(SCROLLINFO);
    scroll.fMask = SIF_RANGE | SIF_PAGE;
    scroll.nMin = 0;
    scroll.nMax = m_autosize ? 0 : (cx ? cx - 1 : 0);
    scroll.nPage = m_autosize ? 0 : RECT_CX(rc);
    bool b_old_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_HSCROLL) != 0;
    ;
    m_horizontal_scroll_position = SetScrollInfo(get_wnd(), SB_HORZ, &scroll, true);
    GetScrollInfo(get_wnd(), SB_HORZ, &scroll);
    bool b_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_HSCROLL) != 0; // scroll.nPage < (UINT)scroll.nMax;
    // if (b_old_show != b_show)
    {
        // RECT rc1, rc2;
        // GetClientRect(get_wnd(), &rc1);
        // BOOL ret = ShowScrollBar(get_wnd(), SB_HORZ, b_show);
        // GetClientRect(get_wnd(), &rc2);
    }
    if (m_horizontal_scroll_position != old_scroll_position /* || b_old_show != b_show*/)
        invalidate_all(false);
    if (b_old_show != b_show) {
        get_items_rect(&rc);
        memset(&scroll, 0, sizeof(SCROLLINFO));
        scroll.cbSize = sizeof(SCROLLINFO);
        scroll.fMask = SIF_PAGE;
        scroll.nPage = RECT_CY(rc);
        m_scroll_position = SetScrollInfo(get_wnd(), SB_VERT, &scroll, true);
    }
}

void ListView::update_scroll_info(bool b_update, bool b_vertical, bool b_horizontal)
{
    // god this is a bit complicated when showing h scrollbar causes need for v scrollbar (and vv)

    // bool b_scroll_shown = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_VSCROLL) != 0;
    if (b_vertical) {
        _update_scroll_info_vertical();
    }
    if (b_horizontal) {
        _update_scroll_info_horizontal();
    }
    if (b_update)
        UpdateWindow(get_wnd());
}
void ListView::create_timer_scroll_up()
{
    if (!m_timer_scroll_up) {
        SetTimer(get_wnd(), TIMER_SCROLL_UP, 10, nullptr);
        m_timer_scroll_up = true;
    }
}
void ListView::create_timer_scroll_down()
{
    if (!m_timer_scroll_down) {
        SetTimer(get_wnd(), TIMER_SCROLL_DOWN, 10, nullptr);
        m_timer_scroll_down = true;
    }
}
void ListView::destroy_timer_scroll_up()
{
    if (m_timer_scroll_up) {
        KillTimer(get_wnd(), TIMER_SCROLL_UP);
        m_timer_scroll_up = false;
    }
}
void ListView::destroy_timer_scroll_down()
{
    if (m_timer_scroll_down) {
        KillTimer(get_wnd(), TIMER_SCROLL_DOWN);
        m_timer_scroll_down = false;
    }
}
} // namespace uih
