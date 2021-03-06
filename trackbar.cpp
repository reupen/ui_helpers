#include "stdafx.h"

/**
 * \file trackbar.cpp
 * trackbar custom control
 * Copyright (C) 2005
 * \author musicmusic
 */

namespace uih {

BOOL TrackbarBase::create_tooltip(const TCHAR* text, POINT pt)
{
    destroy_tooltip();

    DLLVERSIONINFO2 dvi;
    bool b_comctl_6 = SUCCEEDED(uih::get_comctl32_version(dvi)) && dvi.info1.dwMajorVersion >= 6;

    m_wnd_tooltip = CreateWindowEx(WS_EX_TOPMOST | (b_comctl_6 ? WS_EX_TRANSPARENT : 0), TOOLTIPS_CLASS, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, get_wnd(),
        nullptr, mmh::get_current_instance(), nullptr);

    SetWindowPos(m_wnd_tooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    TOOLINFO ti;

    memset(&ti, 0, sizeof(ti));

    ti.cbSize = TTTOOLINFO_V1_SIZE;
    ti.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT | TTF_TRACK | TTF_ABSOLUTE;
    ti.hwnd = get_wnd();
    ti.hinst = mmh::get_current_instance();
    ti.lpszText = const_cast<TCHAR*>(text);

    uih::tooltip_add_tool(m_wnd_tooltip, &ti);

    SendMessage(m_wnd_tooltip, TTM_TRACKPOSITION, 0, MAKELONG(pt.x, pt.y + 21));
    SendMessage(m_wnd_tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);

    return TRUE;
}

void TrackbarBase::destroy_tooltip()
{
    if (m_wnd_tooltip) {
        DestroyWindow(m_wnd_tooltip);
        m_wnd_tooltip = nullptr;
    }
}

BOOL TrackbarBase::update_tooltip(POINT pt, const TCHAR* text)
{
    if (!m_wnd_tooltip)
        return FALSE;

    SendMessage(m_wnd_tooltip, TTM_TRACKPOSITION, 0, MAKELONG(pt.x, pt.y + 21));

    TOOLINFO ti;
    memset(&ti, 0, sizeof(ti));

    ti.cbSize = TTTOOLINFO_V1_SIZE;
    ti.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT | TTF_TRACK | TTF_ABSOLUTE;
    ti.hwnd = get_wnd();
    ti.hinst = mmh::get_current_instance();
    ti.lpszText = const_cast<TCHAR*>(text);

    uih::tooltip_update_tip_text(m_wnd_tooltip, &ti);

    return TRUE;
}

void TrackbarBase::set_callback(TrackbarCallback* p_host)
{
    m_host = p_host;
}

void TrackbarBase::set_show_tooltips(bool val)
{
    m_show_tooltips = val;
    if (!val)
        destroy_tooltip();
}

void TrackbarBase::set_position_internal(unsigned pos)
{
    if (!m_dragging) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(get_wnd(), &pt);
        update_hot_status(pt);
    }

    RECT rc;
    get_thumb_rect(&rc);
    HRGN rgn_old = CreateRectRgnIndirect(&rc);
    get_thumb_rect(pos, m_range, &rc);
    HRGN rgn_new = CreateRectRgnIndirect(&rc);
    CombineRgn(rgn_new, rgn_old, rgn_new, RGN_OR);
    DeleteObject(rgn_old);
    m_display_position = pos;
    // InvalidateRgn(m_wnd, rgn_new, TRUE);
    RedrawWindow(get_wnd(), nullptr, rgn_new, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ERASENOW);
    DeleteObject(rgn_new);
}

void TrackbarBase::set_position(unsigned pos)
{
    m_position = pos;
    if (!m_dragging) {
        set_position_internal(pos);
    }
}
unsigned TrackbarBase::get_position() const
{
    return m_position;
}

void TrackbarBase::set_range(unsigned range)
{
    RECT rc;
    get_thumb_rect(&rc);
    HRGN rgn_old = CreateRectRgnIndirect(&rc);
    get_thumb_rect(m_display_position, range, &rc);
    HRGN rgn_new = CreateRectRgnIndirect(&rc);
    CombineRgn(rgn_new, rgn_old, rgn_new, RGN_OR);
    DeleteObject(rgn_old);
    m_range = range;
    RedrawWindow(get_wnd(), nullptr, rgn_new, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ERASENOW);
    DeleteObject(rgn_new);
}
unsigned TrackbarBase::get_range() const
{
    return m_range;
}

void TrackbarBase::set_scroll_step(unsigned u_val)
{
    m_step = u_val;
}

unsigned TrackbarBase::get_scroll_step() const
{
    return m_step;
}

void TrackbarBase::set_orientation(bool b_vertical)
{
    m_vertical = b_vertical;
    // TODO: write handler
}

bool TrackbarBase::get_orientation() const
{
    return m_vertical;
}

void TrackbarBase::set_auto_focus(bool b_state)
{
    m_auto_focus = b_state;
}

bool TrackbarBase::get_auto_focus() const
{
    return m_auto_focus;
}

void TrackbarBase::set_direction(bool b_reversed)
{
    m_reversed = b_reversed;
}

void TrackbarBase::set_mouse_wheel_direction(bool b_reversed)
{
    m_mouse_wheel_reversed = b_reversed;
}

bool TrackbarBase::get_direction() const
{
    return m_reversed;
}

void TrackbarBase::set_enabled(bool enabled)
{
    EnableWindow(get_wnd(), enabled);
}

bool TrackbarBase::get_enabled() const
{
    return IsWindowEnabled(get_wnd()) != 0;
}

void Trackbar::get_thumb_rect(unsigned pos, unsigned range, RECT* rc) const
{
    RECT rc_client;
    GetClientRect(get_wnd(), &rc_client);

    unsigned cx = calculate_thumb_size();
    if (get_orientation()) {
        rc->left = 2;
        rc->right = rc_client.right - 2;
        rc->top = range ? MulDiv(get_direction() ? range - pos : pos, rc_client.bottom - cx, range)
                        : get_direction() ? rc_client.bottom - cx : 0;
        rc->bottom = rc->top + cx;
    } else {
        rc->top = 2;
        rc->bottom = rc_client.bottom - 2;
        rc->left = range ? MulDiv(get_direction() ? range - pos : pos, rc_client.right - cx, range)
                         : get_direction() ? rc_client.right - cx : 0;
        rc->right = rc->left + cx;
    }
}

void Trackbar::get_channel_rect(RECT* rc) const
{
    RECT rc_client;
    GetClientRect(get_wnd(), &rc_client);
    unsigned cx = calculate_thumb_size();

    rc->left = get_orientation() ? rc_client.right / 2 - 2 : rc_client.left + cx / 2;
    rc->right = get_orientation() ? rc_client.right / 2 + 2 : rc_client.right - cx + cx / 2;
    rc->top = get_orientation() ? rc_client.top + cx / 2 : rc_client.bottom / 2 - 2;
    rc->bottom = get_orientation() ? rc_client.bottom - cx + cx / 2 : rc_client.bottom / 2 + 2;
}

void TrackbarBase::get_thumb_rect(RECT* rc) const
{
    get_thumb_rect(m_display_position, m_range, rc);
}

bool TrackbarBase::get_hot() const
{
    return m_thumb_hot;
}

bool TrackbarBase::get_tracking() const
{
    return m_dragging;
}
void TrackbarBase::update_hot_status(POINT pt)
{
    RECT rc;
    get_thumb_rect(&rc);
    bool in_rect = PtInRect(&rc, pt) != 0;

    POINT pts = pt;
    MapWindowPoints(get_wnd(), HWND_DESKTOP, &pts, 1);

    bool b_in_wnd = WindowFromPoint(pts) == get_wnd();
    bool b_new_hot = in_rect && b_in_wnd;

    if (m_thumb_hot != b_new_hot) {
        m_thumb_hot = b_new_hot;
        if (m_thumb_hot)
            SetCapture(get_wnd());
        else if (GetCapture() == get_wnd() && !m_dragging)
            ReleaseCapture();
        RedrawWindow(get_wnd(), &rc, nullptr, RDW_INVALIDATE | /*RDW_ERASE|*/ RDW_UPDATENOW /*|RDW_ERASENOW*/);
    }
}

void Trackbar::draw_background(HDC dc, const RECT* rc) const
{
    HWND wnd_parent = GetParent(get_wnd());
    POINT pt = {0, 0};
    POINT pt_old = {0, 0};
    MapWindowPoints(get_wnd(), wnd_parent, &pt, 1);
    OffsetWindowOrgEx(dc, pt.x, pt.y, &pt_old);
    if (SendMessage(wnd_parent, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0) == FALSE)
        SendMessage(wnd_parent, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_ERASEBKGND);
    SetWindowOrgEx(dc, pt_old.x, pt_old.y, nullptr);
}

void Trackbar::draw_thumb(HDC dc, const RECT* rc) const
{
    if (get_theme_handle()) {
        const auto part_id = get_orientation() ? TKP_THUMBVERT : TKP_THUMB;
        const auto state_id
            = get_enabled() ? (get_tracking() ? TUS_PRESSED : (get_hot() ? TUS_HOT : TUS_NORMAL)) : TUS_DISABLED;
        DrawThemeBackground(get_theme_handle(), dc, part_id, state_id, rc, nullptr);
    } else {
        HPEN pn_highlight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHIGHLIGHT));
        HPEN pn_light = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DLIGHT));
        HPEN pn_dkshadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));
        HPEN pn_shadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));

        HPEN pn_old = SelectPen(dc, pn_highlight);

        MoveToEx(dc, rc->left, rc->top, nullptr);
        LineTo(dc, rc->right - 1, rc->top);
        SelectPen(dc, pn_dkshadow);
        LineTo(dc, rc->right - 1, rc->bottom - 1);
        SelectPen(dc, pn_highlight);
        MoveToEx(dc, rc->left, rc->top, nullptr);
        LineTo(dc, rc->left, rc->bottom - 1);
        SelectPen(dc, pn_dkshadow);
        LineTo(dc, rc->right, rc->bottom - 1);

        SelectPen(dc, pn_light);
        MoveToEx(dc, rc->left + 1, rc->top + 1, nullptr);
        LineTo(dc, rc->right - 2, rc->top + 1);
        MoveToEx(dc, rc->left + 1, rc->top + 1, nullptr);
        LineTo(dc, rc->left + 1, rc->bottom - 2);

        SelectPen(dc, pn_shadow);
        LineTo(dc, rc->right - 1, rc->bottom - 2);
        MoveToEx(dc, rc->right - 2, rc->top + 1, nullptr);
        LineTo(dc, rc->right - 2, rc->bottom - 2);

        SelectPen(dc, pn_old);

        DeleteObject(pn_light);
        DeleteObject(pn_highlight);
        DeleteObject(pn_shadow);
        DeleteObject(pn_dkshadow);

        RECT rc_fill = *rc;

        rc_fill.top += 2;
        rc_fill.left += 2;
        rc_fill.right -= 2;
        rc_fill.bottom -= 2;

        HBRUSH br = GetSysColorBrush(COLOR_BTNFACE);
        FillRect(dc, &rc_fill, br);
        if (!get_enabled()) {
            COLORREF cr_btnhighlight = GetSysColor(COLOR_BTNHIGHLIGHT);
            for (int x = rc_fill.left; x < rc_fill.right; x++)
                for (int y = rc_fill.top; y < rc_fill.bottom; y++)
                    if ((x + y) % 2)
                        SetPixel(dc, x, y, cr_btnhighlight); // i dont have anything better than SetPixel
        }
    }
}

void Trackbar::draw_channel(HDC dc, const RECT* rc) const
{
    if (get_theme_handle()) {
        DrawThemeBackground(
            get_theme_handle(), dc, get_orientation() ? TKP_TRACKVERT : TKP_TRACK, TUTS_NORMAL, rc, nullptr);
    } else {
        RECT rc_temp = *rc;
        DrawEdge(dc, &rc_temp, EDGE_SUNKEN, BF_RECT);
    }
}

HTHEME TrackbarBase::get_theme_handle() const
{
    return m_theme;
}

bool TrackbarBase::on_hooked_message(uih::MessageHookType p_type, int code, WPARAM wp, LPARAM lp)
{
    auto lpkeyb = uih::GetKeyboardLParam(lp);
    if (wp == VK_ESCAPE && !lpkeyb.transition_code && !lpkeyb.previous_key_state) {
        destroy_tooltip();
        if (GetCapture() == get_wnd())
            ReleaseCapture();
        m_dragging = false;
        set_position_internal(m_position);
        uih::deregister_message_hook(uih::MessageHookType::type_keyboard, this);
        m_hook_registered = false;
        return true;
    }
    return false;
}

unsigned Trackbar::calculate_thumb_size() const
{
    RECT rc_client;
    GetClientRect(get_wnd(), &rc_client);
    return MulDiv(get_orientation() ? rc_client.right : rc_client.bottom, 9, 20);
}

unsigned TrackbarBase::calculate_position_from_point(const POINT& pt_client) const
{
    RECT rc_channel;
    RECT rc_client;
    GetClientRect(get_wnd(), &rc_client);
    get_channel_rect(&rc_channel);
    POINT pt = pt_client;

    if (get_orientation()) {
        pfc::swap_t(pt.x, pt.y);
        pfc::swap_t(rc_channel.left, rc_channel.top);
        pfc::swap_t(rc_channel.bottom, rc_channel.right);
        pfc::swap_t(rc_client.left, rc_client.top);
        pfc::swap_t(rc_client.bottom, rc_client.right);
    }

    int cx = pt.x;

    if (cx < rc_channel.left)
        cx = rc_channel.left;
    else if (cx > rc_channel.right)
        cx = rc_channel.right;

    return rc_channel.right - rc_channel.left
        ? MulDiv(m_reversed ? rc_channel.right - cx : cx - rc_channel.left, m_range, rc_channel.right - rc_channel.left)
        : 0;
}

LRESULT TrackbarBase::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCCREATE:
        break;
    case WM_CREATE: {
        if (IsThemeActive() && IsAppThemed()) {
            m_theme = OpenThemeData(wnd, L"Trackbar");
        }
    } break;
    case WM_THEMECHANGED: {
        {
            if (m_theme) {
                CloseThemeData(m_theme);
                m_theme = nullptr;
            }
            if (IsThemeActive() && IsAppThemed())
                m_theme = OpenThemeData(wnd, L"Trackbar");
        }
    } break;
    case WM_DESTROY: {
        if (m_hook_registered) {
            uih::deregister_message_hook(uih::MessageHookType::type_keyboard, this);
            m_hook_registered = false;
        }
        {
            if (m_theme)
                CloseThemeData(m_theme);
            m_theme = nullptr;
        }
    } break;
    case WM_NCDESTROY:
        break;
    case WM_SIZE:
        RedrawWindow(wnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
        break;
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (m_dragging) {
            if (!m_last_mousemove.m_valid || wp != m_last_mousemove.m_wp || lp != m_last_mousemove.m_lp) {
                if (get_enabled()) {
                    unsigned pos = calculate_position_from_point(pt);
                    set_position_internal(pos);
                    if (m_wnd_tooltip && m_host) {
                        POINT pts = pt;
                        ClientToScreen(wnd, &pts);
                        TrackbarString temp;
                        m_host->get_tooltip_text(pos, temp);
                        update_tooltip(pts, temp.data());
                    }
                    if (m_host)
                        m_host->on_position_change(pos, true);
                }
            }
            m_last_mousemove.m_valid = true;
            m_last_mousemove.m_wp = wp;
            m_last_mousemove.m_lp = lp;
        } else {
            update_hot_status(pt);
        }
    } break;
    case WM_ENABLE: {
        RECT rc;
        get_thumb_rect(&rc);
        InvalidateRect(wnd, &rc, TRUE);
    } break;
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_XBUTTONDOWN: {
        if (get_enabled() && get_auto_focus() && GetFocus() != wnd)
            SetFocus(wnd);

        if (m_dragging) {
            destroy_tooltip();
            if (GetCapture() == wnd)
                ReleaseCapture();
            uih::deregister_message_hook(uih::MessageHookType::type_keyboard, this);
            m_hook_registered = false;
            // SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : uFindParentPopup(wnd));
            m_dragging = false;
            set_position_internal(m_position);
        }
    } break;
    case WM_LBUTTONDOWN: {
        if (get_enabled()) {
            if (get_auto_focus() && GetFocus() != wnd)
                SetFocus(wnd);

            POINT pt;

            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);

            RECT rc_client;
            GetClientRect(wnd, &rc_client);

            if (PtInRect(&rc_client, pt)) {
                m_dragging = true;
                SetCapture(wnd);

                // SetFocus(wnd);
                uih::register_message_hook(uih::MessageHookType::type_keyboard, this);
                m_hook_registered = true;

                unsigned pos = calculate_position_from_point(pt);
                set_position_internal(pos);
                POINT pts = pt;
                ClientToScreen(wnd, &pts);
                if (m_show_tooltips && m_host) {
                    TrackbarString temp;
                    m_host->get_tooltip_text(pos, temp);
                    create_tooltip(temp.data(), pts);
                }
            }
            m_last_mousemove.m_valid = false;
        }
    }
        return 0;
    case WM_LBUTTONUP: {
        if (m_dragging) {
            destroy_tooltip();
            if (GetCapture() == wnd)
                ReleaseCapture();
            m_dragging = false;
            if (get_enabled()) {
                POINT pt;

                pt.x = GET_X_LPARAM(lp);
                pt.y = GET_Y_LPARAM(lp);

                unsigned pos = calculate_position_from_point(pt);
                set_position(pos);
            }
            // SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : uFindParentPopup(wnd));
            uih::deregister_message_hook(uih::MessageHookType::type_keyboard, this);
            m_hook_registered = false;
            if (m_host)
                m_host->on_position_change(m_display_position, false);

            m_last_mousemove.m_valid = false;
        }
    }
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP: {
        if ((wp == VK_ESCAPE || wp == VK_RETURN) && m_host && m_host->on_key(wp, lp))
            return 0;
        if (!(lp & (1 << 31)) && (wp == VK_LEFT || wp == VK_DOWN || wp == VK_RIGHT || wp == VK_UP)) {
            bool down = !(wp == VK_LEFT || wp == VK_UP); //! get_direction();
            unsigned newpos = m_position;
            if (down && m_step > m_position)
                newpos = 0;
            else if (!down && m_step + m_position > m_range)
                newpos = m_range;
            else
                newpos += down ? -(int)m_step : m_step;
            if (newpos != m_position) {
                set_position(newpos);
                if (m_host)
                    m_host->on_position_change(m_position, false);
            }
        }
        if (!(lp & (1 << 31)) && (wp == VK_HOME || wp == VK_END)) {
            bool down = wp != VK_END; //! get_direction();
            unsigned newpos;
            if (down)
                newpos = m_range;
            else
                newpos = 0;
            if (newpos != m_position) {
                set_position(newpos);
                if (m_host)
                    m_host->on_position_change(m_position, false);
            }
        }
    } break;
    case WM_MOUSEWHEEL: {
        UINT ucNumLines = 3; // 3 is the default
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ucNumLines, 0);
        short zDelta = GET_WHEEL_DELTA_WPARAM(wp);
        if (ucNumLines == WHEEL_PAGESCROLL)
            ucNumLines = 3;
        int delta = MulDiv(m_step * zDelta, ucNumLines, WHEEL_DELTA);
        bool down = delta < 0;
        // if (get_direction()) down = down == false;
        if (!get_orientation())
            down = !down;
        if (m_mouse_wheel_reversed)
            down = !down;
        unsigned offset = abs(delta);

        unsigned newpos = m_position;
        if (down && offset > m_position)
            newpos = 0;
        else if (!down && offset + m_position > m_range)
            newpos = m_range;
        else
            newpos += down ? -static_cast<int>(offset) : offset;
        if (newpos != m_position) {
            set_position(newpos);
            if (m_host)
                m_host->on_position_change(m_position, false);
        }
    }
        return 0;
#if 0
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE)
            {
                destroy_tooltip();
                if (GetCapture() == wnd)
                    ReleaseCapture();
                SetFocus(IsWindow(m_wnd_prev) ? m_wnd_prev : uFindParentPopup(wnd));
                m_dragging = false;
                set_position_internal(m_position);
                return 0;
            }
            break;
        case WM_SETFOCUS:
            m_wnd_prev = (HWND)wp;
            break;
#endif
    case WM_MOVE:
        RedrawWindow(wnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);
        break;
    case WM_ERASEBKGND:
        return FALSE;
    case WM_PAINT: {
        RECT rc_client;
        GetClientRect(wnd, &rc_client);

        PAINTSTRUCT ps;

        HDC dc = BeginPaint(wnd, &ps);

        RECT rc_thumb;

        get_thumb_rect(&rc_thumb);

        RECT rc_track; // channel
        get_channel_rect(&rc_track);

        // Offscreen rendering to eliminate flicker
        HDC dc_mem = CreateCompatibleDC(dc);

        // Create a rect same size of update rect
        HBITMAP bm_mem = CreateCompatibleBitmap(dc, rc_client.right, rc_client.bottom);

        auto bm_old = (HBITMAP)SelectObject(dc_mem, bm_mem);

        // we should always be erasing first, so shouldn't be needed
        BitBlt(dc_mem, 0, 0, rc_client.right, rc_client.bottom, dc, 0, 0, SRCCOPY);
        if (ps.fErase) {
            draw_background(dc_mem, &rc_client);
        }

        draw_channel(dc_mem, &rc_track);
        draw_thumb(dc_mem, &rc_thumb);

        BitBlt(dc, 0, 0, rc_client.right, rc_client.bottom, dc_mem, 0, 0, SRCCOPY);
        SelectObject(dc_mem, bm_old);
        DeleteObject(bm_mem);
        DeleteDC(dc_mem);
        EndPaint(wnd, &ps);
    }
        return 0;
    }
    return DefWindowProc(wnd, msg, wp, lp);
}

} // namespace uih
