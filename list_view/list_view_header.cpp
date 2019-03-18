#include "../stdafx.h"

namespace uih {

void ListView::destroy_header()
{
    if (m_wnd_header) {
        DestroyWindow(m_wnd_header);
        m_wnd_header = nullptr;
        m_font_header.release();
    }
}

void ListView::create_header()
{
    // if (m_show_header)
    {
        if (!m_wnd_header) {
            m_font_header = m_lf_header_valid ? CreateFontIndirect(&m_lf_header) : uih::create_icon_font();
            m_wnd_header = CreateWindowEx(0, WC_HEADER, _T("NGLVH"),
                WS_CHILD | (0) | /*(m_autosize ? 0x0800  : NULL) |*/ HDS_HOTTRACK
                    | (m_allow_header_rearrange ? HDS_DRAGDROP : NULL) | HDS_HORZ | HDS_FULLDRAG
                    | (m_sorting_enabled ? HDS_BUTTONS : 0) | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                0, 0, 0, 0, get_wnd(), HMENU(IDC_HEADER), mmh::get_current_instance(), nullptr);
            // SetWindowTheme(m_wnd_header, L"ItemsView", NULL);
            // SendMessage (m_wnd_header, 0x2009, (WPARAM)get_wnd(), NULL);
            SendMessage(m_wnd_header, WM_SETFONT, (WPARAM)m_font_header.get(), MAKELPARAM(FALSE, 0));
            if (m_initialised) {
                build_header();
                ShowWindow(m_wnd_header, SW_SHOWNORMAL);
            }
        }
    }
}

void ListView::set_show_header(bool b_val)
{
    if (b_val != m_show_header) {
        m_show_header = b_val;
        if (m_initialised) {
            if (m_show_header)
                create_header();
            else
                destroy_header();
            on_size();
        }
    }
}

void ListView::reposition_header()
{
    RECT rc;
    GetClientRect(get_wnd(), &rc);
    int cx = RECT_CX(rc);
    RECT rc_header;
    get_header_rect(&rc_header);
    SetWindowPos(m_wnd_header, nullptr, -m_horizontal_scroll_position, 0, cx + m_horizontal_scroll_position,
        RECT_CY(rc_header), SWP_NOZORDER);
}

void ListView::build_header()
{
    if (m_wnd_header) {
        pfc::vartoggle_t<bool> toggle(m_ignore_column_size_change_notification, true);

        t_size header_count = Header_GetItemCount(m_wnd_header);
        for (; header_count; header_count--)
            Header_DeleteItem(m_wnd_header, header_count - 1);

        HDITEM hdi;
        memset(&hdi, 0, sizeof(HDITEM));

        hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
        hdi.fmt = HDF_LEFT | HDF_STRING;

        pfc::string8 name;
        pfc::stringcvt::string_wide_from_utf8 wstr;

        {
            int n, t = m_columns.get_count(), i = 0;
            const auto indentation = get_total_indentation();
            if (indentation /*m_group_count*/) {
                hdi.fmt = HDF_STRING | HDF_LEFT;
                hdi.cchTextMax = 0;
                hdi.pszText = const_cast<wchar_t*>(L"");

                hdi.cxy = indentation;

                Header_InsertItem(m_wnd_header, i++, &hdi);

                m_have_indent_column = true;
            } else
                m_have_indent_column = false;
            for (n = 0; n < t; n++) {
                hdi.fmt = HDF_STRING | HDF_LEFT;
                if (m_columns[n].m_alignment == uih::ALIGN_CENTRE)
                    hdi.fmt |= HDF_CENTER;
                else if (m_columns[n].m_alignment == uih::ALIGN_RIGHT)
                    hdi.fmt |= HDF_RIGHT;
                hdi.cchTextMax = m_columns[n].m_title.length();
                wstr.convert(m_columns[n].m_title);
                hdi.pszText = const_cast<wchar_t*>(wstr.get_ptr());

                hdi.cxy = m_columns[n].m_display_size;

                if (m_sort_column_index == n && m_show_sort_indicators) {
                    hdi.fmt |= (m_sort_direction ? HDF_SORTDOWN : HDF_SORTUP);
                }

                Header_InsertItem(m_wnd_header, i++, &hdi);
            }
        }
    }
}

bool ListView::on_wm_notify_header(LPNMHDR lpnm, LRESULT& ret)
{
    switch (lpnm->code) {
#if 0
        case NM_CUSTOMDRAW:
            {
                LPNMCUSTOMDRAW lpcd = (LPNMCUSTOMDRAW)lpnm;
                switch (lpcd->dwDrawStage)
                {
                case CDDS_PREPAINT :
                    ret = CDRF_NOTIFYITEMDRAW;
                    return true;

                case CDDS_ITEMPREPAINT:
                    {
                        render_background(lpcd->hdc, &lpcd->rc);
                    }

                    SetTextColor(lpcd->hdc, RGB(76,96,128));
                    ret = CDRF_NEWFONT;
                    return true;
                };
            }
            return false;
#endif
    case HDN_BEGINTRACKA:
    case HDN_BEGINTRACKW: {
        LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
        if (m_autosize && (!get_show_group_info_area() || lpnmh->iItem)) {
            ret = TRUE;
            return true;
        }
    } break;
    case HDN_DIVIDERDBLCLICK: {
        LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
        if (!m_autosize) {
            if (lpnmh->iItem != -1 && (!m_have_indent_column || lpnmh->iItem)) {
                t_size realIndex = lpnmh->iItem;
                if (m_have_indent_column)
                    realIndex--;
                if (realIndex < m_columns.get_count()) {
                    HDC dc;
                    dc = GetDC(get_wnd());
                    int size;

                    HFONT fnt_old = SelectFont(dc, m_font.get());

                    int w = 0, n, t = get_item_count();

                    for (n = 0; n < t; n++) {
                        const char* str = get_item_text(n, realIndex);
                        size = uih::get_text_width_colour(dc, str, strlen(str));
                        if (size > w)
                            w = size;
                    }
                    w += uih::scale_dpi_value(3) * 2 + scale_dpi_value(1);

                    SelectFont(dc, fnt_old);
                    ReleaseDC(get_wnd(), dc);

                    m_columns[realIndex].m_size = w;
                    m_columns[realIndex].m_display_size = w;
                    update_header();
                    invalidate_all();
                    notify_on_column_size_change(realIndex, m_columns[realIndex].m_size);
                    update_scroll_info();
                }
            }
        }
    } break;
    case HDN_ITEMCHANGING: {
        if (!m_ignore_column_size_change_notification) {
            LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
            if (lpnmh->pitem->mask & HDI_WIDTH) {
                if (m_have_indent_column && lpnmh->iItem == 0) {
                    int min_indent = get_item_indentation();
                    if (get_show_group_info_area())
                        min_indent += get_default_indentation_step();
                    if (lpnmh->pitem->cxy < min_indent) {
                        ret = TRUE;
                        return true;
                    }
                }
            }
        }
    } break;
    case HDN_ITEMCHANGED: {
        if (!m_ignore_column_size_change_notification) {
            LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
            if (lpnmh->pitem->mask & HDI_WIDTH) {
                if (lpnmh->iItem != -1) {
                    if (m_have_indent_column && lpnmh->iItem == 0) {
                        int new_size = lpnmh->pitem->cxy - get_item_indentation() - get_default_indentation_step();
                        if (new_size >= 0 && new_size != get_group_info_area_width()) {
                            /*set_group_info_area_size(new_size);
                            if (m_autosize)
                            {
                                update_column_sizes();
                                update_header();
                            }*/
                            notify_on_group_info_area_size_change(new_size);
                            /*invalidate_all();
                            update_scroll_info();*/
                        }
                    } else if (!m_autosize) {
                        t_size realIndex = lpnmh->iItem;
                        if (m_have_indent_column)
                            realIndex--;
                        if (realIndex < m_columns.get_count()
                            && m_columns[realIndex].m_display_size != lpnmh->pitem->cxy) {
                            m_columns[realIndex].m_size = lpnmh->pitem->cxy;
                            m_columns[realIndex].m_display_size = lpnmh->pitem->cxy;
                            invalidate_all();
                            notify_on_column_size_change(realIndex, m_columns[realIndex].m_size);
                            on_size();
                        }
                    }
                }
            }
        }
    } break;
    case HDN_ITEMCLICK: {
        LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
        if (lpnmh->iItem != -1 && (!m_have_indent_column || lpnmh->iItem)) {
            t_size realIndex = lpnmh->iItem;
            if (m_have_indent_column)
                realIndex--;
            if (realIndex < m_columns.get_count()) {
                bool des = (realIndex == m_sort_column_index ? !m_sort_direction : false);
                sort_by_column(realIndex, des);
            }
        }
    } break;
    /*case HDN_BEGINDRAG:
        {
            LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
            if (lpnmh->iItem == 0 && m_have_indent_column)
            {
                ret = TRUE;
                return true;
            }
        }
        break;*/
    case HDN_ENDDRAG: {
        LPNMHEADER lpnmh = (LPNMHEADER)lpnm;
        if (lpnmh->iButton == 0) {
            if (lpnmh->pitem && (lpnmh->pitem->mask & HDI_ORDER)) {
                int from = lpnmh->iItem;
                int to = lpnmh->pitem->iOrder;
                if (m_have_indent_column) {
                    from--;
                    to--;
                }
                if (to >= 0 && from >= 0 && from != to) {
                    notify_on_header_rearrange(from, to);
                }
            }
        }
    }
        ret = TRUE;
        return true;
    }
    return false;
}

void ListView::get_header_rect(LPRECT rc) const
{
    if (m_wnd_header)
        *rc = uih::get_relative_rect(m_wnd_header, get_wnd());
    else {
        GetClientRect(get_wnd(), rc);
        rc->bottom = rc->top;
    }
}

unsigned ListView::get_header_height() const
{
    unsigned ret = 0;
    if (m_wnd_header) {
        RECT rc;
        get_header_rect(&rc);
        ret = RECT_CY(rc);
    }
    return ret;
}
unsigned ListView::calculate_header_height()
{
    unsigned rv = 0;
    if (m_wnd_header) {
        HFONT font = (HFONT)SendMessage(m_wnd_header, WM_GETFONT, 0, 0);
        rv = uih::get_font_height(font) + scale_dpi_value(7);
    }
    return rv;
}

void ListView::update_header(bool b_update)
{
    if (m_wnd_header) {
        pfc::vartoggle_t<bool> toggle(m_ignore_column_size_change_notification, true);
        // SendMessage(m_wnd_header, WM_SETREDRAW, FALSE, NULL);
        t_size i, count = m_columns.get_count(), j = 0;
        if (m_have_indent_column) {
            uih::header_set_item_width(m_wnd_header, j, get_total_indentation());
            j++;
        }
        for (i = 0; i < count; i++) {
            uih::header_set_item_width(m_wnd_header, i + j, m_columns[i].m_display_size);
        }
        // SendMessage(m_wnd_header, WM_SETREDRAW, TRUE, NULL);
        // RedrawWindow(m_wnd_header, NULL, NULL, RDW_INVALIDATE|(b_update?RDW_UPDATENOW:0));
    }
}
} // namespace uih
