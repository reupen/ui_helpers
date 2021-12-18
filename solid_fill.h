#pragma once

namespace uih {

class FillWindow {
public:
    enum Mode {
        mode_solid_fill,
        mode_theme_fill,
        mode_theme_solid_fill,
    };

    void set_fill_colour(COLORREF value)
    {
        m_mode = mode_solid_fill;
        m_fill_colour = value;
        if (m_container_window->get_wnd())
            redraw();
    }
    void set_fill_themed(const WCHAR* pclass, int part, int state, COLORREF fallback)
    {
        m_mode = mode_theme_fill;
        m_theme_state = state;
        m_theme_part = part;
        m_fill_colour = fallback;
        m_theme_class = pclass;
        if (m_theme)
            CloseThemeData(m_theme);
        m_theme = nullptr;
        if (m_container_window->get_wnd()) {
            m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(m_container_window->get_wnd(), m_theme_class)
                                                       : nullptr;
            redraw();
        }
    }

    void set_fill_themed_colour(const WCHAR* pclass, int index)
    {
        m_mode = mode_theme_solid_fill;
        m_theme_class = pclass;
        m_theme_colour_index = index;
        if (m_theme)
            CloseThemeData(m_theme);
        m_theme = nullptr;
        if (m_container_window->get_wnd()) {
            m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(m_container_window->get_wnd(), m_theme_class)
                                                       : nullptr;
            redraw();
        }
    }

    FillWindow()
    {
        auto window_config = uih::ContainerWindowConfig{L"columns_ui_fill"};
        window_config.window_styles = WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE;
        window_config.window_ex_styles = WS_EX_CONTROLPARENT | WS_EX_STATICEDGE;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config, [this](auto&&... args) { return on_message(std::forward<decltype(args)>(args)...); });
    }

    template <typename... Ts>
    HWND create(Ts&&... vals)
    {
        return m_container_window->create(std::forward<Ts>(vals)...);
    }
    void destroy() { m_container_window->destroy(); }
    HWND get_wnd() const { return m_container_window->get_wnd(); }

private:
    void redraw()
    {
        RedrawWindow(m_container_window->get_wnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        // RedrawWindow(get_wnd(), 0, 0, RDW_INVALIDATE|RDW_ERASE);
    }
    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
        case WM_CREATE: {
            if (m_mode == mode_theme_fill || m_mode == mode_theme_solid_fill)
                m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(wnd, m_theme_class) : nullptr;
            SetWindowTheme(wnd, L"Explorer", nullptr);
        } break;
        case WM_DESTROY: {
            if (m_theme)
                CloseThemeData(m_theme);
            m_theme = nullptr;
        } break;
        case WM_THEMECHANGED: {
            if (m_theme)
                CloseThemeData(m_theme);
            m_theme = nullptr;
            if (m_mode == mode_theme_fill || m_mode == mode_theme_solid_fill)
                m_theme = (IsThemeActive() && IsAppThemed()) ? OpenThemeData(wnd, m_theme_class) : nullptr;
            redraw();
        } break;
        case WM_ENABLE:
            redraw();
            break;
        case WM_ERASEBKGND:
            return FALSE;
        case WM_PAINT:
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(wnd, &ps);
            wil::unique_hbrush brush;
            RECT rc;
            GetClientRect(wnd, &rc);

            if (!IsWindowEnabled(wnd)) {
                FillRect(dc, &ps.rcPaint, GetSysColorBrush(COLOR_BTNFACE));
            } else if (m_mode == mode_theme_fill && m_theme
                && IsThemePartDefined(m_theme, m_theme_part, m_theme_state)) {
                if (IsThemeBackgroundPartiallyTransparent(m_theme, m_theme_part, m_theme_state))
                    DrawThemeParentBackground(wnd, dc, &rc);
                if (FAILED(DrawThemeBackground(m_theme, dc, m_theme_part, m_theme_state, &rc, nullptr))) {
                    brush.reset(CreateSolidBrush(m_fill_colour));
                    FillRect(dc, &ps.rcPaint, brush.get());
                }
            } else if (m_mode == mode_theme_solid_fill && m_theme) {
                COLORREF cr_fill = GetThemeSysColor(m_theme, m_theme_colour_index);
                brush.reset(CreateSolidBrush(cr_fill));
                FillRect(dc, &ps.rcPaint, brush.get());
            } else {
                brush.reset(CreateSolidBrush(m_fill_colour));
                FillRect(dc, &ps.rcPaint, brush.get());
            }
            brush.release();
            EndPaint(wnd, &ps);
            return 0;
        }
        return DefWindowProc(wnd, msg, wp, lp);
    };

    Mode m_mode{mode_solid_fill};
    pfc::string_simple_t<WCHAR> m_theme_class;
    COLORREF m_fill_colour = NULL;
    HTHEME m_theme = nullptr;
    int m_theme_part = NULL;
    int m_theme_state = NULL;
    int m_theme_colour_index = NULL;
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};

class TranslucentFillWindow {
public:
    void set_fill_colour(COLORREF value)
    {
        m_fill_colour = value;
        if (m_container_window->get_wnd())
            redraw();
    }

    TranslucentFillWindow()
    {
        auto window_config = uih::ContainerWindowConfig{L"columns_ui_transparent_fill"};
        window_config.window_styles = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        window_config.window_ex_styles = WS_EX_LAYERED;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config, [this](auto&&... args) { return on_message(std::forward<decltype(args)>(args)...); });
    }

    template <typename... Ts>
    HWND create(Ts&&... vals)
    {
        return m_container_window->create(std::forward<Ts>(vals)...);
    }
    void destroy() { m_container_window->destroy(); }
    HWND get_wnd() const { return m_container_window->get_wnd(); }

private:
    void redraw() { RedrawWindow(m_container_window->get_wnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW); }
    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    COLORREF m_fill_colour{NULL};
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};
} // namespace uih
