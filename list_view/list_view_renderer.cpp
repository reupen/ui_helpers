#include "../stdafx.h"

namespace uih {

const int _level_spacing_size = 3;

int ListView::get_item_indentation()
{
    const auto rc = get_items_rect();
    int ret = rc.left;
    if (m_group_count)
        ret += get_default_indentation_step() * m_group_count;
    return ret;
}
int ListView::get_default_indentation_step()
{
    int ret = 0;
    if (m_group_level_indentation_enabled) {
        HDC dc = GetDC(get_wnd());
        HFONT font_old = SelectFont(dc, m_font);
        const int cx_space = uih::get_text_width(dc, " ", 1);
        SelectFont(dc, font_old);
        ReleaseDC(get_wnd(), dc);
        ret = cx_space * _level_spacing_size;
    }
    return ret;
}

void ListView::render_items(HDC dc, const RECT& rc_update, int cx)
{
    ColourData p_data;
    render_get_colour_data(p_data);
    const lv::RendererContext context = {p_data, get_wnd(), dc, m_theme, m_items_view_theme};

    const t_size level_spacing_size = m_group_level_indentation_enabled ? _level_spacing_size : 0;
    // COLORREF cr_orig = GetTextColor(dc);
    // OffsetWindowOrgEx(dc, m_horizontal_scroll_position, 0, NULL);
    t_size highlight_index = get_highlight_item();
    t_size index_focus = get_focus_item();
    HWND wnd_focus = GetFocus();
    const bool should_hide_focus
        = (SendMessage(get_wnd(), WM_QUERYUISTATE, NULL, NULL) & UISF_HIDEFOCUS) != 0 && !m_always_show_focus;
    bool b_window_focused = (wnd_focus == get_wnd()) || IsChild(get_wnd(), wnd_focus);

    m_renderer->render_background(p_data, dc, &rc_update);
    const auto rc_items = get_items_rect();

    if (rc_update.bottom <= rc_update.top || rc_update.bottom < rc_items.top)
        return;

    t_size i;
    t_size count = m_items.size();
    const int cx_space = uih::get_text_width(dc, " ", 1);
    const int item_preindentation = cx_space * level_spacing_size * m_group_count + rc_items.left;
    const int item_indentation = item_preindentation + get_group_info_area_total_width();
    cx = get_columns_display_width() + item_indentation;

    bool b_show_group_info_area = get_show_group_info_area();

    i = get_item_at_or_before((rc_update.top > rc_items.top ? rc_update.top - rc_items.top : 0) + m_scroll_position);
    t_size i_start = i;
    t_size i_end = get_item_at_or_after(
        (rc_update.bottom > rc_items.top + 1 ? rc_update.bottom - rc_items.top - 1 : 0) + m_scroll_position);
    for (; i <= i_end && i < count; i++) {
        HFONT fnt_old = SelectFont(dc, m_group_font.get());
        t_size item_group_start = NULL;
        t_size item_group_count = NULL;
        get_item_group(i, m_group_count ? m_group_count - 1 : 0, item_group_start, item_group_count);

        t_size j;
        t_size countj = m_items[i]->m_groups.size();
        t_size counter = 0;
        for (j = 0; j < countj; j++) {
            if (!i || m_items[i]->m_groups[j] != m_items[i - 1]->m_groups[j]) {
                t_group_ptr p_group = m_items[i]->m_groups[j];
                int y = get_item_position(i) - m_scroll_position - m_group_height * (countj - j) + rc_items.top;
                int x = -m_horizontal_scroll_position + rc_items.left;
                // y += counter*m_item_height;
                RECT rc = {x, y, x + cx, y + m_group_height};

                if (rc.top >= rc_update.bottom) {
                    // OffsetWindowOrgEx(dc, -m_horizontal_scroll_position, 0, NULL);
                    break; // CRUDE
                }
                m_renderer->render_group(
                    p_data, dc, m_theme, i, j, p_group->m_text.get_ptr(), cx_space * level_spacing_size, j, rc);

                counter++;
            }
        }

        SelectFont(dc, fnt_old);

        if (b_show_group_info_area && (i == i_start || i == item_group_start)) {
            int height = (std::max)(m_item_height * gsl::narrow<int>(item_group_count), get_group_info_area_height());
            int gx = 0 - m_horizontal_scroll_position + item_preindentation;
            int gy = get_item_position(item_group_start) - m_scroll_position + rc_items.top;
            int gcx = get_group_info_area_width();
            RECT rc_group_info
                = {gx, gy, gx + gcx, get_item_position(item_group_start) + height - m_scroll_position + rc_items.top};
            if (rc_group_info.top >= rc_update.bottom)
                break;
            m_renderer->render_group_info(dc, item_group_start, rc_group_info);

            // console::printf("%u %u %u %u; %u %u %u %u",rc_group_info,rc_update);
        }

        bool b_selected = get_item_selected(i) || i == m_highlight_selected_item_index;

        t_item_ptr item = m_items[i];

        RECT rc = {0 - m_horizontal_scroll_position + item_indentation,
            get_item_position(i) - m_scroll_position + rc_items.top, cx - m_horizontal_scroll_position,
            get_item_position(i) + get_item_height(i) - m_scroll_position + rc_items.top};
        // rc.left += cx_space*level_spacing_size*countj;
        if (rc.top >= rc_update.bottom) {
            // OffsetWindowOrgEx(dc, -m_horizontal_scroll_position, 0, NULL);
            break; // CRUDE
        }
        if (rc.bottom > rc_update.top) {
            const auto show_item_focus = index_focus == i && (b_window_focused || m_always_show_focus);
            std::vector<lv::RendererSubItem> sub_items;

            for (size_t column_index{}; column_index < m_columns.size(); ++column_index) {
                const auto text = get_item_text(i, column_index);
                auto& column = m_columns[column_index];

                sub_items.emplace_back(lv::RendererSubItem{text, column.m_display_size, column.m_alignment});
            }

            m_renderer->render_item(context, i, sub_items, 0 /*item_indentation*/, b_selected, b_window_focused,
                (m_highlight_item_index == i) || (highlight_index == i), should_hide_focus, show_item_focus, rc);
            /*if (i == m_insert_mark_index || i + 1 == m_insert_mark_index)
            {
                gdi_object_t<HPEN>::ptr_t pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_WINDOWTEXT));
                HPEN pen_old = SelectPen(dc, pen);
                int yPos = i == m_insert_mark_index ? rc.top-counter*m_item_height : rc.bottom-1;
                MoveToEx(dc, item_indentation, yPos, NULL);
                LineTo(dc, rc.right, yPos);
                SelectPen(dc, pen_old);
            }*/
        }
    }
    /*if (m_search_editbox)
    {
        RECT rc_search;
        get_search_box_rect(&rc_search);
        rc_search.right = rc_search.left;
        rc_search.left = 0;
        if (rc_update.bottom >= rc_search.top)
        {
            FillRect(dc, &rc_search, GetSysColorBrush(COLOR_BTNFACE));
            //render_background(dc, &rc_search);
            uih::text_out_colours_tab(dc, m_search_label.get_ptr(), m_search_label.get_length(), 0, 2, &rc_search,
    false, GetSysColor(COLOR_WINDOWTEXT), false, false, false, uih::ALIGN_LEFT, NULL);
        }
    }*/
    if (m_insert_mark_index != pfc_infinite && (m_insert_mark_index <= count)) {
        RECT rc_line;
        RECT rc_dummy;
        rc_line.left = item_indentation;
        int yPos = 0;
        if (count) {
            if (m_insert_mark_index == count)
                yPos = get_item_position(count - 1) + get_item_height(count - 1) - m_scroll_position + rc_items.top - 1;
            else
                yPos = get_item_position(m_insert_mark_index) - m_scroll_position + rc_items.top - 1;
        }
        rc_line.top = yPos;
        rc_line.right = cx;
        rc_line.bottom = yPos + scale_dpi_value(2);
        if (IntersectRect(&rc_dummy, &rc_line, &rc_update)) {
            gdi_object_t<HBRUSH>::ptr_t br = CreateSolidBrush(p_data.m_text);
            FillRect(dc, &rc_line, br);
        }
        /*gdi_object_t<HBRUSH>::ptr_t pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_WINDOWTEXT));
        HPEN pen_old = SelectPen(dc, pen);
        MoveToEx(dc, item_indentation, yPos, NULL);
        LineTo(dc, cx, yPos);
        SelectPen(dc, pen_old);                */
    }
    // OffsetWindowOrgEx(dc, -m_horizontal_scroll_position, 0, NULL);
}

void ListView::render_get_colour_data(ColourData& p_out)
{
    p_out.m_themed = true;
    p_out.m_use_custom_active_item_frame = false;
    p_out.m_text = GetSysColor(COLOR_WINDOWTEXT);
    p_out.m_selection_text = GetSysColor(COLOR_HIGHLIGHTTEXT);
    p_out.m_background = GetSysColor(COLOR_WINDOW);
    p_out.m_selection_background = GetSysColor(COLOR_HIGHLIGHT);
    p_out.m_inactive_selection_text = GetSysColor(COLOR_BTNTEXT);
    p_out.m_inactive_selection_background = GetSysColor(COLOR_BTNFACE);
    p_out.m_active_item_frame = GetSysColor(COLOR_WINDOWFRAME);
    p_out.m_group_text = get_group_text_colour_default();
    p_out.m_group_background = p_out.m_background;
}

void lv::DefaultRenderer::render_group_line(const ColourData& p_data, HDC dc, HTHEME theme, const RECT* rc)
{
    if (theme && IsThemePartDefined(theme, LVP_GROUPHEADERLINE, NULL)
        && SUCCEEDED(DrawThemeBackground(theme, dc, LVP_GROUPHEADERLINE, LVGH_OPEN, rc, nullptr))) {
    } else {
        COLORREF cr = p_data.m_group_text; // get_group_text_colour_default();
        gdi_object_t<HPEN>::ptr_t pen = CreatePen(PS_SOLID, uih::scale_dpi_value(1), cr);
        HPEN pen_old = SelectPen(dc, pen);
        MoveToEx(dc, rc->left, rc->top, nullptr);
        LineTo(dc, rc->right, rc->top);
        SelectPen(dc, pen_old);
    }
}

void lv::DefaultRenderer::render_group_background(const ColourData& p_data, HDC dc, const RECT* rc)
{
    FillRect(dc, rc, gdi_object_t<HBRUSH>::ptr_t(CreateSolidBrush(p_data.m_group_background)));
}

COLORREF ListView::get_group_text_colour_default()
{
    COLORREF cr = NULL;
    if (!(m_theme && IsThemePartDefined(m_theme, LVP_GROUPHEADER, NULL)
            && SUCCEEDED(GetThemeColor(m_theme, LVP_GROUPHEADER, LVGH_OPEN, TMT_HEADING1TEXTCOLOR, &cr))))
        cr = GetSysColor(COLOR_WINDOWTEXT);
    return cr;
}

bool ListView::get_group_text_colour_default(COLORREF& cr)
{
    cr = NULL;
    return m_theme && IsThemePartDefined(m_theme, LVP_GROUPHEADER, NULL)
        && SUCCEEDED(GetThemeColor(m_theme, LVP_GROUPHEADER, LVGH_OPEN, TMT_HEADING1TEXTCOLOR, &cr));
}

void lv::DefaultRenderer::render_group(ColourData p_data, HDC dc, HTHEME theme, size_t item_index, size_t group_index,
    std::string_view text, int indentation, t_size level, RECT rc)
{
    COLORREF cr = p_data.m_group_text;

    int text_right = NULL;

    render_group_background(p_data, dc, &rc);
    uih::text_out_colours_tab(dc, text.data(), text.size(),
        uih::scale_dpi_value(1) + indentation * gsl::narrow<int>(level), uih::scale_dpi_value(3), &rc, false, cr, false,
        false, true, uih::ALIGN_LEFT, nullptr, true, true, &text_right);

    auto line_height = scale_dpi_value(1);
    auto line_top = rc.top + RECT_CY(rc) / 2 - line_height / 2;
    RECT rc_line = {
        text_right + scale_dpi_value(7),
        line_top,
        rc.right - scale_dpi_value(4),
        line_top + line_height,
    };

    if (rc_line.right > rc_line.left) {
        render_group_line(p_data, dc, theme, &rc_line);
    }
}

void lv::DefaultRenderer::render_item(RendererContext context, t_size index, std::vector<RendererSubItem> sub_items,
    int indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus, bool b_focused,
    RECT rc)
{
    int theme_state = NULL;
    if (b_selected) {
        if (b_highlight || b_focused && b_window_focused)
            theme_state = LISS_HOTSELECTED;
        else if (b_window_focused)
            theme_state = LISS_SELECTED;
        else
            theme_state = LISS_SELECTEDNOTFOCUS;
    } else if (b_highlight) {
        theme_state = LISS_HOT;
    }

    // NB Third param of IsThemePartDefined "must be 0". But this works.
    bool b_themed = context.list_view_theme && context.colours.m_themed
        && IsThemePartDefined(context.list_view_theme, LVP_LISTITEM, theme_state);

    COLORREF cr_text = NULL;
    if (b_themed && theme_state) {
        cr_text = GetThemeSysColor(context.list_view_theme, b_selected ? COLOR_BTNTEXT : COLOR_WINDOWTEXT);
        ;
        {
            if (IsThemeBackgroundPartiallyTransparent(context.list_view_theme, LVP_LISTITEM, theme_state))
                DrawThemeParentBackground(context.wnd, context.dc, &rc);
            DrawThemeBackground(context.list_view_theme, context.dc, LVP_LISTITEM, theme_state, &rc, nullptr);
        }
    } else {
        cr_text = b_selected
            ? (b_window_focused ? context.colours.m_selection_text : context.colours.m_inactive_selection_text)
            : context.colours.m_text;
        FillRect(context.dc, &rc,
            gdi_object_t<HBRUSH>::ptr_t(
                CreateSolidBrush(b_selected ? (b_window_focused ? context.colours.m_selection_background
                                                                : context.colours.m_inactive_selection_background)
                                            : context.colours.m_background)));
    }
    RECT rc_subitem = rc;

    for (size_t column_index{0}; column_index < sub_items.size(); ++column_index) {
        auto& sub_item = sub_items[column_index];
        rc_subitem.right = rc_subitem.left + sub_item.width;
        text_out_colours_tab(context.dc, sub_item.text.data(), sub_item.text.size(),
            scale_dpi_value(1) + (column_index == 0 ? indentation : 0), scale_dpi_value(3), &rc_subitem, b_selected,
            cr_text, true, true, true, sub_item.alignment);
        rc_subitem.left = rc_subitem.right;
    }

    if (b_focused) {
        render_focus_rect(context, should_hide_focus, rc);
    }
}

void lv::DefaultRenderer::render_focus_rect(RendererContext context, bool should_hide_focus, RECT rc) const
{
    const auto use_themed_rect = context.colours.m_themed && !context.colours.m_use_custom_active_item_frame
        && context.items_view_theme
        && IsThemePartDefined(
            context.items_view_theme, theming::items_view_part_focus_rect, theming::items_view_state_focus_rect_normal);

    if (use_themed_rect) {
        DrawThemeBackground(context.items_view_theme, context.dc, theming::items_view_part_focus_rect,
            theming::items_view_state_focus_rect_normal, &rc, nullptr);

        return;
    }

    if (context.colours.m_themed && context.list_view_theme
        && IsThemePartDefined(context.list_view_theme, LVP_LISTITEM, LISS_SELECTED)) {
        MARGINS margins{};

        const auto hr = GetThemeMargins(
            context.list_view_theme, context.dc, LVP_LISTITEM, LISS_SELECTED, TMT_CONTENTMARGINS, nullptr, &margins);
        if (SUCCEEDED(hr)) {
            rc.left += margins.cxLeftWidth;
            rc.top += margins.cxRightWidth;
            rc.bottom -= margins.cyBottomHeight;
            rc.right -= margins.cxRightWidth;
        }
    }

    if (context.colours.m_use_custom_active_item_frame) {
        draw_rect_outline(context.dc, rc, context.colours.m_active_item_frame, scale_dpi_value(1));
    } else if (!should_hide_focus) {
        // We only obey should_hide_focus for traditional dotted focus rectangles, similar to how
        // Windows behaves
        DrawFocusRect(context.dc, &rc);
    }
}

void lv::DefaultRenderer::render_background(ColourData p_data, HDC dc, const RECT* rc)
{
    FillRect(dc, rc, gdi_object_t<HBRUSH>::ptr_t(CreateSolidBrush(p_data.m_background)));
}

int ListView::get_text_width(const char* text, t_size length)
{
    int ret = 0;
    HDC hdc = GetDC(get_wnd());
    if (hdc) {
        HFONT fnt_old = SelectFont(hdc, m_font);
        ret = uih::get_text_width(hdc, text, strlen(text));
        SelectFont(hdc, fnt_old);
        ReleaseDC(get_wnd(), hdc);
    }
    return ret;
}

bool ListView::is_item_clipped(t_size index, t_size column)
{
    HDC hdc = GetDC(get_wnd());
    if (!hdc)
        return false;

    pfc::string8 text = get_item_text(index, column);
    HFONT fnt_old = SelectFont(hdc, m_font);
    int width = uih::get_text_width_colour(hdc, text, text.length());
    SelectFont(hdc, fnt_old);
    ReleaseDC(get_wnd(), hdc);
    const auto col_width = m_columns[column].m_display_size;
    // if (column == 0) width += get_total_indentation();

    return (width + scale_dpi_value(3) * 2 + scale_dpi_value(1) > col_width);
    // return (width+2+(columns[col]->align == ALIGN_LEFT ? 2 : columns[col]->align == ALIGN_RIGHT ? 1 : 0) >
    // col_width);//we use 3 for the spacing, 1 for column divider
}

} // namespace uih
