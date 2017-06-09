#ifndef _COLUMNS_SOLID_FILL_H_
#define _COLUMNS_SOLID_FILL_H_

class window_fill
{
public:
    enum t_mode
    {
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
    void set_fill_themed(const WCHAR * pclass, int part, int state, COLORREF fallback)
    {
        m_mode = mode_theme_fill;
        m_theme_state = state;
        m_theme_part = part;
        m_fill_colour = fallback;
        m_theme_class = pclass;
        if (m_theme)
            CloseThemeData(m_theme);
        m_theme=nullptr;
        if (m_container_window->get_wnd())
        {
            m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(m_container_window->get_wnd(), m_theme_class) : nullptr;
            redraw();
        }
    }

    void set_fill_themed_colour(const WCHAR * pclass, int index)
    {
        m_mode = mode_theme_solid_fill;
        m_theme_class = pclass;
        m_theme_colour_index = index;
        if (m_theme)
            CloseThemeData(m_theme);
        m_theme=nullptr;
        if (m_container_window->get_wnd())
        {
            m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(m_container_window->get_wnd(), m_theme_class) : nullptr;
            redraw();
        }
    }

    window_fill()
    {
        auto window_config = uih::ContainerWindowConfig{L"columns_ui_fill"};
        window_config.window_styles = WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE;
        window_config.window_ex_styles = WS_EX_CONTROLPARENT | WS_EX_STATICEDGE;
        window_config.class_styles = NULL;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config,
            std::bind(&window_fill::on_message, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)
            );
    }

    template<typename... Ts>
    HWND create(Ts&&... vals)
    {
        return m_container_window->create(std::forward<Ts>(vals)...);
    }
    void destroy()
    {
        m_container_window->destroy();
    }
    HWND get_wnd() const
    {
        return m_container_window->get_wnd();
    }
private:
    void redraw()
    {
        RedrawWindow(m_container_window->get_wnd(), nullptr, nullptr, RDW_INVALIDATE|RDW_UPDATENOW);
        //RedrawWindow(get_wnd(), 0, 0, RDW_INVALIDATE|RDW_ERASE);
    }
    LRESULT on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
    {
        switch (msg)
        {
        case WM_CREATE:
            {
                if (m_mode == mode_theme_fill || m_mode == mode_theme_solid_fill)
                    m_theme = IsThemeActive() && IsAppThemed() ? OpenThemeData(wnd, m_theme_class) : nullptr;
                SetWindowTheme(wnd, L"Explorer", nullptr);
            }
            break;
        case WM_DESTROY:
            {
                if (m_theme) CloseThemeData(m_theme);
                m_theme = nullptr;
            }
            break;
        case WM_THEMECHANGED:
            {
                if (m_theme) CloseThemeData(m_theme);
                m_theme=nullptr;
                if (m_mode == mode_theme_fill || m_mode == mode_theme_solid_fill)
                    m_theme = (IsThemeActive() && IsAppThemed()) ? OpenThemeData(wnd, m_theme_class) : nullptr;
                redraw();
            }
            break;
        case WM_ENABLE:
            redraw();
            break;
        case WM_ERASEBKGND:
            return FALSE;
        case WM_PAINT:
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(wnd, &ps);
            gdi_object_t<HBRUSH>::ptr_t p_brush;
            RECT rc;
            GetClientRect(wnd, &rc);

            if (!IsWindowEnabled(wnd))
            {
                FillRect(dc, &ps.rcPaint, GetSysColorBrush(COLOR_BTNFACE));
            }
            else if (m_mode == mode_theme_fill && m_theme && IsThemePartDefined(m_theme, m_theme_part, m_theme_state))
            {
                if (IsThemeBackgroundPartiallyTransparent(m_theme, m_theme_part, m_theme_state))
                    DrawThemeParentBackground(wnd, dc, &rc);
                if (FAILED(DrawThemeBackground(m_theme, dc, m_theme_part, m_theme_state, &rc, nullptr)))
                {
                    p_brush = CreateSolidBrush(m_fill_colour);
                    FillRect(dc, &ps.rcPaint, p_brush);
                }
            }
            else if (m_mode == mode_theme_solid_fill && m_theme)
            {
                COLORREF cr_fill = GetThemeSysColor(m_theme, m_theme_colour_index);
                p_brush = CreateSolidBrush(cr_fill);
                FillRect(dc, &ps.rcPaint, p_brush);
            }
            else
            {
                p_brush = CreateSolidBrush(m_fill_colour);
                FillRect(dc, &ps.rcPaint, p_brush);
            }
            p_brush.release();
            EndPaint(wnd, &ps);
            return 0;
        }
        return DefWindowProc(wnd, msg, wp, lp);
    };

    t_mode m_mode{mode_solid_fill};
    pfc::string_simple_t<WCHAR> m_theme_class;
    COLORREF m_fill_colour = NULL;
    HTHEME m_theme = nullptr;
    int m_theme_part = NULL;
    int m_theme_state = NULL;
    int m_theme_colour_index = NULL;
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};

class window_transparent_fill : public ui_helpers::container_window
{
public:
    void set_fill_colour(COLORREF value)
    {
        m_fill_colour = value;
        if (get_wnd())
            redraw();
    }

    window_transparent_fill()
        : m_fill_colour(NULL)
    {};

private:
    void redraw()
    {
        RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE|RDW_UPDATENOW);
    }
    class_data & get_class_data()const override 
    {
        __implement_get_class_data_ex(_T("columns_ui_transparent_fill"), _T(""), false, 0, WS_POPUP|WS_CLIPSIBLINGS| WS_CLIPCHILDREN, /*WS_EX_TOPMOST|*/WS_EX_LAYERED, 0);
    }
    LRESULT on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp) override;

    COLORREF m_fill_colour;
};

#endif //_COLUMNS_SOLID_FILL_H_