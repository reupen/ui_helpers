#include "stdafx.h"

#include "list_view.h"
#include "../direct_write_text_out.h"

using namespace std::string_view_literals;
using namespace uih::literals::spx;

namespace uih {

namespace {

std::tuple<int, int> calculate_tooltip_window_offset(const RECT& rect)
{
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{sizeof(MONITORINFO)};
    if (!GetMonitorInfoW(monitor, &mi))
        return {};

    int x_offset{};
    int y_offset{};

    const auto [clip_left, clip_top, clip_right, clip_bottom] = mi.rcWork;

    if (rect.left < clip_left)
        x_offset = clip_left - rect.left;
    else if (rect.right > clip_right)
        x_offset = clip_right - rect.right;

    if (rect.top < clip_top)
        y_offset = clip_top - rect.top;
    else if (rect.bottom > clip_bottom)
        y_offset = clip_bottom - rect.bottom;

    return std::make_tuple(x_offset, y_offset);
}

} // namespace

void ListView::set_show_tooltips(bool b_val)
{
    m_show_tooltips = b_val;
}

void ListView::set_limit_tooltips_to_clipped_items(bool b_val)
{
    m_limit_tooltips_to_clipped_items = b_val;
}

void ListView::create_tooltip(/*size_t index, size_t column, */ const char* str)
{
    destroy_tooltip();

    m_wnd_tooltip = CreateWindowEx(WS_EX_TRANSPARENT, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, get_wnd(), nullptr, wil::GetModuleInstanceHandle(),
        nullptr);

    if (!m_wnd_tooltip)
        return;

    set_tooltip_window_theme();

    if (IsThemeActive() && IsAppThemed())
        m_tooltip_theme.reset(OpenThemeData(m_wnd_tooltip, L"Tooltip"));

    uih::subclass_window(m_wnd_tooltip, [this](auto _, auto wnd, auto msg, auto wp, auto lp) {
        if (msg == WM_THEMECHANGED && IsThemeActive() && IsAppThemed())
            m_tooltip_theme.reset(OpenThemeData(m_wnd_tooltip, L"Tooltip"));

        return std::nullopt;
    });

    RECT rect;
    GetClientRect(get_wnd(), &rect);

    pfc::stringcvt::string_wide_from_utf8 wstr(str);
    TOOLINFO ti{};

    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_TRANSPARENT | TTF_SUBCLASS;
    ti.hwnd = get_wnd();
    ti.hinst = wil::GetModuleInstanceHandle();
    ti.uId = IDC_TOOLTIP;
    ti.lpszText = const_cast<wchar_t*>(wstr.get_ptr());
    ti.rect = rect;

    tooltip_add_tool(m_wnd_tooltip, &ti);
}

void ListView::destroy_tooltip()
{
    if (m_wnd_tooltip) {
        DestroyWindow(m_wnd_tooltip);
        m_wnd_tooltip = nullptr;
    }
    m_tooltip_theme.reset();
    m_tooltip_last_index = -1;
    m_tooltip_last_column = -1;
}

void ListView::set_tooltip_window_theme() const
{
    if (!m_wnd_tooltip)
        return;

    SetWindowTheme(m_wnd_tooltip, m_use_dark_mode ? L"DarkMode_Explorer" : nullptr, nullptr);
}

void ListView::calculate_tooltip_position(size_t item_index, size_t column_index, std::string_view text)
{
    int cx = get_total_indentation();

    for (auto index : std::ranges::views::iota(size_t{}, column_index))
        cx += m_columns[index].m_display_size;

    POINT top_left;
    top_left.x = cx + 1_spx + 3_spx - m_horizontal_scroll_position;
    top_left.y = (get_item_position(item_index) - m_scroll_position) + get_items_top();
    ClientToScreen(get_wnd(), &top_left);

    const auto& column = m_columns[column_index];

    auto utf16_text = mmh::to_utf16(text);
    const auto text_width = m_items_text_format->measure_text_width(utf16_text);

    const auto max_width = std::max(0.0f,
        static_cast<float>(column.m_display_size - 1_spx - 3_spx * 2) / direct_write::get_default_scaling_factor());

    // Work around DirectWrite not rendering trailing whitespace
    // for centre- and right-aligned text
    if (column.m_alignment != ALIGN_LEFT)
        utf16_text.push_back(L'\u200b');

    const auto alignment = direct_write::get_text_alignment(column.m_alignment);
    const auto has_newline = utf16_text.find_first_of(L"\r\n"sv) != std::string_view::npos;
    const auto metrics
        = m_items_text_format->measure_text_position(utf16_text, m_item_height, max_width, !has_newline, alignment);

    m_tooltip_window_x_offset = 0;
    m_tooltip_window_y_offset = 0;
    m_tooltip_text_left_offset = metrics.left_remainder_dip;

    const auto placement_y = top_left.y + (has_newline ? metrics.top : 0);

    m_tooltip_text_render_rect = {top_left.x + metrics.left, placement_y, top_left.x + metrics.left + text_width,
        placement_y + (has_newline ? metrics.height : m_item_height)};

    m_tooltip_internal_rect.top = top_left.y + metrics.top;
    m_tooltip_internal_rect.bottom = m_tooltip_internal_rect.top + metrics.height + 2_spx;

    if (!has_newline)
        m_tooltip_internal_rect.bottom = std::min(m_tooltip_internal_rect.bottom, top_left.y + m_item_height);

    m_tooltip_internal_rect.left = top_left.x + metrics.left;
    m_tooltip_internal_rect.right = m_tooltip_internal_rect.left + text_width;
}

std::optional<LRESULT> ListView::on_wm_notify_tooltip(LPNMHDR lpnm)
{
    switch (lpnm->code) {
    case NM_CUSTOMDRAW: {
        const auto lpttcd = reinterpret_cast<LPNMTTCUSTOMDRAW>(lpnm);
        switch (lpttcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT: {
            if (!m_tooltip_theme) {
                const auto back_colour
                    = static_cast<COLORREF>(SendMessage(lpttcd->nmcd.hdr.hwndFrom, TTM_GETTIPBKCOLOR, 0, 0));
                SetTextColor(lpttcd->nmcd.hdc, back_colour);
                return CDRF_NOTIFYPOSTPAINT;
            }

            RECT rc_background = lpttcd->nmcd.rc;
            SendMessage(m_wnd_tooltip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc_background));

            DrawThemeBackground(
                m_tooltip_theme.get(), lpttcd->nmcd.hdc, TTP_STANDARD, TTSS_NORMAL, &rc_background, &rc_background);

            render_tooltip_text(lpttcd->nmcd.hdr.hwndFrom, lpttcd->nmcd.hdc, GetTextColor(lpttcd->nmcd.hdc));

            return CDRF_SKIPDEFAULT;
        }
        case CDDS_POSTPAINT: {
            if (!m_tooltip_theme) {
                const auto text_colour
                    = static_cast<COLORREF>(SendMessage(lpttcd->nmcd.hdr.hwndFrom, TTM_GETTIPTEXTCOLOR, 0, 0));

                render_tooltip_text(lpttcd->nmcd.hdr.hwndFrom, lpttcd->nmcd.hdc, text_colour);
            }

            return CDRF_DODEFAULT;
        }
        }
        break;
    }
    case TTN_SHOW: {
        RECT rc = m_tooltip_internal_rect;

        SendMessage(m_wnd_tooltip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));

        std::tie(m_tooltip_window_x_offset, m_tooltip_window_y_offset) = calculate_tooltip_window_offset(rc);
        OffsetRect(&rc, m_tooltip_window_x_offset, m_tooltip_window_y_offset);

        SetWindowPos(m_wnd_tooltip, nullptr, rc.left, rc.top, RECT_CX(rc), RECT_CY(rc), SWP_NOZORDER | SWP_NOACTIVATE);
        return TRUE;
    }
    }
    return {};
}

void ListView::render_tooltip_text(HWND wnd, HDC dc, COLORREF colour) const
{
    if (!m_items_text_format)
        return;

    auto text = get_window_text(wnd);

    RECT rc_text = m_tooltip_text_render_rect;
    OffsetRect(&rc_text, m_tooltip_window_x_offset, m_tooltip_window_y_offset);
    MapWindowPoints(HWND_DESKTOP, wnd, reinterpret_cast<LPPOINT>(&rc_text), 2);

    if (m_items_text_format) {
        try {
            const auto max_width = direct_write::px_to_dip(gsl::narrow_cast<float>(wil::rect_width(rc_text)));
            const auto max_height = direct_write::px_to_dip(gsl::narrow_cast<float>(wil::rect_height(rc_text)));

            const auto text_layout = m_items_text_format->create_text_layout(
                text, max_width, max_height, false, DWRITE_TEXT_ALIGNMENT_LEADING);

            text_layout.render_with_transparent_background(wnd, dc, rc_text, colour, false, m_tooltip_text_left_offset);
        }
        CATCH_LOG()
    }
}

} // namespace uih
