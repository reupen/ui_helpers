#include "../stdafx.h"

namespace uih {

void ListView::activate_inline_editing(t_size column_start)
{
    unsigned count = m_columns.get_count();
    if (count) {
        t_size focus = get_focus_item();
        if (focus != pfc_infinite) {
            t_size i, pcount = m_items.get_count();
            pfc::bit_array_bittable sel(pcount);
            get_selection_state(sel);

            pfc::list_t<t_size> indices;
            indices.prealloc(32);
            for (i = 0; i < pcount; i++)
                if (sel[i])
                    indices.add_item(i);

            if (column_start > count)
                column_start = 0;

            unsigned column;
            for (column = column_start; column < count; column++) {
                if (notify_before_create_inline_edit(indices, column, false)) {
                    create_inline_edit(indices, column);
                    break;
                }
            }
        }
    }
}

void ListView::activate_inline_editing(const pfc::list_base_const_t<t_size>& indices, t_size column)
{
    unsigned count = m_columns.get_count();
    if (column < count) {
        {
            if (notify_before_create_inline_edit(indices, column, false)) {
                create_inline_edit(indices, column);
            }
        }
    }
}
void ListView::activate_inline_editing(t_size index, t_size column)
{
    activate_inline_editing(pfc::list_single_ref_t<t_size>(index), column);
}

LRESULT WINAPI ListView::g_on_inline_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ListView* p_this;
    LRESULT rv;

    p_this = reinterpret_cast<ListView*>(GetWindowLongPtr(wnd, GWLP_USERDATA));

    rv = p_this ? p_this->on_inline_edit_message(wnd, msg, wp, lp) : DefWindowProc(wnd, msg, wp, lp);
    ;

    return rv;
}

LRESULT ListView::on_inline_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KILLFOCUS:
        if (!m_inline_edit_prevent_kill) {
            save_inline_edit();
            PostMessage(get_wnd(), MSG_KILL_INLINE_EDIT, 0, 0);
        }
        invalidate_all();
        break;
    case WM_SETFOCUS:
        break;
    case WM_GETDLGCODE:
        return CallWindowProc(m_proc_inline_edit, wnd, msg, wp, lp) | DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_TAB: {
            unsigned count = m_columns.get_count();
            t_size indices_count = m_inline_edit_indices.get_count();
            assert(indices_count);
            if (count && indices_count) {
                bool back = (GetKeyState(VK_SHIFT) & KF_UP) != 0;
                if (back) {
                    t_size column = m_inline_edit_column;
                    t_size row = m_inline_edit_indices[0];
                    pfc::string8_fast_aggressive temp;
                    bool found = false;

                    if (indices_count == 1) {
                        do {
                            pfc::list_single_ref_t<t_size> indices(row);
                            while (
                                column > 0 && !((found = notify_before_create_inline_edit(indices, --column, false)))) {
                            }
                            if (!found)
                                column = count;
                        } while (!found && row > 0 && --row >= 0);

                        if (found) {
                            pfc::list_single_ref_t<t_size> indices(row);
                            create_inline_edit(indices, column);
                        }
                    } else {
                        while (column > 0
                            && !((found = notify_before_create_inline_edit(m_inline_edit_indices, --column, false)))) {
                        }

                        if (found) {
                            create_inline_edit(m_inline_edit_indices, column);
                        }
                    }
                } else {
                    t_size column = m_inline_edit_column + 1;
                    t_size row = m_inline_edit_indices[0];
                    t_size row_count = m_items.get_count();
                    pfc::string8_fast_aggressive temp;
                    bool found = false;

                    if (indices_count == 1) {
                        do {
                            pfc::list_single_ref_t<t_size> indices(row);
                            while (column < count
                                && !((found = notify_before_create_inline_edit(indices, column, false)))) {
                                column++;
                            }
                            if (!found)
                                column = 0;
                        } while (!found && ++row < row_count);

                        if (found) {
                            pfc::list_single_ref_t<t_size> indices(row);
                            create_inline_edit(indices, column);
                        }

                    } else {
                        while (column < count
                            && !((found = notify_before_create_inline_edit(m_inline_edit_indices, column, false)))) {
                            column++;
                        }

                        if (found) {
                            create_inline_edit(m_inline_edit_indices, column);
                        }
                    }
                }
            }
        }
            return 0;
        case VK_ESCAPE:
            m_inline_edit_save = false;
            exit_inline_edit();
            return 0;
        case VK_RETURN:
            if ((GetKeyState(VK_CONTROL) & KF_UP) == 0)
                exit_inline_edit();
            // else
            //    return CallWindowProc(m_proc_original_inline_edit,wnd,msg,wp,lp); //cheat
            return 0;
        }
        break;
    }
    return CallWindowProc(m_proc_inline_edit, wnd, msg, wp, lp);
}

void ListView::create_inline_edit(const pfc::list_base_const_t<t_size>& indices, unsigned column)
{
    t_size indices_count = indices.get_count();
    if (!indices_count)
        return;
    if (!(column < m_columns.get_count()) || m_selecting) {
        // console::print("internal error - edit column index out of range");
        return;
    }

    {
        t_size item_count = m_items.get_count();
        for (t_size j = 0; j < indices_count; j++) {
            if (indices[j] >= item_count)
                return;
        }
    }

    t_size indices_spread = indices[indices_count - 1] - indices[0] + 1;
    t_size items_top = get_item_position(indices[0]);
    t_size items_bottom = get_item_position_bottom(indices[indices_count - 1]);
    int indices_total_height = min(items_bottom - items_top, MAXLONG);

    if (m_timer_inline_edit) {
        KillTimer(get_wnd(), EDIT_TIMER_ID);
        m_timer_inline_edit = false;
    }

    t_size median = indices[0] + indices_spread / 2; // indices[(indices_count/2)];

    bool start_visible = is_visible(indices[0]);
    bool end_visible = is_visible(indices[indices_count - 1]);

    if (!start_visible || !end_visible) {
        SCROLLINFO si;
        memset(&si, 0, sizeof(SCROLLINFO));
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS | SIF_TRACKPOS | SIF_PAGE | SIF_RANGE;
        GetScrollInfo(get_wnd(), SB_VERT, &si);
        t_size target;
        if (indices_count > si.nPage) {
            target = median;
            scroll(false, get_item_position(target) + m_item_height / 2 - ((si.nPage > 1 ? si.nPage - 1 : 0) / 2));
        } else {
            target = get_item_position(indices[0]) > m_scroll_position ? indices[indices_count - 1] : indices[0];
            scroll(false,
                get_item_position(target)
                    - (get_item_position(target) > m_scroll_position ? (si.nPage > 1 ? si.nPage - 1 : 0) - m_item_height
                                                                     : 0));
        }
    }

    int x;
    {
        x = get_total_indentation();

        unsigned n, count = m_columns.get_count();
        for (n = 0; n < count && n < column; n++) {
            x += m_columns[n].m_display_size;
        }
    }

    RECT rc_playlist, rc_items;
    GetClientRect(get_wnd(), &rc_playlist);
    get_items_rect(&rc_items);

    int font_height = uih::get_font_height(m_font);
    int header_height = rc_items.top;

    int cx = m_columns[column].m_display_size;
    int cy = min(max(indices_total_height, font_height), rc_items.bottom - rc_items.top);
    int y = get_item_position(indices[0]) - m_scroll_position + header_height;
    if (cy > indices_total_height)
        y -= (cy - indices_total_height) / 2;
    if (y < header_height)
        y = header_height;

    if (!m_autosize
        && ((x - m_horizontal_scroll_position < 0) || x + cx - m_horizontal_scroll_position > rc_items.right)) {
        if (x - m_horizontal_scroll_position < 0) {
            scroll(false, x, true);
        } else if (x + cx - m_horizontal_scroll_position > rc_items.right) {
            const int x_right = x + cx - rc_items.right;
            scroll(false, cx > rc_items.right ? x : x_right, true);
        }
    }

    x -= m_horizontal_scroll_position;

    if (m_wnd_inline_edit) {
        save_inline_edit();

        m_inline_edit_prevent_kill = true;
        DestroyWindow(m_wnd_inline_edit);
        m_wnd_inline_edit = nullptr;
        m_inline_edit_autocomplete.release();
        m_inline_edit_prevent_kill = false;
    }

    pfc::string8 text;
    t_size flags = 0;
    mmh::ComPtr<IUnknown> pAutoCompleteEntries;
    if (!notify_create_inline_edit(indices, column, text, flags, pAutoCompleteEntries)) {
        m_inline_edit_save = false;
        exit_inline_edit();
        return;
    }

    if (!m_wnd_inline_edit) {
        m_inline_edit_save = true;
        m_wnd_inline_edit = CreateWindowEx(0, WC_EDIT, pfc::stringcvt::string_os_from_utf8(text).get_ptr(),
            WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL | ES_MULTILINE | ES_AUTOVSCROLL
                | WS_BORDER | WS_CLIPCHILDREN | ((flags & inline_edit_uppercase) ? ES_UPPERCASE : 0),
            x, y, cx, cy, get_wnd(), HMENU(667), mmh::get_current_instance(), nullptr);

        m_proc_original_inline_edit = reinterpret_cast<WNDPROC>(GetWindowLongPtr(m_wnd_inline_edit, GWLP_WNDPROC));

        if (pAutoCompleteEntries.is_valid()) {
            if (SUCCEEDED(m_inline_edit_autocomplete.instantiate(CLSID_AutoComplete))) {
                if (pAutoCompleteEntries.is_valid())
                    m_inline_edit_autocomplete->Init(m_wnd_inline_edit, pAutoCompleteEntries, nullptr, nullptr);

                mmh::ComPtr<IAutoComplete2> pA2 = m_inline_edit_autocomplete;
                mmh::ComPtr<IAutoCompleteDropDown> pAutoCompleteDropDown = m_inline_edit_autocomplete;
                if (pA2.is_valid()) {
                    pA2->SetOptions(ACO_AUTOSUGGEST | ACO_UPDOWNKEYDROPSLIST);
                }
            }
        }

        SetWindowLongPtr(m_wnd_inline_edit, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
        m_proc_inline_edit = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(m_wnd_inline_edit, GWLP_WNDPROC, reinterpret_cast<LPARAM>(g_on_inline_edit_message)));

        SetWindowPos(m_wnd_inline_edit, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        SendMessage(m_wnd_inline_edit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), MAKELONG(TRUE, 0));
    }

    RECT rc;
    rc.left = x + 2;
    rc.top = y + (cy - font_height) / 2;
    rc.right = x + (cx > 0 ? cx - 2 : 0);
    rc.bottom = rc.top + font_height;
    MapWindowPoints(get_wnd(), m_wnd_inline_edit, reinterpret_cast<LPPOINT>(&rc), 2);

    SendMessage(m_wnd_inline_edit, EM_SETRECT, NULL, reinterpret_cast<LPARAM>(&rc));

    SendMessage(m_wnd_inline_edit, EM_SETSEL, 0, -1);
    SetFocus(m_wnd_inline_edit);

    if (&indices != &m_inline_edit_indices)
        m_inline_edit_indices = indices;
    m_inline_edit_column = column;
}

void ListView::save_inline_edit()
{
    if (m_inline_edit_save && !m_inline_edit_saving) {
        m_inline_edit_saving = true;

        pfc::string8 text;
        uih::get_window_text(m_wnd_inline_edit, text);
        notify_save_inline_edit(text.get_ptr());

        m_inline_edit_saving = false;
    }
    m_inline_edit_save = true;
}

void ListView::exit_inline_edit()
{
    // m_inline_edit_autocomplete_entries.release();
    m_inline_edit_autocomplete.release();
    if (m_wnd_inline_edit) {
        DestroyWindow(m_wnd_inline_edit);
        m_wnd_inline_edit = nullptr;
    }

    if (m_timer_inline_edit) {
        KillTimer(get_wnd(), EDIT_TIMER_ID);
        m_timer_inline_edit = false;
    }
    m_inline_edit_indices.remove_all();
    notify_exit_inline_edit();
}

} // namespace uih