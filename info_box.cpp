#include "stdafx.h"

namespace uih {

void InfoBox::s_run(HWND wnd_parent, const char* p_title, const char* p_text, INT icon,
    std::function<void(HWND)> on_creation, std::function<void(HWND)> on_destruction, alignment text_alignment)
{
    const auto message_window
        = std::make_shared<InfoBox>(std::move(on_creation), std::move(on_destruction), text_alignment);
    message_window->create(wnd_parent, p_title, p_text, icon);
}

int InfoBox::calc_height() const
{
    RECT button_rect{};
    GetWindowRect(m_wnd_button, &button_rect);

    RECT window_rect{};
    GetWindowRect(m_wnd, &window_rect);

    RECT client_rect{};
    GetClientRect(m_wnd, &client_rect);

    return get_large_padding() * 6 + scale_dpi_value(1) + wil::rect_height(button_rect)
        + (wil::rect_height(window_rect) - wil::rect_height(client_rect))
        + std::max(get_text_height(), get_icon_height());
}

void InfoBox::create(HWND wnd_parent, const char* p_title, const char* p_text, INT oem_icon)
{
    std::vector<uint8_t> dlg_template(sizeof(DLGTEMPLATE) + sizeof(WORD) * 2, 0);

    const auto lpdt = reinterpret_cast<LPDLGTEMPLATE>(dlg_template.data());
    lpdt->style = WS_SYSMENU | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | DS_MODALFRAME;

    const auto title_utf16 = mmh::to_utf16(p_title);
    const std::span title_data(
        reinterpret_cast<const uint8_t*>(title_utf16.data()), sizeof(wchar_t) * (title_utf16.size() + 1));
    ranges::insert(dlg_template, dlg_template.end(), title_data);

    RECT parent_rect{};
    GetWindowRect(wnd_parent, &parent_rect);
    const int cx = scale_dpi_value(470);

    const auto wnd = modeless_dialog_box(reinterpret_cast<LPDLGTEMPLATE>(dlg_template.data()), wnd_parent,
        [this, self = shared_from_this()](
            auto wnd, auto msg, auto wp, auto lp) -> INT_PTR { return on_message(wnd, msg, wp, lp); });

    SetWindowPos(wnd, nullptr, 0, 0, cx, scale_dpi_value(175), SWP_NOZORDER | SWP_NOMOVE);

    const auto normalised_text = std::regex_replace(p_text, std::regex("\n"), "\r\n");
    SetWindowText(m_wnd_edit, mmh::to_utf16(normalised_text).c_str());

    HICON icon{};
    const HRESULT hr = LoadIconMetric(nullptr, MAKEINTRESOURCE(oem_icon), LIM_LARGE, &icon);
    if (SUCCEEDED(hr)) {
        m_icon = icon;
        SendMessage(m_wnd_static, STM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(icon));
    }

    const int cy
        = std::min(calc_height(), std::max(static_cast<int>(wil::rect_height(parent_rect)), scale_dpi_value(150)));
    const int x = parent_rect.left + (wil::rect_width(parent_rect) - cx) / 2;
    const int y = std::max<int>(parent_rect.top + (wil::rect_height(parent_rect) - cy) / 2, parent_rect.top);
    SetWindowPos(wnd, nullptr, x, y, cx, cy, SWP_NOZORDER);

    ShowWindow(wnd, SW_SHOWNORMAL);
}

INT_PTR InfoBox::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        m_wnd = wnd;

        if (m_on_creation)
            m_on_creation(wnd);

        m_font.reset(create_icon_font());

        RECT client_rect{};
        GetClientRect(wnd, &client_rect);

        const DWORD edit_styles = WS_CHILD | WS_VISIBLE | WS_GROUP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL
            | get_edit_alignment_style();

        m_wnd_edit = CreateWindowEx(0, WC_EDIT, L"", edit_styles, get_large_padding(), get_large_padding(),
            wil::rect_width(client_rect) - get_large_padding() * 2,
            wil::rect_height(client_rect) - get_large_padding() * 2, wnd, reinterpret_cast<HMENU>(1001),
            mmh::get_current_instance(), nullptr);
        SetWindowFont(m_wnd_edit, m_font.get(), FALSE);

        const int cy_button = get_font_height(m_font.get()) + scale_dpi_value(10);
        m_wnd_button
            = CreateWindowEx(0, WC_BUTTON, L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON | WS_GROUP,
                wil::rect_width(client_rect) - get_large_padding() * 2 - get_button_width(),
                wil::rect_height(client_rect) - get_large_padding() - cy_button, get_button_width(), cy_button, wnd,
                reinterpret_cast<HMENU>(IDCANCEL), mmh::get_current_instance(), nullptr);
        SetWindowFont(m_wnd_button, m_font.get(), FALSE);

        m_wnd_static = CreateWindowEx(0, WC_STATIC, L"", WS_CHILD | WS_VISIBLE | WS_GROUP | SS_ICON, 0, 0,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), wnd, reinterpret_cast<HMENU>(1002),
            mmh::get_current_instance(), nullptr);

        return TRUE;
    }
    case WM_NCDESTROY:
        if (m_on_destruction)
            m_on_destruction(wnd);

        m_wnd = nullptr;
        m_font.reset();
        break;
    case WM_SIZE: {
        RedrawWindow(wnd, nullptr, nullptr, RDW_INVALIDATE);

        RECT button_rect{};
        GetWindowRect(m_wnd_button, &button_rect);

        RECT icon_rect{};
        GetWindowRect(m_wnd_static, &icon_rect);

        const int cy_button = wil::rect_height(button_rect);
        HDWP dwp = BeginDeferWindowPos(3);

        const WindowPosition edit_position{get_large_padding() * 2 + get_small_padding() + wil::rect_width(icon_rect),
            get_large_padding() * 2,
            LOWORD(lp) - get_large_padding() * 4 - get_small_padding() - wil::rect_width(icon_rect),
            HIWORD(lp) - get_large_padding() * 6 - cy_button};
        const WindowPosition button_position{LOWORD(lp) - get_large_padding() * 2 - get_button_width(),
            HIWORD(lp) - get_large_padding() - cy_button, get_button_width(), cy_button};
        const WindowPosition static_position{
            get_large_padding() * 2, get_large_padding() * 2, wil::rect_width(icon_rect), wil::rect_height(icon_rect)};
        dwp = DeferWindowPos(dwp, m_wnd_edit, nullptr, edit_position.x, edit_position.y, edit_position.cx,
            edit_position.cy, SWP_NOZORDER);
        dwp = DeferWindowPos(dwp, m_wnd_button, nullptr, button_position.x, button_position.y, button_position.cx,
            button_position.cy, SWP_NOZORDER);
        dwp = DeferWindowPos(dwp, m_wnd_static, nullptr, static_position.x, static_position.y, static_position.cx,
            static_position.cy, SWP_NOZORDER);
        EndDeferWindowPos(dwp);
        RedrawWindow(wnd, nullptr, nullptr, RDW_UPDATENOW);
        break;
    }
    case WM_GETMINMAXINFO: {
        RECT button_rect{};
        GetWindowRect(m_wnd_button, &button_rect);

        RECT icon_rect{};
        GetWindowRect(m_wnd_static, &icon_rect);

        RECT window_rect{};
        GetWindowRect(wnd, &window_rect);

        RECT client_rect{};
        GetClientRect(wnd, &client_rect);

        auto lpmmi = reinterpret_cast<LPMINMAXINFO>(lp);
        lpmmi->ptMinTrackSize.x = get_large_padding() * 4 + get_small_padding() + wil::rect_width(icon_rect)
            + scale_dpi_value(50) + (wil::rect_width(window_rect) - wil::rect_width(client_rect));
        lpmmi->ptMinTrackSize.y = get_large_padding() * 4 + wil::rect_height(icon_rect) + wil::rect_height(button_rect)
            + (wil::rect_height(window_rect) - wil::rect_height(client_rect));
        break;
    }
    case WM_SHOWWINDOW:
        if (wp == TRUE && lp == 0 && m_wnd_button)
            SetFocus(m_wnd_button);
        break;
    case DM_GETDEFID:
        return IDCANCEL | (DC_HASDEFID << 16);
    case WM_CTLCOLORSTATIC:
        SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
        SetTextColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_ERASEBKGND:
        SetWindowLongPtr(wnd, DWLP_MSGRESULT, FALSE);
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(wnd, &ps);
        if (dc) {
            RECT rc_client{};
            RECT rc_button{};
            GetClientRect(wnd, &rc_client);
            RECT rc_fill = rc_client;
            if (m_wnd_button) {
                GetWindowRect(m_wnd_button, &rc_button);
                rc_fill.bottom -= wil::rect_height(rc_button) + get_large_padding();
                rc_fill.bottom -= get_large_padding();
            }

            FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_WINDOW));

            if (m_wnd_button) {
                rc_fill.top = rc_fill.bottom;
                rc_fill.bottom += scale_dpi_value(1);
                FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DLIGHT));
            }
            rc_fill.top = rc_fill.bottom;
            rc_fill.bottom = rc_client.bottom;
            FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DFACE));
            EndPaint(wnd, &ps);
        }
    }
        return TRUE;
    case WM_COMMAND:
        switch (wp) {
        case IDCANCEL:
            DestroyWindow(wnd);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

} // namespace uih
