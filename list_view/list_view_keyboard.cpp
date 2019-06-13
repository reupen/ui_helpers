#include "../stdafx.h"

namespace uih {

bool ListView::on_wm_keydown(WPARAM wp, LPARAM lp, LRESULT& ret)
{
    SendMessage(get_wnd(), WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), NULL);

    bool unused{};
    if (notify_on_keyboard_keydown_filter(WM_KEYDOWN, wp, lp, unused))
        return true;

    ret = 0;

    switch (wp) {
    case VK_TAB:
        uih::handle_tab_down(get_wnd());
        return true;
    case VK_HOME:
    case VK_DOWN:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP: {
        const bool b_redraw = disable_redrawing();

        process_navigation_keydown(wp, ((HIWORD(lp) & KF_ALTDOWN) != 0), (HIWORD(lp) & KF_REPEAT) != 0);

        if (b_redraw)
            enable_redrawing();
        return true;
    }
    case VK_SPACE: {
        const bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);
        if (ctrl_down) {
            const t_size focus = get_focus_item();

            if (focus != pfc_infinite)
                set_item_selected(focus, !get_item_selected(focus));
        }
        return true;
    }
    case VK_RETURN: {
        const bool ctrl_down = 0 != (GetKeyState(VK_CONTROL) & KF_UP);
        const t_size focus = get_focus_item();
        if (focus != pfc_infinite)
            execute_default_action(focus, pfc_infinite, true, ctrl_down);
        return true;
    }
    case VK_SHIFT:
        if (!(HIWORD(lp) & KF_REPEAT)) {
            t_size focus = get_focus_item();
            m_shift_start = focus != pfc_infinite ? focus : 0;
        }
        return false;
    case VK_F2:
        activate_inline_editing();
        return true;
    case VK_DELETE:
        return notify_on_keyboard_keydown_remove();
    case VK_F3:
        return notify_on_keyboard_keydown_search();
    }

    return false;
}
} // namespace uih
