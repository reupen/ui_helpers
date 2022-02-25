#include "stdafx.h"

using namespace uih::literals::spx;

namespace uih {

const RECT rect_null = {0, 0, 0, 0};

void list_view_set_explorer_theme(HWND wnd)
{
    if (mmh::is_windows_vista_or_newer()) {
        ListView_SetExtendedListViewStyleEx(wnd, 0x00010000 | LVS_EX_FULLROWSELECT, 0x00010000 | LVS_EX_FULLROWSELECT);
        if (mmh::is_windows_7_or_newer()) {
            SetWindowTheme(wnd, L"ItemsView", nullptr);
            // SetWindowTheme(ListView_GetHeader (wnd), L"ItemsView", NULL);
        } else {
            SetWindowTheme(wnd, L"Explorer", nullptr);
        }
    }
}

bool are_keyboard_cues_enabled()
{
    BOOL a = true;
    SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &a, 0);
    return a != 0;
}

void tree_view_set_explorer_theme(HWND wnd, bool b_reduce_indent)
{
    if (mmh::is_windows_vista_or_newer()) {
        UINT_PTR stylesex = TVS_EX_DOUBLEBUFFER | TVS_EX_AUTOHSCROLL;
        UINT_PTR styles = NULL;

        SendMessage(wnd, TVM_SETEXTENDEDSTYLE, stylesex, stylesex);
        SetWindowTheme(wnd, L"Explorer", nullptr);
        SetWindowLongPtr(
            wnd, GWL_STYLE, (GetWindowLongPtr(wnd, GWL_STYLE) & ~(TVS_HASLINES /*|TVS_NOHSCROLL*/)) | styles);
        if (b_reduce_indent)
            TreeView_SetIndent(wnd, 0xa);
    }
}

int list_view_insert_column_text(HWND wnd_lv, UINT index, const TCHAR* text, int cx)
{
    LVCOLUMN lvc;
    memset(&lvc, 0, sizeof(LVCOLUMN));
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;

    lvc.pszText = const_cast<TCHAR*>(text);
    lvc.cx = cx;
    return ListView_InsertColumn(wnd_lv, index, &lvc);
}

LRESULT list_view_insert_item_text(
    HWND wnd_lv, UINT item, UINT subitem, const TCHAR* text, bool b_set, LPARAM lp, int image_index)
{
    LVITEM lvi;
    memset(&lvi, 0, sizeof(LVITEM));
    lvi.mask = LVIF_TEXT | (b_set ? 0 : LVIF_PARAM);
    lvi.iItem = item;
    lvi.iSubItem = subitem;
    lvi.pszText = const_cast<TCHAR*>(text);
    lvi.lParam = lp;
    if (image_index != I_IMAGENONE) {
        lvi.mask |= LVIF_IMAGE;
        lvi.iImage = image_index;
    }
    return b_set ? ListView_SetItem(wnd_lv, &lvi) : ListView_InsertItem(wnd_lv, &lvi);
}

LRESULT list_view_insert_item_text(
    HWND wnd_lv, UINT item, UINT subitem, const char* text, bool b_set, LPARAM lp, int image_index)
{
    pfc::stringcvt::string_os_from_utf8 wide(text);
    return list_view_insert_item_text(
        wnd_lv, item, subitem, const_cast<TCHAR*>(wide.get_ptr()), b_set, lp, image_index);
}
HTREEITEM tree_view_insert_item_simple(HWND wnd_tree, const char* sz_text, LPARAM data, DWORD state,
    HTREEITEM ti_parent, HTREEITEM ti_after, bool b_image, UINT image, UINT integral_height)
{
    return tree_view_insert_item_simple(wnd_tree, pfc::stringcvt::string_os_from_utf8(sz_text), data, state, ti_parent,
        ti_after, b_image, image, integral_height);
}

HTREEITEM tree_view_insert_item_simple(HWND wnd_tree, const WCHAR* sz_text, LPARAM data, DWORD state,
    HTREEITEM ti_parent, HTREEITEM ti_after, bool b_image, UINT image, UINT integral_height)
{
    TVINSERTSTRUCT is;
    memset(&is, 0, sizeof(is));
    is.hParent = ti_parent;
    is.hInsertAfter = ti_after;
    is.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE | (b_image ? TVIF_IMAGE | TVIF_SELECTEDIMAGE : NULL)
        | (integral_height > 1 ? TVIF_INTEGRAL : NULL);
    is.item.pszText = const_cast<WCHAR*>(sz_text);
    is.item.state = state;
    is.item.stateMask = state;
    is.item.lParam = data;
    is.item.iImage = image;
    is.item.iSelectedImage = image;
    is.itemex.iIntegral = integral_height;
    return TreeView_InsertItem(wnd_tree, &is);
}

t_size tree_view_get_child_index(HWND wnd_tv, HTREEITEM ti)
{
    HTREEITEM item = ti;
    unsigned n = 0;
    while (item = TreeView_GetPrevSibling(wnd_tv, item))
        n++;
    return n;
}

void tree_view_collapse_other_nodes(HWND wnd, HTREEITEM ti)
{
    auto child = ti;
    do {
        auto sibling = child;
        while ((sibling = TreeView_GetPrevSibling(wnd, sibling))) {
            TreeView_Expand(wnd, sibling, TVE_COLLAPSE);
        }
        sibling = child;
        while ((sibling = TreeView_GetNextSibling(wnd, sibling))) {
            TreeView_Expand(wnd, sibling, TVE_COLLAPSE);
        }
    } while ((child = TreeView_GetParent(wnd, child)));
}

BOOL FileTimeToLocalFileTime2(__in CONST FILETIME* lpFileTime, __out LPFILETIME lpLocalFileTime)
{
    SYSTEMTIME stUTC;
    SYSTEMTIME stLocal;
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
void format_date(uint64_t time, std::basic_string<TCHAR>& str, bool b_convert_to_local)
{
    FILETIME ft1 = filetimestamp_to_FileTime(time);
    FILETIME ft2 = ft1;
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
BOOL set_process_dpi_aware()
{
    using SETPROCESSDPIAWAREPROC = BOOL (WINAPI*)();
    HINSTANCE hinstDll = LoadLibrary(_T("user32.dll"));

    if (hinstDll) {
        auto pSetProcessDPIAware = (SETPROCESSDPIAWAREPROC)GetProcAddress(hinstDll, "SetProcessDPIAware");

        if (pSetProcessDPIAware) {
            return pSetProcessDPIAware();
        }
    }

    return FALSE;
}

SIZE get_system_dpi()
{
    HDC dc = GetDC(nullptr);
    SIZE ret = {GetDeviceCaps(dc, LOGPIXELSX), GetDeviceCaps(dc, LOGPIXELSY)};
    ReleaseDC(nullptr, dc);
    return ret;
}

SIZE get_system_dpi_cached()
{
    static const SIZE size = get_system_dpi();
    return size;
}

int scale_dpi_value(int value, unsigned original_dpi)
{
    return MulDiv(value, get_system_dpi_cached().cx, original_dpi);
}

HRESULT get_comctl32_version(DLLVERSIONINFO2& p_dvi)
{
    static bool have_version = false;
    static HRESULT rv = E_FAIL;

    static DLLVERSIONINFO2 g_dvi;

    if (!have_version) {
        HINSTANCE hinstDll = LoadLibrary(_T("comctl32.dll"));

        if (hinstDll) {
            if (!have_version) {
                auto pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");

                if (pDllGetVersion) {
                    memset(&g_dvi, 0, sizeof(DLLVERSIONINFO2));
                    g_dvi.info1.cbSize = sizeof(DLLVERSIONINFO2);

                    rv = (*pDllGetVersion)(&g_dvi.info1);

                    if (FAILED(rv)) {
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

BOOL run_action(DWORD action, NOTIFYICONDATA* data)
{
    if (Shell_NotifyIcon(action, data))
        return TRUE;
    if (action == NIM_MODIFY) {
        if (Shell_NotifyIcon(NIM_ADD, data))
            return TRUE;
    }
    return FALSE;
}

BOOL shell_notify_icon(DWORD action, HWND wnd, UINT id, UINT version, UINT callbackmsg, HICON icon, const char* tip)
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = NOTIFYICONDATA_V2_SIZE;
    nid.hWnd = wnd;
    nid.uID = id;
    nid.uFlags = 0;
    if (action & NIM_SETVERSION)
        nid.uVersion = version;
    if (callbackmsg) {
        nid.uFlags |= NIF_MESSAGE;
        nid.uCallbackMessage = callbackmsg;
    }
    if (icon) {
        nid.uFlags |= NIF_ICON;
        nid.hIcon = icon;
    }
    if (tip) {
        nid.uFlags |= NIF_TIP;
        _tcsncpy_s(nid.szTip, pfc::stringcvt::string_os_from_utf8(tip), tabsize(nid.szTip) - 1);
    }

    return run_action(action, &nid);
}

BOOL shell_notify_icon_ex(DWORD action, HWND wnd, UINT id, UINT callbackmsg, HICON icon, const char* tip,
    const char* balloon_title, const char* balloon_msg)
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = NOTIFYICONDATA_V2_SIZE;
    nid.hWnd = wnd;
    nid.uID = id;
    if (callbackmsg) {
        nid.uFlags |= NIF_MESSAGE;
        nid.uCallbackMessage = callbackmsg;
    }
    if (icon) {
        nid.uFlags |= NIF_ICON;
        nid.hIcon = icon;
    }
    if (tip) {
        nid.uFlags |= NIF_TIP;
        _tcsncpy_s(nid.szTip, pfc::stringcvt::string_os_from_utf8(tip), tabsize(nid.szTip) - 1);
    }

    nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    // if (balloon_title || balloon_msg)
    {
        nid.uFlags |= NIF_INFO;
        if (balloon_title)
            _tcsncpy_s(
                nid.szInfoTitle, pfc::stringcvt::string_os_from_utf8(balloon_title), tabsize(nid.szInfoTitle) - 1);
        if (balloon_msg)
            _tcsncpy_s(nid.szInfo, pfc::stringcvt::string_os_from_utf8(balloon_msg), tabsize(nid.szInfo) - 1);
    }
    return run_action(action, reinterpret_cast<NOTIFYICONDATA*>(&nid));
}
int combo_box_add_string_data(HWND wnd, const TCHAR* str, LPARAM data)
{
    int index = ComboBox_AddString(wnd, str);
    ComboBox_SetItemData(wnd, index, data);
    return index;
}

int combo_box_find_item_by_data(HWND wnd, t_size id)
{
    int count = ComboBox_GetCount(wnd);
    for (int i = 0; i < count; i++)
        if (ComboBox_GetItemData(wnd, i) == id)
            return i;
    return -1;
}

void rebar_show_all_bands(HWND wnd)
{
    const UINT count = static_cast<UINT>(SendMessage(wnd, RB_GETBANDCOUNT, 0, 0));
    unsigned n;
    for (n = 0; n < count; n++) {
        SendMessage(wnd, RB_SHOWBAND, n, TRUE);
    }
}

void handle_modern_background_paint(HWND wnd, HWND wnd_button)
{
    const auto dpi = dpi::get_dpi_for_window(wnd);
    const auto padding_height = dpi::scale_value(dpi, 11);
    const auto line_height = dpi ::scale_value(dpi, 1);

    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(wnd, &ps);
    if (dc) {
        RECT rc_client;
        RECT rc_button;
        GetClientRect(wnd, &rc_client);
        RECT rc_fill = rc_client;
        if (wnd_button) {
            GetWindowRect(wnd_button, &rc_button);
            MapWindowPoints(HWND_DESKTOP, wnd, reinterpret_cast<LPPOINT>(&rc_button), 2);
            rc_fill.bottom = rc_button.top - padding_height - line_height / 2;
        }
        FillRect(dc, &rc_fill, GetSysColorBrush(COLOR_WINDOW));
        if (wnd_button) {
            rc_fill.top = rc_fill.bottom;
            rc_fill.bottom += line_height;
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
    } while ((wnd = GetWindow(wnd, GW_HWNDNEXT)));
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
    const auto buffer_size = GetWindowTextLength(wnd) + 1;
    if (buffer_size <= 0)
        return false;

    pfc::array_staticsize_t<wchar_t> buffer(buffer_size);
    pfc::fill_array_t(buffer, 0);
    const auto chars_written = GetWindowText(wnd, buffer.get_ptr(), buffer_size);
    if (chars_written <= 0)
        return false;

    const auto utf8_size = pfc::stringcvt::estimate_wide_to_utf8(buffer.get_ptr(), chars_written);
    auto utf8_buffer = pfc::string_buffer(out, utf8_size);
    pfc::stringcvt::convert_wide_to_utf8(utf8_buffer, utf8_size, buffer.get_ptr(), chars_written);
    return true;
}

void set_window_font(HWND wnd, HFONT font, bool redraw)
{
    SendMessage(wnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), MAKELPARAM(redraw, 0));
}

HFONT get_window_font(HWND wnd)
{
    return reinterpret_cast<HFONT>(SendMessage(wnd, WM_GETFONT, 0, 0));
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

std::optional<std::vector<uint8_t>> get_clipboard_data(CLIPFORMAT format)
{
    if (!OpenClipboard(nullptr))
        return {};

    auto _ = gsl::finally([] { CloseClipboard(); });

    const auto global_mem = GetClipboardData(format);

    if (!global_mem)
        return {};

    const uint8_t* data = reinterpret_cast<uint8_t*>(GlobalLock(global_mem));

    if (!data)
        return {};

    const auto size = GlobalSize(global_mem);
    std::vector<uint8_t> read_data(data, data + size);

    GlobalUnlock(global_mem);

    return {std::move(read_data)};
}

BOOL tooltip_add_tool(HWND wnd, const LPTOOLINFO pti)
{
    return static_cast<BOOL>(SendMessage(wnd, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(pti)));
}

BOOL tooltip_update_tip_text(HWND wnd, const LPTOOLINFO pti)
{
    return static_cast<BOOL>(SendMessage(wnd, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(pti)));
}

BOOL header_set_item_text(HWND wnd, int n, const wchar_t* text)
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
            const auto flags = SendMessage(wnd_next, WM_GETDLGCODE, 0, 0);
            if (flags & DLGC_HASSETSEL)
                SendMessage(wnd_next, EM_SETSEL, 0, -1);
            SetFocus(wnd_next);

            wnd_focused = wnd_next;
        }
    }
    return wnd_focused;
}

bool set_clipboard_text(const char* text, HWND wnd)
{
    const pfc::stringcvt::string_wide_from_utf8 text_utf16(text);
    return set_clipboard_data<const wchar_t>(CF_UNICODETEXT, {text_utf16.get_ptr(), text_utf16.length() + 1}, wnd);
}

} // namespace uih
