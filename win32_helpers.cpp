#include "stdafx.h"

class param_utf16_from_utf8 : public pfc::stringcvt::string_wide_from_utf8 {
    bool is_null;
    WORD low_word;
public:
    param_utf16_from_utf8(const char * p) :
        pfc::stringcvt::string_wide_from_utf8(p && HIWORD((DWORD)p) != 0 ? p : ""),
        is_null(p == nullptr),
        low_word(HIWORD((DWORD)p) == 0 ? LOWORD((DWORD)p) : 0)
    {}
    inline operator const WCHAR *()
    {
        return get_ptr();
    }
    const WCHAR * get_ptr()
    {
        return low_word ? (const WCHAR*)(DWORD)low_word : is_null ? nullptr : pfc::stringcvt::string_wide_from_utf8::get_ptr();
    }

};

namespace uih {
    const RECT rect_null = { 0,0,0,0 };

    void SetListViewWindowExplorerTheme(HWND wnd)
    {
        if (mmh::osversion::is_windows_vista_or_newer())
        {
            ListView_SetExtendedListViewStyleEx(wnd, 0x00010000 | LVS_EX_FULLROWSELECT, 0x00010000 | LVS_EX_FULLROWSELECT);
            if (mmh::osversion::is_windows_7_or_newer())
            {
                SetWindowTheme(wnd, L"ItemsView", nullptr);
                //SetWindowTheme(ListView_GetHeader (wnd), L"ItemsView", NULL);
            }
            else
            {
                SetWindowTheme(wnd, L"Explorer", nullptr);
            }
        }
    }

    bool GetKeyboardCuesEnabled()
    {
        BOOL a = true;
        SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &a, 0);
        return a != 0;
    }


#define TVS_EX_UNKNOWN               0x0001 //
#define TVS_EX_MULTISELECT          0x0002
#define TVS_EX_DOUBLEBUFFER         0x0004 //
#define TVS_EX_NOINDENTSTATE        0x0008
#define TVS_EX_RICHTOOLTIP          0x0010 //
#define TVS_EX_AUTOHSCROLL          0x0020 //
#define TVS_EX_FADEINOUTEXPANDOS    0x0040 //
#define TVS_EX_PARTIALCHECKBOXES    0x0080
#define TVS_EX_EXCLUSIONCHECKBOXES  0x0100
#define TVS_EX_DIMMEDCHECKBOXES     0x0200
#define TVS_EX_DRAWIMAGEASYNC       0x0400 //

    void SetTreeViewWindowExplorerTheme(HWND wnd, bool b_reduce_indent)
    {
        if (mmh::osversion::is_windows_vista_or_newer())
        {
            UINT_PTR stylesex = /*TVS_EX_FADEINOUTEXPANDOS|*/TVS_EX_DOUBLEBUFFER | TVS_EX_AUTOHSCROLL;
            UINT_PTR styles = NULL;//TVS_TRACKSELECT;

            SendMessage(wnd, TV_FIRST + 44, stylesex, stylesex);
            SetWindowTheme(wnd, L"Explorer", nullptr);
            SetWindowLongPtr(wnd, GWL_STYLE, (GetWindowLongPtr(wnd, GWL_STYLE) & ~(TVS_HASLINES/*|TVS_NOHSCROLL*/)) | styles);
            if (b_reduce_indent)
                TreeView_SetIndent(wnd, 0xa);
        }

    }

    void RemoveTreeViewWindowExplorerTheme(HWND wnd)
    {
        if (mmh::osversion::is_windows_vista_or_newer())
        {
            UINT_PTR stylesex = /*TVS_EX_FADEINOUTEXPANDOS|*/TVS_EX_DOUBLEBUFFER;
            UINT_PTR styles = NULL;//TVS_TRACKSELECT;

            SendMessage(wnd, TV_FIRST + 44, stylesex, NULL);
            SetWindowTheme(wnd, L"", nullptr);
            SetWindowLongPtr(wnd, GWL_STYLE, (GetWindowLongPtr(wnd, GWL_STYLE) | TVS_HASLINES));
        }

    }

    int ListView_InsertColumnText(HWND wnd_lv, UINT index, const TCHAR * text, int cx)
    {
        LVCOLUMN lvc;
        memset(&lvc, 0, sizeof(LVCOLUMN));
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;

        lvc.pszText = const_cast<TCHAR*>(text);
        lvc.cx = cx;
        return ListView_InsertColumn(wnd_lv, index, &lvc);
    }

    LRESULT ListView_InsertItemText(HWND wnd_lv, UINT item, UINT subitem, const TCHAR * text, bool b_set, LPARAM lp, int image_index)
    {
        LVITEM lvi;
        memset(&lvi, 0, sizeof(LVITEM));
        lvi.mask = LVIF_TEXT | (b_set ? 0 : LVIF_PARAM);
        lvi.iItem = item;
        lvi.iSubItem = subitem;
        lvi.pszText = const_cast<TCHAR*>(text);
        lvi.lParam = lp;
        if (image_index != I_IMAGENONE)
        {
            lvi.mask |= LVIF_IMAGE;
            lvi.iImage = image_index;
        }
        return b_set ? ListView_SetItem(wnd_lv, &lvi) : ListView_InsertItem(wnd_lv, &lvi);
    }

    LRESULT ListView_InsertItemText(HWND wnd_lv, UINT item, UINT subitem, const char * text, bool b_set, LPARAM lp, int image_index)
    {
        pfc::stringcvt::string_os_from_utf8 wide(text);
        return ListView_InsertItemText(wnd_lv, item, subitem, const_cast<TCHAR*>(wide.get_ptr()), b_set, lp, image_index);
    }
    HTREEITEM TreeView_InsertItemSimple(HWND wnd_tree, const char * sz_text, LPARAM data, DWORD state, HTREEITEM ti_parent, HTREEITEM ti_after, bool b_image, UINT image, UINT integral_height)
    {
        return TreeView_InsertItemSimple(wnd_tree, pfc::stringcvt::string_os_from_utf8(sz_text), data, state, ti_parent, ti_after, b_image, image, integral_height);
    }

    HTREEITEM TreeView_InsertItemSimple(HWND wnd_tree, const WCHAR * sz_text, LPARAM data, DWORD state, HTREEITEM ti_parent, HTREEITEM ti_after, bool b_image, UINT image, UINT integral_height)
    {
        TVINSERTSTRUCT is;
        memset(&is, 0, sizeof(is));
        is.hParent = ti_parent;
        is.hInsertAfter = ti_after;
        is.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE | (b_image ? TVIF_IMAGE | TVIF_SELECTEDIMAGE : NULL) | (integral_height > 1 ? TVIF_INTEGRAL : NULL);
        is.item.pszText = const_cast<WCHAR*>(sz_text);
        is.item.state = state;
        is.item.stateMask = state;
        is.item.lParam = data;
        is.item.iImage = image;
        is.item.iSelectedImage = image;
        is.itemex.iIntegral = integral_height;
        return TreeView_InsertItem(wnd_tree, &is);
    }

    t_size TreeView_GetChildIndex(HWND wnd_tv, HTREEITEM ti)
    {
        HTREEITEM item = ti;
        unsigned n = 0;
        while (item = TreeView_GetPrevSibling(wnd_tv, item))
            n++;
        return n;
    }

    BOOL
        FileTimeToLocalFileTime2(
            __in  CONST FILETIME *lpFileTime,
            __out LPFILETIME lpLocalFileTime
            )
    {
        SYSTEMTIME stUTC, stLocal;
        memset(&stUTC, 0, sizeof(stUTC));
        memset(&stLocal, 0, sizeof(stLocal));

        FileTimeToSystemTime(lpFileTime, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
        return SystemTimeToFileTime(&stLocal, lpLocalFileTime);
    }
    FILETIME filetimestamp_to_FileTime(uint64_t time)
    {
        FILETIME ret;
        ret.dwLowDateTime = (DWORD)(time & 0xFFFFFFFF);
        ret.dwHighDateTime = (DWORD)(time >> 32);
        return ret;
    }
    void FormatDate(uint64_t time, std::basic_string<TCHAR> & str, bool b_convert_to_local)
    {
        FILETIME ft1 = filetimestamp_to_FileTime(time), ft2 = ft1;
        if (b_convert_to_local)
            FileTimeToLocalFileTime2(&ft1, &ft2);
        SYSTEMTIME st;
        FileTimeToSystemTime(&ft2, &st);
        int size = GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, nullptr, NULL);
        int size2 = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, nullptr, NULL);
        pfc::array_t<TCHAR> buf, buf2;
        buf.set_size(size);
        buf2.set_size(size2);
        GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, buf.get_ptr(), size);
        GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, buf2.get_ptr(), size2);
        str = _T("");
        str += buf.get_ptr();
        str += _T(" ");
        str += buf2.get_ptr();
    }
    BOOL SetProcessDpiAware()
    {
        typedef BOOL(WINAPI* SETPROCESSDPIAWAREPROC)();
        HINSTANCE hinstDll = LoadLibrary(_T("user32.dll"));

        if (hinstDll)
        {
            SETPROCESSDPIAWAREPROC pSetProcessDPIAware = (SETPROCESSDPIAWAREPROC)GetProcAddress(hinstDll, "SetProcessDPIAware");

            if (pSetProcessDPIAware)
            {
                return pSetProcessDPIAware();
            }
        }

        return FALSE;
    }

    SIZE get_system_dpi()
    {
        HDC dc = GetDC(nullptr);
        SIZE ret = { GetDeviceCaps(dc, LOGPIXELSX), GetDeviceCaps(dc, LOGPIXELSY) };
        ReleaseDC(nullptr, dc);
        return ret;
    }

    SIZE GetSystemDpiCached()
    {
        static const SIZE size = get_system_dpi();
        return size;
    }

    int ScaleDpiValue(int value, unsigned original_dpi)
    {
        return MulDiv(value, GetSystemDpiCached().cx, original_dpi);
    }

    HRESULT GetComCtl32Version(DLLVERSIONINFO2 & p_dvi)
    {
        static bool have_version = false;
        static HRESULT rv = E_FAIL;

        static DLLVERSIONINFO2 g_dvi;

        if (!have_version)
        {
            HINSTANCE hinstDll = LoadLibrary(_T("comctl32.dll"));

            if (hinstDll)
            {
                if (!have_version)
                {
                    DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");

                    if (pDllGetVersion)
                    {

                        memset(&g_dvi, 0, sizeof(DLLVERSIONINFO2));
                        g_dvi.info1.cbSize = sizeof(DLLVERSIONINFO2);

                        rv = (*pDllGetVersion)(&g_dvi.info1);

                        if (FAILED(rv))
                        {
                            memset(&g_dvi, 0, sizeof(DLLVERSIONINFO));
                            g_dvi.info1.cbSize = sizeof(DLLVERSIONINFO);

                            rv = (*pDllGetVersion)(&g_dvi.info1);
                        }
                    }
                    have_version = true;
                }
                FreeLibrary(hinstDll);
            }
        }
        p_dvi = g_dvi;
        return rv;
    }

    BOOL ShellNotifyIconSimple(DWORD dwMessage, HWND wnd, UINT id, UINT callbackmsg, HICON icon,
        const char * tip, const char * balloon_title, const char * balloon_msg)
    {
        //param_utf16_from_utf8 wtip(tip), wbtitle(balloon_title), wbmsg(balloon_msg);
        NOTIFYICONDATA nid;
        memset(&nid, 0, sizeof(nid));

        nid.cbSize = NOTIFYICONDATA_V2_SIZE;
        nid.hWnd = wnd;
        nid.uID = id;
        nid.uCallbackMessage = callbackmsg;
        nid.hIcon = icon;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | (balloon_msg ? NIF_INFO : NULL);
        if (tip)
            wcscpy_s(nid.szTip, pfc::stringcvt::string_wide_from_utf8(tip).get_ptr());
        if (balloon_title)
            wcscpy_s(nid.szInfoTitle, pfc::stringcvt::string_wide_from_utf8(balloon_title).get_ptr());
        if (balloon_msg)
            wcscpy_s(nid.szInfo, pfc::stringcvt::string_wide_from_utf8(balloon_msg).get_ptr());
        nid.uTimeout = 10000;
        nid.dwInfoFlags = NIIF_INFO;

        return Shell_NotifyIcon(dwMessage, &nid);


    }

    BOOL run_action(DWORD action, NOTIFYICONDATA * data)
    {
        if (Shell_NotifyIcon(action, data)) return TRUE;
        if (action == NIM_MODIFY)
        {
            if (Shell_NotifyIcon(NIM_ADD, data)) return TRUE;
        }
        return FALSE;
    }


    BOOL ShellNotifyIcon(DWORD action, HWND wnd, UINT id, UINT version, UINT callbackmsg, HICON icon, const char * tip)
    {
        NOTIFYICONDATA nid;
        memset(&nid, 0, sizeof(nid));
        nid.cbSize = NOTIFYICONDATA_V2_SIZE;
        nid.hWnd = wnd;
        nid.uID = id;
        nid.uFlags = 0;
        if (action & NIM_SETVERSION)
            nid.uVersion = version;
        if (callbackmsg)
        {
            nid.uFlags |= NIF_MESSAGE;
            nid.uCallbackMessage = callbackmsg;
        }
        if (icon)
        {
            nid.uFlags |= NIF_ICON;
            nid.hIcon = icon;
        }
        if (tip)
        {
            nid.uFlags |= NIF_TIP;
            _tcsncpy_s(nid.szTip, pfc::stringcvt::string_os_from_utf8(tip), tabsize(nid.szTip) - 1);
        }

        return run_action(action, &nid);
    }

    BOOL ShellNotifyIconEx(DWORD action, HWND wnd, UINT id, UINT callbackmsg, HICON icon, const char * tip, const char * balloon_title, const char * balloon_msg)
    {

        NOTIFYICONDATA nid;
        memset(&nid, 0, sizeof(nid));
        nid.cbSize = NOTIFYICONDATA_V2_SIZE;
        nid.hWnd = wnd;
        nid.uID = id;
        if (callbackmsg)
        {
            nid.uFlags |= NIF_MESSAGE;
            nid.uCallbackMessage = callbackmsg;
        }
        if (icon)
        {
            nid.uFlags |= NIF_ICON;
            nid.hIcon = icon;
        }
        if (tip)
        {
            nid.uFlags |= NIF_TIP;
            _tcsncpy_s(nid.szTip, pfc::stringcvt::string_os_from_utf8(tip), tabsize(nid.szTip) - 1);
        }

        nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
        //if (balloon_title || balloon_msg)
        {
            nid.uFlags |= NIF_INFO;
            if (balloon_title) _tcsncpy_s(nid.szInfoTitle, pfc::stringcvt::string_os_from_utf8(balloon_title), tabsize(nid.szInfoTitle) - 1);
            if (balloon_msg) _tcsncpy_s(nid.szInfo, pfc::stringcvt::string_os_from_utf8(balloon_msg), tabsize(nid.szInfo) - 1);
        }
        return run_action(action, reinterpret_cast<NOTIFYICONDATA*>(&nid));


    }
    int ComboBox_AddStringData(HWND wnd, const TCHAR * str, LPARAM data)
    {
        int index = ComboBox_AddString(wnd, str);
        ComboBox_SetItemData(wnd, index, data);
        return index;
    }

    void RegisterShellHookWindowHelper(HWND wnd)
    {
        typedef BOOL(WINAPI * RegisterShellHookWindowProc)(HWND);
        HINSTANCE inst = LoadLibrary(L"user32.dll");
        if (inst)
        {
            RegisterShellHookWindowProc pRegisterShellHookWindow = (RegisterShellHookWindowProc)GetProcAddress(inst, "RegisterShellHookWindow");
            if (pRegisterShellHookWindow)
                pRegisterShellHookWindow(wnd);
            FreeLibrary(inst);
        }
    }

    void DeregisterShellHookWindowHelper(HWND wnd)
    {
        typedef BOOL(WINAPI * DeregisterShellHookWindowProc)(HWND);
        HINSTANCE inst = LoadLibrary(L"user32.dll");
        if (inst)
        {
            DeregisterShellHookWindowProc pDeregisterShellHookWindow = (DeregisterShellHookWindowProc)GetProcAddress(inst, "DeregisterShellHookWindow");
            if (pDeregisterShellHookWindow)
                pDeregisterShellHookWindow(wnd);
            FreeLibrary(inst);
        }
    }

    int ComboBox_FindItemByData(HWND wnd, t_size id)
    {
        t_size i, count = ComboBox_GetCount(wnd);
        for (i = 0; i < count; i++)
            if (ComboBox_GetItemData(wnd, i) == id)
                return i;
        return -1;
    }

    int Rebar_FindItemById(HWND wnd, unsigned id)
    {
        /* Avoid RB_IDTOINDEX for backwards compatibility */
        REBARBANDINFO  rbbi;
        memset(&rbbi, 0, sizeof(rbbi));
        rbbi.cbSize = sizeof(rbbi);
        rbbi.fMask = RBBIM_ID;

        UINT count = SendMessage(wnd, RB_GETBANDCOUNT, 0, 0);
        unsigned n;
        for (n = 0; n < count; n++)
        {
            SendMessage(wnd, RB_GETBANDINFO, n, (long)&rbbi);
            if (rbbi.wID == id) return n;
        }
        return -1;
    }

    void Rebar_ShowAllBands(HWND wnd)
    {
        UINT count = SendMessage(wnd, RB_GETBANDCOUNT, 0, 0);
        unsigned n;
        for (n = 0; n < count; n++)
        {
            SendMessage(wnd, RB_SHOWBAND, n, TRUE);
        }
    }

    void HandleModernBackgroundPaint(HWND wnd, HWND wnd_button)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(wnd, &ps);
        if (dc)
        {
            RECT rc_client, rc_button;
            GetClientRect(wnd, &rc_client);
            RECT rc_fill = rc_client;
            if (wnd_button)
            {
                GetWindowRect(wnd_button, &rc_button);
                rc_fill.bottom -= RECT_CY(rc_button) + 11;
                rc_fill.bottom -= 11;
            }
            FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_WINDOW));
            if (wnd_button)
            {
                rc_fill.top = rc_fill.bottom;
                rc_fill.bottom += 1;
                FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DLIGHT));
            }
            rc_fill.top = rc_fill.bottom;
            rc_fill.bottom = rc_client.bottom;
            FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_3DFACE));
            EndPaint(wnd, &ps);
        }
    }

    void send_message_to_direct_children(HWND wnd_parent, UINT msg, WPARAM wp, LPARAM lp)
    {
        HWND wnd = GetWindow(wnd_parent, GW_CHILD);
        if (!wnd)
            return;

        do {
            SendMessage(wnd, msg, wp, lp);
        }
        while ((wnd = GetWindow(wnd, GW_HWNDNEXT)));
    }

    RECT get_relative_rect(HWND wnd, HWND wnd_parent)
    {
        RECT rc{0};
        GetWindowRect(wnd, &rc);
        MapWindowPoints(HWND_DESKTOP, wnd_parent, reinterpret_cast<LPPOINT>(&rc), 2);
        return rc;
    }

    bool get_window_text(HWND wnd, pfc::string_base& out)
    {
        auto buffer_size = GetWindowTextLength(wnd);
        if (buffer_size <= 0)
            return false;

        pfc::array_staticsize_t<wchar_t> buffer(buffer_size);
        pfc::fill_array_t(buffer, 0);
        auto chars_written = GetWindowText(wnd, buffer.get_ptr(), buffer_size);
        if (chars_written <= 0)
            return false;

        auto utf8_size = pfc::stringcvt::estimate_wide_to_utf8(buffer.get_ptr(), chars_written);
        auto utf8_buffer = pfc::string_buffer(out, utf8_size);
        pfc::stringcvt::convert_wide_to_utf8(utf8_buffer, utf8_size, buffer.get_ptr(), chars_written);
        return true;
    }

    HFONT create_icon_font()
    {
        LOGFONT lf;
        memset(&lf, 0, sizeof(LOGFONT));
        SystemParametersInfo(SPI_GETICONTITLELOGFONT, 0, &lf, 0);

        return CreateFontIndirectW(&lf);
    }

    int get_font_height(HFONT font)
    {
        HDC dc{GetDC(nullptr)};
        HFONT font_old = SelectFont(dc, font);
        const auto font_height = get_dc_font_height(dc);
        SelectFont(dc, font_old);
        ReleaseDC(nullptr, dc);
        return font_height;
    }

    int get_dc_font_height(HDC dc)
    {
        TEXTMETRIC tm{};
        GetTextMetrics(dc, &tm);
        // Default mapping mode is MM_TEXT, so we don't map the units
        return tm.tmHeight > 1 ? tm.tmHeight : 1;
    }

    BOOL tooltip_add_tool(HWND wnd, const LPTOOLINFO pti)
    {
        return static_cast<BOOL>(SendMessage(wnd, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(pti)));
    }

    BOOL tooltip_update_tip_text(HWND wnd, const LPTOOLINFO pti)
    {
        return static_cast<BOOL>(SendMessage(wnd, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(pti)));
    }

    BOOL header_set_item_text(HWND wnd, int n, const wchar_t * text)
    {
        HDITEM hdi;
        memset(&hdi, 0, sizeof(hdi));
        hdi.mask = HDI_TEXT;
        hdi.cchTextMax = NULL;
        hdi.pszText = const_cast<wchar_t*>(text);
        return Header_SetItem(wnd, n, &hdi);
    }

    BOOL header_set_item_width(HWND wnd, int n, UINT cx)
    {
        HDITEM hdi;
        memset(&hdi, 0, sizeof(hdi));
        hdi.mask = HDI_WIDTH;
        hdi.cxy = cx;
        return Header_SetItem(wnd, n, &hdi);
    }

    HWND handle_tab_down(HWND wnd)
    {
        HWND wnd_focused{nullptr};
        HWND wnd_temp{GetAncestor(wnd, GA_ROOT)};

        if (wnd_temp) {
            HWND wnd_next = GetNextDlgTabItem(wnd_temp, wnd, (GetKeyState(VK_SHIFT) & KF_UP) ? TRUE : FALSE);
            if (wnd_next && wnd_next != wnd) {
                unsigned flags = SendMessage(wnd_next, WM_GETDLGCODE, 0, 0);
                if (flags & DLGC_HASSETSEL) SendMessage(wnd_next, EM_SETSEL, 0, -1);
                SetFocus(wnd_next);

                wnd_focused = wnd_next;
            }
        }
        return wnd_focused;
    }

    bool set_clipboard_text(const char* text, HWND wnd)
    {
        pfc::stringcvt::string_wide_from_utf8 wstr(text);

        if (!OpenClipboard(nullptr))
            return false;

        auto succeeded = false;
        const auto buffer_size = wstr.length() * sizeof(WCHAR) + 1;
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, buffer_size);
        if (mem) {
            auto buffer = GlobalLock(mem);

            if (buffer) {
                memcpy(buffer, wstr.get_ptr(), buffer_size);
                GlobalUnlock(mem);
                if (EmptyClipboard())
                    succeeded = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
            }
        }
        if (!succeeded && mem)
            GlobalFree(mem);
        CloseClipboard();
        return succeeded;
    }
}
