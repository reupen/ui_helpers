#include "stdafx.h"

#include "list_view.h"
#include "text_style.h"
#include "../direct_write_text_out.h"

using namespace std::string_view_literals;
using namespace uih::literals::spx;

namespace uih {

namespace {

std::string clean_tooltip_text(std::string_view text, size_t max_code_points = 2048)
{
    auto cleaned_text = text_style::remove_colour_and_font_codes(text);
    std::ranges::replace(cleaned_text, '\t', ' ');

    if (cleaned_text.size() > max_code_points) {
        size_t code_point_counter{};
        const char* pos{cleaned_text.c_str()};

        while (code_point_counter < max_code_points) {
            if (!pfc::utf8_advance(pos))
                break;

            ++code_point_counter;
        }

        const size_t truncate_num_bytes = pos - cleaned_text.c_str();

        if (truncate_num_bytes < cleaned_text.size()) {
            cleaned_text.resize(truncate_num_bytes);
            cleaned_text += "…";
        }
    }

    return cleaned_text;
}

std::tuple<int, int> calculate_tooltip_window_offset(const RECT& tooltip_rect, const RECT& root_window_rect)
{
    if (tooltip_rect.left >= root_window_rect.left && tooltip_rect.right <= root_window_rect.right
        && tooltip_rect.top >= root_window_rect.top && tooltip_rect.bottom <= root_window_rect.bottom)
        return {};

    HMONITOR monitor = MonitorFromRect(&tooltip_rect, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{sizeof(MONITORINFO)};
    if (!GetMonitorInfo(monitor, &mi))
        return {};

    int x_offset{};
    int y_offset{};

    const auto [clip_left, clip_top, clip_right, clip_bottom] = mi.rcWork;

    if (tooltip_rect.left < clip_left)
        x_offset = clip_left - tooltip_rect.left;
    else if (tooltip_rect.right > clip_right)
        x_offset = clip_right - tooltip_rect.right;

    if (tooltip_rect.top < clip_top)
        y_offset = clip_top - tooltip_rect.top;
    else if (tooltip_rect.bottom > clip_bottom)
        y_offset = clip_bottom - tooltip_rect.bottom;

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

void ListView::create_tooltip()
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

    subclass_window(m_wnd_tooltip, [this](auto _, auto wnd, auto msg, auto wp, auto lp) {
        if (msg == WM_THEMECHANGED && IsThemeActive() && IsAppThemed())
            m_tooltip_theme.reset(OpenThemeData(m_wnd_tooltip, L"Tooltip"));

        return std::nullopt;
    });

    TOOLINFO ti{};

    ti.cbSize = sizeof(TOOLINFO);
    ti.uFlags = TTF_TRANSPARENT | TTF_SUBCLASS;
    ti.hwnd = get_wnd();
    ti.hinst = wil::GetModuleInstanceHandle();
    ti.uId = IDC_TOOLTIP;
    ti.lpszText = m_tooltip_text.data();
    ti.rect = m_tooltip_item_rect;

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

void ListView::reposition_tooltip()
{
    const auto text_width = m_items_text_format->measure_text_width(m_tooltip_text);

    const auto max_width = std::max(0.0f,
        static_cast<float>(wil::rect_width(m_tooltip_item_rect) - 1_spx - 3_spx * 2)
            / direct_write::get_default_scaling_factor());

    const auto alignment = direct_write::get_text_alignment(m_tooltip_alignment);
    const auto has_newline = m_tooltip_text.find_first_of(L"\r\n"sv) != std::string_view::npos;
    const auto metrics
        = m_items_text_format->measure_text_position(m_tooltip_text, m_item_height, max_width, !has_newline, alignment);

    m_tooltip_window_x_offset = 0;
    m_tooltip_window_y_offset = 0;
    m_tooltip_text_left_offset = metrics.left_remainder_dip;

    POINT top_left{m_tooltip_item_rect.left, m_tooltip_item_rect.top};
    ClientToScreen(get_wnd(), &top_left);

    RECT tooltip_rect{};
    tooltip_rect.top = top_left.y + metrics.top;
    tooltip_rect.bottom = tooltip_rect.top + metrics.height + 2_spx;

    if (!has_newline)
        tooltip_rect.bottom = std::min(tooltip_rect.bottom, top_left.y + m_item_height);

    tooltip_rect.left = top_left.x + metrics.left;
    tooltip_rect.right = tooltip_rect.left + text_width;

    SendMessage(m_wnd_tooltip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&tooltip_rect));

    RECT root_window_rect{};
    GetWindowRect(GetAncestor(get_wnd(), GA_ROOT), &root_window_rect);

    const auto [x_offset, y_offset] = calculate_tooltip_window_offset(tooltip_rect, root_window_rect);

    const auto text_x = top_left.x + metrics.left + x_offset;
    const auto text_y = top_left.y + (has_newline ? metrics.top : 0) + y_offset;

    m_tooltip_text_render_rect
        = {text_x, text_y, text_x + text_width, text_y + (has_newline ? metrics.height : m_item_height)};

    SetWindowPos(m_wnd_tooltip, nullptr, tooltip_rect.left + x_offset, tooltip_rect.top + y_offset,
        wil::rect_width(tooltip_rect), wil::rect_height(tooltip_rect), SWP_NOZORDER | SWP_NOACTIVATE);
}

void ListView::handle_mousemove_for_tooltip(POINT pt)
{
    if (!m_show_tooltips || !m_items_text_format || pt.y < get_items_top()) {
        destroy_tooltip();
        return;
    }

    HitTestResult hit_result;
    hit_test_ex(pt, hit_result);

    auto _ = wil::scope_exit([&] {
        m_tooltip_last_index = hit_result.index;
        m_tooltip_last_column = hit_result.column;
    });

    if (!(hit_result.category == HitTestCategory::OnUnobscuredItem
            || hit_result.category == HitTestCategory::OnItemObscuredBelow
            || hit_result.category == HitTestCategory::OnItemObscuredAbove)
        || hit_result.column == -1) {
        destroy_tooltip();
        return;
    }

    if (m_tooltip_last_index == hit_result.index && m_tooltip_last_column == hit_result.column)
        return;

    bool is_clipped = is_item_clipped(hit_result.index, hit_result.column);

    if (m_limit_tooltips_to_clipped_items && !is_clipped) {
        destroy_tooltip();
        return;
    }

    const auto cleaned_text = clean_tooltip_text(get_item_text(hit_result.index, hit_result.column));
    m_tooltip_text = mmh::to_utf16(cleaned_text);

    // Work around DirectWrite not rendering trailing whitespace
    // for centre- and right-aligned text
    if (m_columns[hit_result.column].m_alignment != ALIGN_LEFT)
        m_tooltip_text.push_back(L'\u200b');

    calculate_tooltip_position(hit_result.index, hit_result.column);

    create_tooltip();
}

void ListView::set_tooltip_window_theme() const
{
    if (!m_wnd_tooltip)
        return;

    SetWindowTheme(m_wnd_tooltip, m_use_dark_mode ? L"DarkMode_Explorer" : nullptr, nullptr);
}

void ListView::calculate_tooltip_position(size_t item_index, size_t column_index)
{
    int cx = get_total_indentation();

    for (auto index : std::ranges::views::iota(size_t{}, column_index))
        cx += m_columns[index].m_display_size;

    POINT top_left;
    top_left.x = cx + 1_spx + 3_spx - m_horizontal_scroll_position;
    top_left.y = (get_item_position(item_index) - m_scroll_position) + get_items_top();

    const auto& column = m_columns[column_index];

    m_tooltip_item_rect = {top_left.x, top_left.y, top_left.x + column.m_display_size, top_left.y + m_item_height};
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
    case TTN_SHOW:
        reposition_tooltip();
        return TRUE;
    }
    return {};
}

void ListView::render_tooltip_text(HWND wnd, HDC dc, COLORREF colour) const
{
    if (!m_items_text_format)
        return;

    RECT rc_text = m_tooltip_text_render_rect;
    MapWindowPoints(HWND_DESKTOP, wnd, reinterpret_cast<LPPOINT>(&rc_text), 2);

    if (m_items_text_format) {
        try {
            const auto max_width = direct_write::px_to_dip(gsl::narrow_cast<float>(wil::rect_width(rc_text)));
            const auto max_height = direct_write::px_to_dip(gsl::narrow_cast<float>(wil::rect_height(rc_text)));

            const auto text_layout = m_items_text_format->create_text_layout(
                m_tooltip_text, max_width, max_height, false, DWRITE_TEXT_ALIGNMENT_LEADING);

            text_layout.render_with_transparent_background(wnd, dc, rc_text, colour, false, m_tooltip_text_left_offset);
        }
        CATCH_LOG()
    }
}

} // namespace uih
