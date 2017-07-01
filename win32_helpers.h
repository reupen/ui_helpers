#ifndef _UI_WIN32_HELPERS_H_
#define _UI_WIN32_HELPERS_H_

namespace uih {
    extern const RECT rect_null;

    struct KeyboardLParam {
        /** Specifies the repeat count. The value is the number of times the keystroke is repeated as a result of the user's holding down the key. */
        LPARAM repeat_count : 16;
        /** Specifies the scan code. The value depends on the OEM. */
        LPARAM scan_code : 8;
        /** Specifies whether the key is an extended key, such as a function key or a key on the numeric keypad. The value is 1 if the key is an extended key; otherwise, it is 0. */
        LPARAM extended_key : 1;
        /** Reserved. */
        LPARAM reserved : 4;
        /** Specifies the context code. The value is 1 if the ALT key is down; otherwise, it is 0.*/
        LPARAM context_code : 1;
        /** Specifies the previous key state. The value is 1 if the key is down before the message is sent; it is 0 if the key is up.*/
        LPARAM previous_key_state : 1;
        /** Specifies the transition state. The value is 0 if the key is being pressed and 1 if it is being released. */
        LPARAM transition_code : 1;
    };

    inline KeyboardLParam& GetKeyboardLParam(LPARAM& lp)
    {
        return reinterpret_cast<KeyboardLParam&>(lp);
    }

    void SetListViewWindowExplorerTheme(HWND wnd);
    void SetTreeViewWindowExplorerTheme(HWND wnd, bool b_reduce_indent = false);
    void RemoveTreeViewWindowExplorerTheme(HWND wnd);
    bool GetKeyboardCuesEnabled();

    void FormatDate(uint64_t time, std::basic_string<TCHAR> & str, bool b_convert_to_local = false);
    HRESULT GetComCtl32Version(DLLVERSIONINFO2 & p_dvi);

    int ListView_InsertColumnText(HWND wnd_lv, UINT index, const TCHAR * text, int cx);
    LRESULT ListView_InsertItemText(HWND wnd_lv, UINT item, UINT subitem, const TCHAR * text, bool b_set = false, LPARAM lp = 0, int image_index = I_IMAGENONE);
    LRESULT ListView_InsertItemText(HWND wnd_lv, UINT item, UINT subitem, const char * text, bool b_set = false, LPARAM lp = 0, int image_index = I_IMAGENONE);

    HTREEITEM TreeView_InsertItemSimple(HWND wnd_tree, const char * sz_text, LPARAM data, DWORD state = TVIS_EXPANDED, HTREEITEM ti_parent = TVI_ROOT, HTREEITEM ti_after = TVI_LAST, bool b_image = false, UINT image = NULL, UINT integral_height = 1);
    HTREEITEM TreeView_InsertItemSimple(HWND wnd_tree, const WCHAR * sz_text, LPARAM data, DWORD state = TVIS_EXPANDED, HTREEITEM ti_parent = TVI_ROOT, HTREEITEM ti_after = TVI_LAST, bool b_image = false, UINT image = NULL, UINT integral_height = 1);

    t_size TreeView_GetChildIndex(HWND wnd_tv, HTREEITEM ti);

    BOOL ShellNotifyIconSimple(DWORD dwMessage, HWND wnd, UINT id, UINT callbackmsg, HICON icon,
        const char * tip, const char * balloon_title = nullptr, const char * balloon_msg = nullptr);

    BOOL ShellNotifyIcon(DWORD action, HWND wnd, UINT id, UINT version, UINT callbackmsg, HICON icon, const char * tip);
    BOOL ShellNotifyIconEx(DWORD action, HWND wnd, UINT id, UINT callbackmsg, HICON icon, const char * tip, const char * balloon_title, const char * balloon_msg);
    int ComboBox_AddStringData(HWND wnd, const TCHAR * str, LPARAM data);

    void RegisterShellHookWindowHelper(HWND wnd);
    void DeregisterShellHookWindowHelper(HWND wnd);

    int ComboBox_FindItemByData(HWND wnd, t_size id);

    int Rebar_FindItemById(HWND wnd, unsigned id);
    void Rebar_ShowAllBands(HWND wnd);

    BOOL SetProcessDpiAware();

    // This is cached as system-DPI changes take effect at next log on. 
    // Per monitor-DPI changes can occur more frequently for per monitor-DPI aware applications.
    // At present, though, per-monitor DPI support is not declared by the foobar2000.exe manifest.
    // See https://msdn.microsoft.com/en-gb/library/windows/desktop/dn469266%28v=vs.85%29.aspx
    SIZE GetSystemDpiCached();

    int ScaleDpiValue(int value, unsigned original_dpi = USER_DEFAULT_SCREEN_DPI);

    void HandleModernBackgroundPaint(HWND wnd, HWND wnd_button);

    void send_message_to_direct_children(HWND wnd_parent, UINT msg, WPARAM wp, LPARAM lp);

    /**
     * \brief Gets window rect of a window using the co-ordinate space of another window
     * \param wnd           Window to retrieve the window rect of
     * \param wnd_parent    Window whose co-ordinate space should be used
     * \return              Window rect of wnd in wnd_parent co-ordinate space
     */
    RECT get_relative_rect(HWND wnd, HWND wnd_parent);
    bool get_window_text(HWND wnd, pfc::string_base& out);

    HFONT create_icon_font();
    int get_font_height(HFONT font);
    int get_dc_font_height(HDC dc);

    BOOL tooltip_add_tool(HWND wnd, const LPTOOLINFO pti);
    BOOL tooltip_update_tip_text(HWND wnd, const LPTOOLINFO pti);

    BOOL header_set_item_text(HWND wnd, int n, const wchar_t* text);
    BOOL header_set_item_width(HWND wnd, int n, UINT cx);

    HWND handle_tab_down(HWND wnd);
    bool set_clipboard_text(const char* text, HWND wnd = nullptr);

    template <typename TInteger>
    class IntegerAndDpi {
    public:
        using Type = IntegerAndDpi<TInteger>;

        TInteger value;
        uint32_t dpi;

        operator TInteger () const { return getScaledValue(); }
        Type & operator = (TInteger value) { set(value);  return *this; }

        void set(TInteger _value, uint32_t _dpi = GetSystemDpiCached().cx)
        {
            value = _value;
            dpi = _dpi;
        }
        TInteger getScaledValue() const { return ScaleDpiValue(value, dpi); };

        IntegerAndDpi(TInteger _value = NULL, uint32_t _dpi = USER_DEFAULT_SCREEN_DPI) : value(_value), dpi(_dpi) {};
    };
}

class DisableRedrawing
{
    bool m_active, m_disable_invalidate, m_disable_update;
    HWND m_wnd;
public:
    void reenable_redrawing()
    {
        if (m_active)
        {
            SendMessage(m_wnd, WM_SETREDRAW, TRUE, 0);
            if (!m_disable_invalidate ||!m_disable_update)
                RedrawWindow(m_wnd, nullptr, nullptr, (m_disable_invalidate?NULL:RDW_INVALIDATE)|(m_disable_update?NULL:RDW_UPDATENOW));
            m_active=false;
        }
    }
    DisableRedrawing(HWND wnd, bool b_disable_invalidate = false, bool b_disable_update = false)
        : m_active(false), m_disable_invalidate(b_disable_invalidate), m_disable_update(b_disable_update), m_wnd(wnd)
    {
        if (IsWindowVisible(m_wnd))
        {
            SendMessage(m_wnd, WM_SETREDRAW, FALSE, 0);
            m_active = true;
        }
    }
    ~DisableRedrawing() {reenable_redrawing();}

};

#endif //_UI_WIN32_HELPERS_H_