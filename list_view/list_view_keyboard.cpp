#include "stdafx.h"

#include "list_view.h"

namespace uih {

bool ListView::on_wm_keydown(WPARAM wp, LPARAM lp)
{
    SendMessage(get_wnd(), WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), NULL);

    if (notify_on_keyboard_keydown_filter(WM_KEYDOWN, wp, lp))
        return true;

    const auto is_alt_down = (HIWORD(lp) & KF_ALTDOWN) != 0;
    const auto is_ctrl_down = (GetKeyState(VK_CONTROL) & KF_UP) != 0;

    const auto should_process_ctrl_shortcuts = [&] {
        const auto is_shift_down = (GetKeyState(VK_SHIFT) & KF_UP) != 0;
        const auto is_lwin_down = (GetKeyState(VK_LWIN) & KF_UP) != 0;
        const auto is_rwin_down = (GetKeyState(VK_RWIN) & KF_UP) != 0;
        return is_ctrl_down && !(is_alt_down || is_shift_down || is_lwin_down || is_rwin_down);
    };

    switch (wp) {
    case VK_TAB:
        uih::handle_tab_down(get_wnd());
        return true;
    case VK_HOME:
    case VK_DOWN:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
        process_navigation_keydown(wp, ((HIWORD(lp) & KF_ALTDOWN) != 0), (HIWORD(lp) & KF_REPEAT) != 0);
        return true;
    case VK_SPACE: {
        if (is_ctrl_down) {
            const size_t focus = get_focus_item();

            if (focus != pfc_infinite)
                set_item_selected(focus, !get_item_selected(focus));
            return true;
        }
        return false;
    }
    case VK_RETURN: {
        const size_t focus = get_focus_item();
        if (focus != pfc_infinite)
            execute_default_action(focus, pfc_infinite, true, is_ctrl_down);
        return true;
    }
    case VK_SHIFT:
        if (!(HIWORD(lp) & KF_REPEAT)) {
            size_t focus = get_focus_item();
            m_shift_start = focus != pfc_infinite ? focus : 0;
        }
        return false;
    case VK_F2:
        activate_inline_editing();
        return true;
    case VK_DELETE:
        return notify_on_keyboard_keydown_remove();
    case VK_F3:
        if (m_search_bar) {
            if (GetKeyState(VK_SHIFT) & 0x8000)
                m_search_bar.previous();
            else
                m_search_bar.next();

            return true;
        }
        return notify_on_keyboard_keydown_search();
    case 'A':
        if (should_process_ctrl_shortcuts() && m_selection_mode == SelectionMode::Multiple) {
            set_selection_state(pfc::bit_array_true(), pfc::bit_array_true());
            return true;
        }
        return false;
    case 'C':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_copy();
    case 'F':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_search();
    case 'V':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_paste();
    case 'X':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_cut();
    case 'Y':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_redo();
    case 'Z':
        return should_process_ctrl_shortcuts() && notify_on_keyboard_keydown_undo();
    default:
        return false;
    }
}
} // namespace uih
