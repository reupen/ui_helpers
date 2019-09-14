#pragma once

#include <utility>

namespace uih {
class InfoBox {
public:
    InfoBox(std::function<void(HWND)> on_creation = nullptr, std::function<void(HWND)> on_destruction = nullptr,
        alignment text_alignment = ALIGN_LEFT)
        : m_wnd_edit(nullptr)
        , m_wnd_button(nullptr)
        , m_wnd_static(nullptr)
        , m_text_alignment{text_alignment}
        , m_on_creation(std::move(on_creation))
        , m_on_destruction(std::move(on_destruction))
    {
        auto window_config = uih::ContainerWindowConfig{L"uih_message_window"};
        window_config.window_styles = uih::window_styles::style_popup_default;
        window_config.window_ex_styles = uih::window_styles::ex_style_popup_default;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config, [this](auto&&... args) { return on_message(std::forward<decltype(args)>(args)...); });
    }

    static void g_run(HWND wnd_parent, const char* p_title, const char* p_text, INT icon = OIC_INFORMATION,
        std::function<void(HWND)> on_creation = nullptr, std::function<void(HWND)> on_destruction = nullptr,
        alignment text_alignment = ALIGN_LEFT)
    {
        auto message_window = std::make_unique<InfoBox>(on_creation, on_destruction, text_alignment);
        message_window->create(wnd_parent, p_title, p_text, icon);
        message_window.release();
    }

    int calc_height() const
    {
        RECT rc;
        RECT rcw;
        RECT rcwc;
        GetWindowRect(m_wnd_button, &rc);
        GetWindowRect(m_container_window->get_wnd(), &rcw);
        GetClientRect(m_container_window->get_wnd(), &rcwc);
        return get_large_padding() * 6 + uih::scale_dpi_value(1) + RECT_CY(rc) + (RECT_CY(rcw) - RECT_CY(rcwc))
            + std::max((t_size)get_text_height(), (t_size)get_icon_height());
    }

    int get_text_height() const
    {
        return SendMessage(m_wnd_edit, EM_GETLINECOUNT, 0, 0) * uih::get_font_height(m_font);
    }

    int get_icon_height() const
    {
        RECT rc_icon;
        GetWindowRect(m_wnd_static, &rc_icon);
        return RECT_CY(rc_icon);
    }

private:
    static int get_large_padding() { return uih::scale_dpi_value(11); }
    static int get_small_padding() { return uih::scale_dpi_value(7); }
    static int get_button_width() { return uih::scale_dpi_value(75); }

    void create(HWND wnd_parent, const char* p_title, const char* p_text, INT oem_icon = OIC_INFORMATION)
    {
        RECT rc;
        GetWindowRect(wnd_parent, &rc);
        int cx = uih::scale_dpi_value(470);
        int cy = uih::scale_dpi_value(175);

        HWND wnd = m_container_window->create(wnd_parent,
            uih::WindowPosition((rc.left + (RECT_CX(rc) - cx) / 2), (rc.top + (RECT_CY(rc) - cy) / 2), cx, cy));

        SetWindowText(wnd, pfc::stringcvt::string_wide_from_utf8(p_title));
        pfc::string8 buffer;
        const char* ptr = p_text;

        while (*ptr) {
            const char* start = ptr;
            while (*ptr && *ptr != '\r' && *ptr != '\n')
                ptr++;
            if (ptr > start)
                buffer.add_string(start, ptr - start);
            if (*ptr) {
                buffer.add_byte('\r');
                buffer.add_byte('\n');
            }
            if (*ptr == '\r')
                ptr++;
            if (*ptr == '\n')
                ptr++;
        }

        SetWindowText(m_wnd_edit, pfc::stringcvt::string_wide_from_utf8(buffer));

        HICON icon{};
        const HRESULT hr = LoadIconMetric(nullptr, MAKEINTRESOURCE(oem_icon), LIM_LARGE, &icon);
        if (SUCCEEDED(hr)) {
            m_icon = icon;
            SendMessage(m_wnd_static, STM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(icon));
        }

        cy = std::min(calc_height(), std::max(static_cast<int>(RECT_CY(rc)), uih::scale_dpi_value(150)));
        int y = (rc.top + (RECT_CY(rc) - cy) / 2);
        if (y < rc.top)
            y = rc.top;
        SetWindowPos(wnd, nullptr, (rc.left + (RECT_CX(rc) - cx) / 2), y, cx, cy, SWP_NOZORDER);
        ShowWindow(wnd, SW_SHOWNORMAL);
    }

    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
        case WM_NCCREATE:
            if (m_on_creation)
                m_on_creation(wnd);
            break;
        case WM_SIZE: {
            RedrawWindow(wnd, nullptr, nullptr, RDW_INVALIDATE);
            RECT rc;
            RECT rcicon;
            GetWindowRect(m_wnd_button, &rc);
            GetWindowRect(m_wnd_static, &rcicon);
            int cy_button = RECT_CY(rc);
            HDWP dwp = BeginDeferWindowPos(3);

            const uih::WindowPosition edit_position{get_large_padding() * 2 + get_small_padding() + RECT_CX(rcicon),
                get_large_padding() * 2, LOWORD(lp) - get_large_padding() * 4 - get_small_padding() - RECT_CX(rcicon),
                HIWORD(lp) - get_large_padding() * 6 - cy_button};
            const uih::WindowPosition button_position{LOWORD(lp) - get_large_padding() * 2 - get_button_width(),
                HIWORD(lp) - get_large_padding() - cy_button, get_button_width(), cy_button};
            const uih::WindowPosition static_position{
                get_large_padding() * 2, get_large_padding() * 2, RECT_CX(rcicon), RECT_CY(rcicon)};
            dwp = DeferWindowPos(dwp, m_wnd_edit, nullptr, edit_position.x, edit_position.y, edit_position.cx,
                edit_position.cy, SWP_NOZORDER);
            dwp = DeferWindowPos(dwp, m_wnd_button, nullptr, button_position.x, button_position.y, button_position.cx,
                button_position.cy, SWP_NOZORDER);
            dwp = DeferWindowPos(dwp, m_wnd_static, nullptr, static_position.x, static_position.y, static_position.cx,
                static_position.cy, SWP_NOZORDER);
            EndDeferWindowPos(dwp);
            RedrawWindow(wnd, nullptr, nullptr, RDW_UPDATENOW);
        }
            return 0;
        case WM_GETMINMAXINFO: {
            RECT rc;
            RECT rcicon;
            RECT rcw;
            RECT rcwc;
            GetWindowRect(m_wnd_button, &rc);
            GetWindowRect(m_wnd_static, &rcicon);
            GetWindowRect(wnd, &rcw);
            GetClientRect(wnd, &rcwc);
            LPMINMAXINFO lpmmi = reinterpret_cast<LPMINMAXINFO>(lp);
            lpmmi->ptMinTrackSize.x = get_large_padding() * 4 + get_small_padding() + RECT_CX(rcicon)
                + uih::scale_dpi_value(50) + (RECT_CX(rcw) - RECT_CX(rcwc));
            lpmmi->ptMinTrackSize.y
                = get_large_padding() * 4 + RECT_CY(rcicon) + RECT_CY(rc) + (RECT_CY(rcw) - RECT_CY(rcwc));
        }
            return 0;
        case WM_CREATE: {
            m_font = uih::create_icon_font();

            auto edit_styles
                = WS_CHILD | WS_VISIBLE | WS_GROUP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL;

            edit_styles |= get_edit_alignment_style();

            RECT rc;
            GetClientRect(wnd, &rc);
            m_wnd_edit = CreateWindowEx(0, WC_EDIT, L"", edit_styles, get_large_padding(), 
                get_large_padding(), RECT_CX(rc) - get_large_padding() * 2,
                RECT_CY(rc) - get_large_padding() * 2, wnd, reinterpret_cast<HMENU>(1001), mmh::get_current_instance(),
                nullptr);
            SendMessage(m_wnd_edit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), MAKELPARAM(FALSE, 0));
            int cy_button = uih::get_font_height(m_font) + uih::scale_dpi_value(10);
            m_wnd_button = CreateWindowEx(0, WC_BUTTON, L"Close",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON | WS_GROUP,
                RECT_CX(rc) - get_large_padding() * 2 - get_button_width(),
                RECT_CY(rc) - get_large_padding() - cy_button, get_button_width(), cy_button, wnd,
                reinterpret_cast<HMENU>(IDCANCEL), mmh::get_current_instance(), nullptr);
            SendMessage(m_wnd_button, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), MAKELPARAM(FALSE, 0));
            m_wnd_static = CreateWindowEx(0, WC_STATIC, L"", WS_CHILD | WS_VISIBLE | WS_GROUP | SS_ICON, 0, 0,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), wnd, reinterpret_cast<HMENU>(1002),
                mmh::get_current_instance(), nullptr);
        }
            return 0;
        case WM_SHOWWINDOW:
            if (wp == TRUE && lp == 0 && m_wnd_button)
                SetFocus(m_wnd_button);
            break;
        case DM_GETDEFID:
            return IDCANCEL | (DC_HASDEFID << 16);
        case WM_DESTROY:
            m_font.release();
            return 0;
        case WM_CTLCOLORSTATIC:
            SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
            SetTextColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_ERASEBKGND:
            return FALSE;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(wnd, &ps);
            if (dc) {
                RECT rc_client;
                RECT rc_button;
                GetClientRect(wnd, &rc_client);
                RECT rc_fill = rc_client;
                if (m_wnd_button) {
                    GetWindowRect(m_wnd_button, &rc_button);
                    rc_fill.bottom -= RECT_CY(rc_button) + get_large_padding();
                    rc_fill.bottom -= get_large_padding();
                }

                FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_WINDOW));

                if (m_wnd_button) {
                    rc_fill.top = rc_fill.bottom;
                    rc_fill.bottom += uih::scale_dpi_value(1);
                    FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DLIGHT));
                }
                rc_fill.top = rc_fill.bottom;
                rc_fill.bottom = rc_client.bottom;
                FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DFACE));
                EndPaint(wnd, &ps);
            }
        }
            return 0;
        case WM_NCDESTROY:
            if (m_on_destruction)
                m_on_destruction(wnd);
            delete this;
            break;
        case WM_COMMAND:
            switch (wp) {
            case IDCANCEL:
                SendMessage(wnd, WM_CLOSE, NULL, NULL);
                return 0;
            }
            break;
        case WM_CLOSE:
            auto container_window{move(m_container_window)};
            container_window->destroy();
            return 0;
        }
        return DefWindowProc(wnd, msg, wp, lp);
    }

    DWORD get_edit_alignment_style() const
    {
        switch (m_text_alignment) {
        case ALIGN_LEFT:
        default:
            return ES_LEFT;
        case ALIGN_CENTRE:
            return ES_CENTER;
        case ALIGN_RIGHT:
            return ES_RIGHT;
        }
    }

    HWND m_wnd_edit, m_wnd_button, m_wnd_static;
    alignment m_text_alignment{ALIGN_LEFT};
    gdi_object_t<HFONT>::ptr_t m_font;
    icon_ptr m_icon;
    std::function<void(HWND)> m_on_creation;
    std::function<void(HWND)> m_on_destruction;
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};
} // namespace uih
