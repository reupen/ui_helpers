#include "stdafx.h"

#include "list_view.h"
#include "../text_style.h"

using namespace uih::literals::spx;

namespace uih {

LRESULT ListView::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
#if 1
    static const UINT MSG_DI_GETDRAGIMAGE = RegisterWindowMessage(DI_GETDRAGIMAGE);

    if (msg && msg == MSG_DI_GETDRAGIMAGE) {
        auto lpsdi = (LPSHDRAGIMAGE)lp;

        return render_drag_image(lpsdi);
    }
#endif

    switch (msg) {
    case WM_CREATE:
        m_buffered_paint_initialiser.emplace();

        // For dark mode, the window needs to have the DarkMode_Explorer theme to get dark scroll bars,
        // but we also need access to DarkMode_ItemsView themes. To do this, a dummy window with a
        // different window theme is created.
        m_dummy_theme_window
            = std::make_unique<ContainerWindow>(ContainerWindowConfig{L"list_view_dummy_theme_window_FJg96cJ"});
        m_dummy_theme_window->create(wnd, {-1, -1, 0, 0});

        try {
            m_direct_write_context = direct_write::Context::s_create();
        }
        CATCH_LOG();

        notify_on_initialisation();

        set_window_theme();
        reopen_themes();

        if (!m_items_log_font || !m_header_log_font) {
            LOGFONT icon_font{};
            if (SystemParametersInfo(SPI_GETICONTITLELOGFONT, 0, &icon_font, 0)) {
                if (!m_items_log_font)
                    m_items_log_font = icon_font;

                if (!m_header_log_font)
                    m_header_log_font = m_items_log_font;
            }
        }

        if (m_direct_write_context) {
            if (!m_items_text_format && m_items_log_font)
                m_items_text_format = m_direct_write_context->create_text_format_with_fallback(*m_items_log_font);

            if (!m_header_text_format && m_header_log_font)
                m_header_text_format = m_direct_write_context->create_text_format_with_fallback(*m_header_log_font);
        }

        if (!m_group_text_format)
            m_group_text_format = m_items_text_format;

        refresh_items_font();
        refresh_group_font();

        if (m_show_header)
            create_header();

        m_initialised = true;
        notify_on_create();
        build_header();

        if (m_wnd_header)
            ShowWindow(m_wnd_header, SW_SHOWNORMAL);
        return 0;
    case WM_DESTROY:
        m_initialised = false;
        m_inline_edit_save = false;
        destroy_tooltip();
        exit_inline_edit();
        destroy_header();
        close_themes();
        m_items_font.reset();
        m_dummy_theme_window->destroy();
        m_dummy_theme_window.reset();
        m_items.clear();
        m_columns.clear();
        m_items_text_format.reset();
        m_header_text_format.reset();
        m_group_text_format.reset();
        m_direct_write_context.reset();
        m_drag_image_creator.reset();
        notify_on_destroy();
        return 0;
    case WM_NCDESTROY:
        m_buffered_paint_initialiser.reset();
        break;
    case WM_SIZE:
        on_size(LOWORD(lp), HIWORD(lp));
        break;
    case WM_THEMECHANGED:
        reopen_themes();
        RedrawWindow(wnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);
        break;
    case WM_TIMECHANGE:
        notify_on_time_change();
        break;
    case WM_MENUSELECT:
        notify_on_menu_select(wp, lp);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        const auto dc = wil::BeginPaint(wnd, &ps);
        BufferedPaint buffered_dc(dc.get(), ps.rcPaint);
        render_items(buffered_dc.get(), ps.rcPaint);
        return 0;
    }
    case WM_UPDATEUISTATE:
        RedrawWindow(wnd, nullptr, nullptr, RDW_INVALIDATE);
        break;
    case WM_SETFOCUS:
        invalidate_all();
        if (!HWND(wp) || (HWND(wp) != wnd && !IsChild(wnd, HWND(wp))))
            notify_on_set_focus(HWND(wp));
        break;
    case WM_KILLFOCUS:
        invalidate_all();
        if (!HWND(wp) || (HWND(wp) != wnd && !IsChild(wnd, HWND(wp))))
            notify_on_kill_focus(HWND(wp));
        break;
    case WM_MOUSEACTIVATE:
        if (GetFocus() != wnd)
            m_inline_edit_prevent = true;
        return MA_ACTIVATE;
    case WM_LBUTTONDOWN: {
        bool b_was_focused = GetFocus() == wnd;
        if (!b_was_focused)
            m_inline_edit_prevent = true;
        exit_inline_edit();
        SetFocus(wnd);
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        HitTestResult hit_result;
        hit_test_ex(pt, hit_result);
        m_lbutton_down_hittest = hit_result;
        bool b_shift_down = (wp & MK_SHIFT) != 0;
        m_lbutton_down_ctrl = (wp & MK_CONTROL) != 0 && m_selection_mode == SelectionMode::Multiple; // Cheat.

        if (hit_result.category == HitTestCategory::OnUnobscuredItem
            || hit_result.category == HitTestCategory::OnItemObscuredBelow
            || hit_result.category == HitTestCategory::OnItemObscuredAbove) {
            if (!m_inline_edit_prevent)
                m_inline_edit_prevent
                    = !((get_item_selected(hit_result.index) && !m_wnd_inline_edit && (get_selection_count(2) == 1)));
            if (hit_result.category == HitTestCategory::OnItemObscuredBelow)
                ensure_visible(hit_result.index);
            if (hit_result.category == HitTestCategory::OnItemObscuredAbove)
                ensure_visible(hit_result.index);

            m_dragging_initial_point = pt;
            /*if (b_shift_down && m_lbutton_down_ctrl)
            {
                move_selection (hit_result.index-get_focus_item());
            }
            else */
            if (b_shift_down && m_selection_mode == SelectionMode::Multiple) {
                size_t focus = get_focus_item();
                size_t start = m_alternate_selection ? focus : m_shift_start;
                pfc::bit_array_range br(std::min(start, hit_result.index), abs(t_ssize(start - hit_result.index)) + 1);
                if (m_lbutton_down_ctrl && !m_alternate_selection) {
                    set_selection_state(br, br, true);
                } else {
                    if (m_alternate_selection && !get_item_selected(focus))
                        set_selection_state(br, pfc::bit_array_not(br), true);
                    else if (m_alternate_selection)
                        set_selection_state(br, br, true);
                    else
                        set_selection_state(pfc::bit_array_true(), br, true);
                }
                set_focus_item(hit_result.index, true);
            } else {
                m_selecting_move = get_item_selected(hit_result.index);
                m_selecting_moved = false;
                m_selecting_start = hit_result.index;
                m_selecting_start_column = hit_result.column;
                if (!m_lbutton_down_ctrl) {
                    if (!m_selecting_move)
                        set_item_selected_single(hit_result.index);
                    else
                        set_focus_item(hit_result.index);
                    m_selecting = true; //! m_single_selection;
                }
            }
            SetCapture(wnd);
        } else if (hit_result.category == HitTestCategory::OnGroupHeader) {
            if (m_selection_mode == SelectionMode::Multiple) {
                if (!m_lbutton_down_ctrl) {
                    const auto [index, count] = get_item_group_range(hit_result.index, hit_result.group_level);

                    if (!hit_result.is_stuck || b_shift_down)
                        set_selection_state(pfc::bit_array_true(), pfc::bit_array_range(index, count));

                    if (count > 0) {
                        ensure_visible(index, EnsureVisibleMode::PreferMinimalScrolling);
                        set_focus_item(index);
                    }
                }
            }
        } else // if (hit_result.result != hit_test_)
        {
            if (m_selection_mode != SelectionMode::SingleStrict)
                set_selection_state(pfc::bit_array_true(), pfc::bit_array_false());
        }
        // console::formatter() << hit_result.result ;
    }
        return 0;
    case WM_LBUTTONUP: {
        if (m_selecting_move && !m_selecting_moved && !m_lbutton_down_ctrl) {
            if (m_selecting_start < m_items.size()) {
                if (!m_inline_edit_prevent && get_item_selected(m_selecting_start)) {
                    {
                        exit_inline_edit();
                        pfc::list_t<size_t> indices;
                        indices.add_item(m_selecting_start);
                        if (notify_before_create_inline_edit(indices, m_selecting_start_column, true)) {
                            m_inline_edit_indices = indices;
                            m_inline_edit_column = m_selecting_start_column;
                            if (m_inline_edit_column >= 0) {
                                m_timer_inline_edit
                                    = (SetTimer(wnd, EDIT_TIMER_ID, GetDoubleClickTime(), nullptr) != 0);
                            }
                        }
                    }
                }
                set_item_selected_single(m_selecting_start);
                // set_focus_item(m_selecting_start);
            }
        }
        if (m_selecting) {
            m_selecting = false;
            destroy_timer_scroll_down();
            destroy_timer_scroll_up();
        } else if (m_lbutton_down_ctrl) {
            const HitTestResult& hit_result = m_lbutton_down_hittest;
            if (wp & MK_CONTROL) {
                if (hit_result.category == HitTestCategory::OnUnobscuredItem
                    || hit_result.category == HitTestCategory::OnItemObscuredBelow
                    || hit_result.category == HitTestCategory::OnItemObscuredAbove) {
                    if (m_selecting_start < m_items.size()) {
                        set_item_selected(m_selecting_start, !get_item_selected(m_selecting_start));
                        set_focus_item(m_selecting_start);
                    }
                } else if (hit_result.category == HitTestCategory::OnGroupHeader) {
                    if (hit_result.index < m_items.size() && hit_result.group_level < m_group_count) {
                        const auto [index, count] = get_item_group_range(hit_result.index, hit_result.group_level);

                        if (count > 0) {
                            set_selection_state(pfc::bit_array_range(index, count),
                                pfc::bit_array_range(index, count, !is_range_selected(index, count)), true);
                            ensure_visible(index, EnsureVisibleMode::PreferMinimalScrolling);
                            set_focus_item(index);
                        }
                    }
                }
            }
            m_lbutton_down_ctrl = false;
        }
        m_selecting_start = pfc_infinite;
        m_selecting_start_column = pfc_infinite;
        m_selecting_move = false;
        m_selecting_moved = false;
        m_inline_edit_prevent = false;
        if (GetCapture() == wnd)
            ReleaseCapture();
    }
        return 0;
    case WM_RBUTTONUP: {
        m_inline_edit_prevent = false;
        m_dragging_rmb = false;
        m_dragging_rmb_initial_point.x = 0;
        m_dragging_rmb_initial_point.y = 0;
    } break;
    case WM_RBUTTONDOWN: {
        SetFocus(wnd);

        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        HitTestResult hit_result;
        hit_test_ex(pt, hit_result);
        if (hit_result.category == HitTestCategory::OnUnobscuredItem
            || hit_result.category == HitTestCategory::OnItemObscuredBelow
            || hit_result.category == HitTestCategory::OnItemObscuredAbove) {
            m_dragging_rmb = true;
            m_dragging_rmb_initial_point = pt;

            if (hit_result.category == HitTestCategory::OnItemObscuredBelow
                || hit_result.category == HitTestCategory::OnItemObscuredAbove)
                ensure_visible(hit_result.index);

            if (!get_item_selected(hit_result.index)) {
                if (m_selection_mode == SelectionMode::SingleStrict) {
                    set_focus_item(hit_result.index);
                } else
                    set_item_selected_single(hit_result.index, true, notification_source_rmb);
            } else if (get_focus_item() != hit_result.index)
                set_focus_item(hit_result.index);
        } else if (hit_result.category == HitTestCategory::OnGroupHeader) {
            const auto [index, count] = get_item_group_range(hit_result.index, hit_result.group_level);
            const auto is_selected = is_range_selected(index, count);

            if (!is_selected)
                set_selection_state(pfc::bit_array_true(), pfc::bit_array_range(index, count));

            set_focus_item(index);
        } else if (m_selection_mode != SelectionMode::SingleStrict) {
            set_selection_state(pfc::bit_array_true(), pfc::bit_array_false());
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (!m_selecting) {
            handle_mousemove_for_tooltip(pt);
        }
        if (m_selecting_move || (m_selection_mode != SelectionMode::Multiple && m_selecting) || m_dragging_rmb) {
            const auto cx_drag = (unsigned)abs(GetSystemMetrics(SM_CXDRAG));
            const auto cy_drag = (unsigned)abs(GetSystemMetrics(SM_CYDRAG));

            bool b_enter_drag = false;

            if (!m_dragging_rmb && (wp & MK_LBUTTON)
                && ((unsigned)abs(m_dragging_initial_point.x - pt.x) > cx_drag
                    || (unsigned)abs(m_dragging_initial_point.y - pt.y) > cy_drag))
                b_enter_drag = true;
            if (m_dragging_rmb && (wp & MK_RBUTTON)
                && ((unsigned)abs(m_dragging_rmb_initial_point.x - pt.x) > cx_drag
                    || (unsigned)abs(m_dragging_rmb_initial_point.y - pt.y) > cy_drag))
                b_enter_drag = true;

            if (b_enter_drag) {
                hide_tooltip();
                if (do_drag_drop(wp)) {
                    m_selecting_moved = false;
                    m_selecting_move = false;
                    m_selecting = false;
                    m_dragging_rmb = false;
                    m_dragging_rmb_initial_point.x = 0;
                    m_dragging_rmb_initial_point.y = 0;
                }
            }
        }
        if (m_selecting && m_selection_mode == SelectionMode::Multiple) {
            // size_t index;
            if (!m_selecting_move) {
                HitTestResult hit_result;
                hit_test_ex(pt, hit_result, true);
                {
                    if (hit_result.category == HitTestCategory::AboveViewport
                        || hit_result.category == HitTestCategory::BelowViewport
                        || hit_result.category == HitTestCategory::OnUnobscuredItem
                        //|| hit_result.result == hit_test_obscured_below || hit_result.result ==
                        // hit_test_obscured_above
                        || hit_result.category == HitTestCategory::RightOfItem
                        || hit_result.category == HitTestCategory::LeftOfItem
                        || hit_result.category == HitTestCategory::RightOfGroupHeader
                        || hit_result.category == HitTestCategory::LeftOfGroupHeader
                        || hit_result.category == HitTestCategory::NotOnItem
                        || hit_result.category == HitTestCategory::OnGroupHeader) {
                        if (hit_result.category == HitTestCategory::BelowViewport) {
                            create_timer_scroll_down();
                        } else
                            destroy_timer_scroll_down();

                        if (hit_result.category == HitTestCategory::AboveViewport) {
                            create_timer_scroll_up();
                        } else
                            destroy_timer_scroll_up();

                        if (hit_result.category == HitTestCategory::OnItemObscuredBelow
                            || hit_result.category == HitTestCategory::OnItemObscuredAbove)
                            ensure_visible(hit_result.index);

                        size_t target_index{};

                        if (hit_result.category == HitTestCategory::OnGroupHeader
                            && hit_result.index > m_selecting_start) {
                            target_index = hit_result.index - 1;
                        } else if (hit_result.category == HitTestCategory::NotOnItem
                            && hit_result.index < m_selecting_start && hit_result.index + 1 < get_item_count())
                            target_index = hit_result.index + 1;
                        else {
                            target_index = hit_result.index;
                        }

                        if (get_focus_item() != target_index) {
                            if (!is_partially_visible(target_index)) {
                                if (gsl::narrow_cast<int>(target_index) > get_last_unobscured_item())
                                    scroll(get_item_position_bottom(target_index) - get_item_area_height());
                                else
                                    scroll(get_item_position(target_index));
                            }

                            const auto num_to_select
                                = static_cast<size_t>(std::abs(
                                      gsl::narrow_cast<int>(m_selecting_start) - gsl::narrow_cast<int>(target_index)))
                                + 1;
                            set_selection_state(pfc::bit_array_true(),
                                pfc::bit_array_range(std::min(target_index, m_selecting_start), num_to_select));
                            set_focus_item(target_index);
                        }
                    }
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        HitTestResult hit_result;
        hit_test_ex(pt, hit_result);
        if (hit_result.category == HitTestCategory::OnUnobscuredItem) {
            exit_inline_edit();
            m_inline_edit_prevent = true;
            size_t focus = get_focus_item();
            if (focus != pfc_infinite)
                execute_default_action(focus, hit_result.column, false, (wp & MK_CONTROL) != 0);
            return 0;
        }
        if (hit_result.category == HitTestCategory::RightOfItem
            || hit_result.category == HitTestCategory::RightOfGroupHeader
            || hit_result.category == HitTestCategory::NotOnItem)
            if (notify_on_doubleleftclick_nowhere())
                return 0;
        break;
    }
    case WM_MBUTTONDOWN:
        SetFocus(wnd);
        return 0;
    case WM_MBUTTONUP: {
        m_inline_edit_prevent = false;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        HitTestResult hit_result;
        hit_test_ex(pt, hit_result);
        if (notify_on_middleclick(hit_result.category == HitTestCategory::OnUnobscuredItem, hit_result.index))
            return 0;
    } break;
    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL: {
        const auto style = GetWindowLongPtr(get_wnd(), GWL_STYLE);
        const auto has_vscroll = (style & WS_VSCROLL) != 0;
        const auto has_hscroll = (style & WS_HSCROLL) != 0;
        const auto ctrl_down = (wp & MK_CONTROL) != 0;
        const auto is_vert_mousewheel = msg == WM_MOUSEWHEEL;
        const auto is_horz_mousewheel = msg == WM_MOUSEHWHEEL;
        const bool scroll_horizontally = is_horz_mousewheel || ((!has_vscroll || ctrl_down) && has_hscroll);

        SCROLLINFO si{};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS | SIF_TRACKPOS | SIF_PAGE | SIF_RANGE;
        GetScrollInfo(get_wnd(), scroll_horizontally ? SB_HORZ : SB_VERT, &si);

        UINT system_scroll_lines = 3; // 3 is default
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &system_scroll_lines, 0);

        if (!si.nPage)
            si.nPage++;

        int scroll_unit{};

        if (system_scroll_lines == -1)
            scroll_unit = si.nPage - 1;
        else
            scroll_unit = system_scroll_lines * m_item_height;

        if (scroll_unit == 0)
            scroll_unit = 1;

        int wheel_delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (is_vert_mousewheel)
            wheel_delta *= -1;

        int scroll_delta = MulDiv(wheel_delta, scroll_unit, 120);

        // Limit scrolling to one page ?!?!?! It was in Columns Playlist code...
        if (scroll_delta < 0 && static_cast<UINT>(-scroll_delta) > si.nPage) {
            scroll_delta = si.nPage * -1;
            if (scroll_delta < -1)
                scroll_delta++;
        } else if (scroll_delta > 0 && static_cast<UINT>(scroll_delta) > si.nPage) {
            scroll_delta = si.nPage;
            if (scroll_delta > 1)
                scroll_delta--;
        }

        exit_inline_edit();
        scroll(scroll_delta + (scroll_horizontally ? m_horizontal_scroll_position : m_scroll_position),
            scroll_horizontally);
    }
        return 0;
    case WM_GETDLGCODE:
        return DefWindowProc(wnd, msg, wp, lp) | DLGC_WANTARROWS;
    case WM_SHOWWINDOW:
        if (wp == TRUE && lp == 0 && !m_shown) {
            on_first_show();
            m_shown = true;
        }
        break;
    case WM_KEYDOWN: {
        if ((m_prevent_wm_char_processing = on_wm_keydown(wp, lp)))
            return 0;
    } break;
    case WM_CHAR:
        // Values below 32, and 127, are special values (e.g. Ctrl-A and Ctrl-Backspace)
        if (!m_prevent_wm_char_processing && wp >= 32u && wp != 127u)
            on_search_string_change(LOWORD(wp));
        break;
    case WM_SYSKEYDOWN: {
        if ((m_prevent_wm_char_processing = notify_on_keyboard_keydown_filter(WM_SYSKEYDOWN, wp, lp)))
            return 0;
    } break;
    case WM_VSCROLL:
        // IDropTargetHelper inexplicably sends these messages. They aren't processed to
        // avoid interfering with the timer usually used by clients.
        // If this causes problems, the alternative is to let clients disable and enable
        // processing as needed.
        if ((LOWORD(wp) == SB_LINEDOWN || LOWORD(wp) == SB_LINEUP) && HIWORD(wp) == 1)
            return 0;

        exit_inline_edit();
        scroll_from_scroll_bar(LOWORD(wp));
        return 0;
    case WM_HSCROLL:
        exit_inline_edit();
        scroll_from_scroll_bar(LOWORD(wp), true);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_INLINEEDIT:
            switch (HIWORD(wp)) {
            case EN_KILLFOCUS: {
                HWND wnd_focus = GetFocus();
                if (!HWND(wnd_focus) || (HWND(wnd_focus) != wnd && !IsChild(wnd, wnd_focus)))
                    notify_on_kill_focus(wnd_focus);
                break;
            }
            }
            break;
        case IDC_SEARCHBOX:
            switch (HIWORD(wp)) {
            case EN_CHANGE: {
                pfc::string8 text;
                uih::get_window_text(reinterpret_cast<HWND>(lp), text);
                notify_on_search_box_contents_change(text);
            } break;
            case EN_KILLFOCUS: {
                RedrawWindow(HWND(lp), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
                HWND wnd_focus = GetFocus();
                if (!HWND(wnd_focus) || (HWND(wnd_focus) != wnd && !IsChild(wnd, wnd_focus)))
                    notify_on_kill_focus(wnd_focus);
            } break;
            case EN_SETFOCUS:
                RedrawWindow(HWND(lp), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
                break;
            };
            break;
        };
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        const auto wnd_edit = reinterpret_cast<HWND>(lp);
        const auto dc = reinterpret_cast<HDC>(wp);

        if (!wnd_edit)
            break;

        if (wnd_edit == m_wnd_inline_edit && m_use_dark_mode) {
            SetTextColor(dc, m_dark_edit_text_colour);
            SetBkColor(dc, m_dark_edit_background_colour);

            if (!m_edit_background_brush) {
                m_edit_background_brush.reset(CreateSolidBrush(m_dark_edit_background_colour));
            }
            return reinterpret_cast<LPARAM>(m_edit_background_brush.get());
        }

        if (wnd_edit == m_search_editbox) {
            /*POINT pt;
            GetMessagePos(&pt);
            HWND wnd_focus = GetFocus();

            bool b_hot = WindowFromPoint(pt) == m_search_editbox;*/
            bool b_focused = GetFocus() == m_search_editbox;

            if (b_focused)
                return (LRESULT)GetSysColorBrush(COLOR_WINDOW);

            if (m_search_box_hot)
                return (LRESULT)GetSysColorBrush(
                    COLOR_WINDOW); // m_search_box_hot_brush.get();//GetSysColorBrush(COLOR_BTNFACE);

            return (LRESULT)GetSysColorBrush(IsThemeActive() && IsAppThemed()
                    ? COLOR_BTNFACE
                    : COLOR_WINDOW); // m_search_box_nofocus_brush.get();//GetSysColorBrush(COLOR_3DLIGHT);
        }
        break;
    }
    case WM_NOTIFY: {
        const auto lpnm = reinterpret_cast<LPNMHDR>(lp);
        if (m_wnd_header && lpnm->hwndFrom == m_wnd_header) {
            if (auto result = on_wm_notify_header(lpnm))
                return *result;
        } else if (m_wnd_tooltip && lpnm->hwndFrom == m_wnd_tooltip) {
            if (auto result = on_wm_notify_tooltip(lpnm))
                return *result;
        }
        break;
    }
    case WM_TIMER:
        switch (wp) {
        case TIMER_END_SEARCH: {
            destroy_timer_search();
            m_search_string.clear();
        }
            return 0;
        case TIMER_SCROLL_UP:
            scroll_from_scroll_bar(SB_LINEUP);
            return 0;
        case TIMER_SCROLL_DOWN:
            scroll_from_scroll_bar(SB_LINEDOWN);
            return 0;
        case EDIT_TIMER_ID: {
            create_inline_edit(pfc::list_t<size_t>(m_inline_edit_indices), m_inline_edit_column);
            if (m_timer_inline_edit) {
                KillTimer(wnd, EDIT_TIMER_ID);
                m_timer_inline_edit = false;
            }
            return 0;
        }
        default:
            if (notify_on_timer(wp))
                return 0;
            break;
        };
        break;
    case MSG_KILL_INLINE_EDIT:
        m_inline_edit_prevent_kill = true;

        if (wp == TRUE)
            save_inline_edit();

        exit_inline_edit();
        m_inline_edit_prevent_kill = false;
        return 0;
    case WM_CONTEXTMENU: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        const auto from_keyboard = pt.x == -1 && pt.y == -1;
        if (get_wnd() == (HWND)wp) {
            POINT px;
            {
                if (from_keyboard) {
                    const auto focus = gsl::narrow_cast<int>(get_focus_item());
                    const auto last = get_last_unobscured_item();
                    if (focus < 0 || focus < get_item_at_or_after(m_scroll_position) || focus > last) {
                        px.x = 0;
                        px.y = 0;
                    } else {
                        const auto rc = get_items_rect();
                        px.x = 0;
                        px.y = (get_item_position(focus) - m_scroll_position) + m_item_height / 2 + rc.top;
                    }
                    pt = px;
                    MapWindowPoints(wnd, HWND_DESKTOP, &pt, 1);
                } else {
                    px = pt;
                    ScreenToClient(wnd, &px);
                    RECT rc;
                    GetClientRect(wnd, &rc);
                    if (!PtInRect(&rc, px))
                        break;
                }
            }
            if (notify_on_contextmenu(pt, from_keyboard))
                return 0;
        } else if (m_wnd_header && (HWND)wp == m_wnd_header) {
            POINT temp;
            temp.x = pt.x;
            temp.y = pt.y;
            ScreenToClient(m_wnd_header, &temp);
            HDHITTESTINFO hittest;
            hittest.pt.x = temp.x;
            hittest.pt.y = temp.y;
            SendMessage(m_wnd_header, HDM_HITTEST, 0, (LPARAM)&hittest);

            if (notify_on_contextmenu_header(pt, hittest))
                return 0;
        }
    } break;
    }
    return DefWindowProc(wnd, msg, wp, lp);
}
} // namespace uih
