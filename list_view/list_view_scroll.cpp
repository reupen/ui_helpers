#include "stdafx.h"

#include "list_view.h"

namespace uih {

lv::SavedScrollPosition ListView::save_scroll_position() const
{
    // Work out where the scroll position is proportionally in the item at the
    // scroll position, or between the previous and next items if the scroll
    // position is between items

    const auto previous_item_index = get_item_at_or_before(m_scroll_position);
    const auto next_item_index = get_item_at_or_after(m_scroll_position);
    const auto next_item_bottom = get_item_position_bottom(next_item_index);
    const auto previous_item_top = get_item_position(previous_item_index);

    // If next_item_bottom == previous_item_bottom == 0, there are probably no items
    const auto proportional_position = next_item_bottom != previous_item_top
        ? static_cast<double>(m_scroll_position - previous_item_top)
            / static_cast<double>(next_item_bottom - previous_item_top)
        : 0.0;

    return {previous_item_index, next_item_index, proportional_position};
}

void ListView::restore_scroll_position(const lv::SavedScrollPosition& position)
{
    const auto new_next_item_bottom = get_item_position_bottom(position.next_item_index);
    const auto new_previous_item_top = get_item_position(position.previous_item_index);
    const auto new_position
        = position.proportional_position * static_cast<double>(new_next_item_bottom - new_previous_item_top)
        + new_previous_item_top;
    const auto new_position_rounded = gsl::narrow<int>(std::lround(new_position));

    update_scroll_info(true, true, true, new_position_rounded);
}

void ListView::ensure_visible(size_t index, EnsureVisibleMode mode)
{
    if (m_ensure_visible_suspended || index >= m_items.size())
        return;

    const auto item_visibility = get_item_visibility(index);

    const auto item_area_height = get_item_area_height();
    const auto item_height = get_item_height(index);
    const auto item_start_position = get_item_position(index);
    const auto stuck_headers_height = [&] {
        if (!m_are_group_headers_sticky || m_group_count == 0)
            return 0;

        const auto is_new_group = get_is_new_group(index);

        return gsl::narrow<int>(get_cumulative_item_display_group_count(index)) * m_group_height
            + (is_new_group ? get_leaf_group_header_bottom_margin() : get_stuck_leaf_group_header_bottom_margin());
    };
    const auto item_end_position = get_item_position_bottom(index);

    switch (mode) {
    case EnsureVisibleMode::PreferCentringItem:
        switch (item_visibility) {
        case ItemVisibility::ObscuredAbove:
            scroll(item_start_position - stuck_headers_height());
            break;
        case ItemVisibility::ObscuredBelow:
            scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        default:
            scroll(item_start_position - (item_area_height - item_height) / 2);
        }
        break;
    case EnsureVisibleMode::PreferMinimalScrolling:
        switch (item_visibility) {
        case ItemVisibility::ObscuredAbove:
        case ItemVisibility::AboveViewport:
            scroll(item_start_position - stuck_headers_height());
            break;
        case ItemVisibility::ObscuredBelow:
        case ItemVisibility::BelowViewport:
            scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        }
    }
}

void ListView::scroll(int position, bool b_horizontal, bool suppress_scroll_window)
{
    const INT scroll_bar_type = b_horizontal ? SB_HORZ : SB_VERT;
    int& scroll_position = b_horizontal ? m_horizontal_scroll_position : m_scroll_position;
    const int original_scroll_position = scroll_position;

    SCROLLINFO scroll_info{};
    scroll_info.cbSize = sizeof(SCROLLINFO);
    scroll_info.fMask = SIF_POS;
    scroll_info.nPos = position;

    if (scroll_position == scroll_info.nPos)
        return;

    scroll_position = SetScrollInfo(get_wnd(), scroll_bar_type, &scroll_info, true);

    const auto items_rect = get_items_rect();
    int dx = 0;
    int dy = 0;
    (b_horizontal ? dx : dy) = original_scroll_position - scroll_position;

    if (dx != 0 || dy != 0)
        hide_tooltip();

    if (b_horizontal)
        reposition_header();

    std::vector<RECT> invalidate_after_scroll_window{};

    if (suppress_scroll_window) {
        invalidate_all();
    } else {
        if (m_group_count > 0 && get_show_group_info_area() && m_is_group_info_area_sticky && dy != 0) {
            const auto first_items = std::unordered_set{
                gsl::narrow_cast<size_t>(get_first_or_previous_visible_item(original_scroll_position)),
                gsl::narrow_cast<size_t>(get_first_or_previous_visible_item(m_scroll_position))};

            const auto first_group_items = first_items | ranges::views::transform([this](auto index) -> size_t {
                return std::get<0>(get_item_group_range(index, m_group_count - 1));
            }) | ranges::to<std::unordered_set>;

            for (const auto index : first_group_items) {
                if (index >= m_items.size())
                    continue;

                const auto old_rect = get_item_group_info_area_render_rect(index, items_rect, original_scroll_position);
                const RECT adjusted_old_rect{old_rect.left, old_rect.top + dy, old_rect.right, old_rect.bottom + dy};

                const auto new_rect = get_item_group_info_area_render_rect(index, items_rect);

                if (adjusted_old_rect == new_rect)
                    continue;

                RECT invalidate_rect{};

                if (IntersectRect(&invalidate_rect, &items_rect, &old_rect))
                    RedrawWindow(get_wnd(), &invalidate_rect, nullptr, RDW_INVALIDATE);

                if (IntersectRect(&invalidate_rect, &items_rect, &new_rect))
                    invalidate_after_scroll_window.push_back(invalidate_rect);
            }
        }

        RECT clip_rect{items_rect};

        if (m_group_count > 0 && m_are_group_headers_sticky && dy != 0) {
            const auto old_stuck_group_headers_info = get_stuck_group_headers_info(original_scroll_position);
            RECT old_stuck_headers_rect{items_rect.left, items_rect.top, items_rect.right,
                items_rect.top + old_stuck_group_headers_info.height};

            const auto new_stuck_group_headers_info = get_stuck_group_headers_info();
            RECT new_stuck_headers_rect{items_rect.left, items_rect.top, items_rect.right,
                items_rect.top + new_stuck_group_headers_info.height};

            if (old_stuck_group_headers_info != new_stuck_group_headers_info) {
                RECT invalidate_rect{};

                if (IntersectRect(&invalidate_rect, &items_rect, &old_stuck_headers_rect))
                    RedrawWindow(get_wnd(), &invalidate_rect, nullptr, RDW_INVALIDATE);

                if (IntersectRect(&invalidate_rect, &items_rect, &new_stuck_headers_rect))
                    invalidate_after_scroll_window.push_back(invalidate_rect);
            }

            clip_rect.top += new_stuck_group_headers_info.height;
        }

        RECT rc_invalidated{};

        const int rgn_type
            = ScrollWindowEx(get_wnd(), dx, dy, &clip_rect, &clip_rect, nullptr, &rc_invalidated, SW_INVALIDATE);

        const auto skip_update_now = dx == 0 && rgn_type == SIMPLEREGION && rc_invalidated == items_rect;

        for (const auto& rect : invalidate_after_scroll_window) {
            RedrawWindow(get_wnd(), &rect, nullptr, RDW_INVALIDATE);
        }

        if (!skip_update_now)
            RedrawWindow(get_wnd(), nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
}

void ListView::scroll_from_scroll_bar(short scroll_bar_command, bool b_horizontal)
{
    const int scroll_bar_type = b_horizontal ? SB_HORZ : SB_VERT;
    int& scroll_position = b_horizontal ? m_horizontal_scroll_position : m_scroll_position;

    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(get_wnd(), scroll_bar_type, &si);

    int pos{};
    if (scroll_bar_command == SB_LINEDOWN)
        pos = (std::min)(scroll_position + m_item_height, si.nMax);
    else if (scroll_bar_command == SB_LINEUP)
        pos = (std::max)(scroll_position - m_item_height, si.nMin);
    else if (scroll_bar_command == SB_PAGEUP) {
        pos = scroll_position - si.nPage;

        if (scroll_position > si.nMin)
            pos += get_stuck_group_headers_height();
    } else if (scroll_bar_command == SB_PAGEDOWN) {
        pos = scroll_position + si.nPage;
        pos -= get_stuck_group_headers_height(std::min(si.nMax, pos));
    } else if (scroll_bar_command == SB_THUMBTRACK)
        pos = si.nTrackPos;
    else if (scroll_bar_command == SB_THUMBPOSITION)
        pos = si.nTrackPos;
    else if (scroll_bar_command == SB_BOTTOM)
        pos = si.nMax;
    else if (scroll_bar_command == SB_TOP)
        pos = si.nMin;
    else // SB_ENDSCROLL
        return;

    scroll(pos, b_horizontal);
}

int ListView::get_items_viewport_height() const
{
    const auto items_rect = get_items_rect();
    return static_cast<int>(wil::rect_height(items_rect));
}

void ListView::update_vertical_scroll_info(bool redraw, std::optional<int> new_vertical_position)
{
    const auto old_scroll_position = m_scroll_position;

    SCROLLINFO scroll{};
    scroll.cbSize = sizeof(SCROLLINFO);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;
    const auto count = m_items.size();
    scroll.nMax = count ? get_item_group_bottom(count - 1) : 0;
    scroll.nPage = get_items_viewport_height();
    scroll.nPos = new_vertical_position.value_or(m_scroll_position);

    m_scroll_position = SetScrollInfo(get_wnd(), SB_VERT, &scroll, redraw);

    if (new_vertical_position || m_scroll_position != old_scroll_position)
        invalidate_all();
}

void ListView::update_horizontal_scroll_info(bool redraw)
{
    auto rc = get_items_rect();

    const auto old_scroll_position = m_horizontal_scroll_position;
    const auto cx = get_columns_display_width() + get_total_indentation();

    SCROLLINFO horizontal_si{};
    horizontal_si.cbSize = sizeof(SCROLLINFO);
    horizontal_si.fMask = SIF_RANGE | SIF_PAGE;
    horizontal_si.nMin = 0;
    horizontal_si.nMax = m_autosize ? 0 : (cx ? cx - 1 : 0);
    horizontal_si.nPage = m_autosize ? 0 : wil::rect_width(rc);
    bool b_old_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_HSCROLL) != 0;
    ;
    m_horizontal_scroll_position = SetScrollInfo(get_wnd(), SB_HORZ, &horizontal_si, redraw);

    bool b_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_HSCROLL) != 0;

    if (m_horizontal_scroll_position != old_scroll_position) {
        invalidate_all();
        reposition_header();
    }

    if (b_old_show != b_show) {
        rc = get_items_rect();

        SCROLLINFO vertical_si{};
        vertical_si.cbSize = sizeof(SCROLLINFO);
        vertical_si.fMask = SIF_PAGE;
        vertical_si.nPage = get_items_viewport_height();

        m_scroll_position = SetScrollInfo(get_wnd(), SB_VERT, &vertical_si, redraw);
    }
}

void ListView::update_scroll_info(
    bool b_vertical, bool b_horizontal, bool redraw, std::optional<int> new_vertical_position)
{
    // Scroll bar updates can cause WM_SIZE (if one of the scroll bars was
    // shown or hidden), leading to recursive scroll bar updates and
    // misbehaviour
    if (m_scroll_bar_update_in_progress)
        return;

    m_scroll_bar_update_in_progress = true;
    auto _ = gsl::finally([this] { m_scroll_bar_update_in_progress = false; });

    // Note the dependency between the two scroll bars – showing one can cause the other to be required

    if (b_vertical)
        update_vertical_scroll_info(redraw, new_vertical_position);

    if (b_horizontal)
        update_horizontal_scroll_info(redraw);
}

unsigned ListView::calculate_scroll_timer_speed() const
{
    const auto num_visible_items = get_item_area_height() / get_item_height();
    const auto divisor = (std::max)(num_visible_items - 5, 1);
    return std::clamp(25 * 15 / divisor, 10, 100);
}

void ListView::create_timer_scroll_up()
{
    if (!m_timer_scroll_up) {
        SetTimer(get_wnd(), TIMER_SCROLL_UP, calculate_scroll_timer_speed(), nullptr);
        m_timer_scroll_up = true;
    }
}

void ListView::create_timer_scroll_down()
{
    if (!m_timer_scroll_down) {
        SetTimer(get_wnd(), TIMER_SCROLL_DOWN, calculate_scroll_timer_speed(), nullptr);
        m_timer_scroll_down = true;
    }
}
void ListView::destroy_timer_scroll_up()
{
    if (m_timer_scroll_up) {
        KillTimer(get_wnd(), TIMER_SCROLL_UP);
        m_timer_scroll_up = false;
    }
}
void ListView::destroy_timer_scroll_down()
{
    if (m_timer_scroll_down) {
        KillTimer(get_wnd(), TIMER_SCROLL_DOWN);
        m_timer_scroll_down = false;
    }
}
} // namespace uih
