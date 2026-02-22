#include "stdafx.h"

#include "list_view.h"

using namespace std::chrono_literals;

namespace uih {

lv::SavedScrollPosition ListView::save_scroll_position() const
{
    // Work out where the scroll position is proportionally in the item at the
    // scroll position, or between the previous and next items if the scroll
    // position is between items

    const auto scroll_position = m_smooth_scroll_helper->current_target(ScrollAxis::Vertical);
    const auto previous_item_index = get_item_at_or_before(scroll_position);
    const auto next_item_index = get_item_at_or_after(scroll_position);
    const auto next_item_bottom = get_item_position_bottom(next_item_index);
    const auto previous_item_top = get_item_position(previous_item_index);

    // If next_item_bottom == previous_item_bottom == 0, there are probably no items
    const auto proportional_position = next_item_bottom != previous_item_top
        ? static_cast<double>(scroll_position - previous_item_top)
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
            absolute_scroll(item_start_position - stuck_headers_height());
            break;
        case ItemVisibility::ObscuredBelow:
            absolute_scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        default:
            absolute_scroll(item_start_position - (item_area_height - item_height) / 2);
        }
        break;
    case EnsureVisibleMode::PreferMinimalScrolling:
        switch (item_visibility) {
        case ItemVisibility::ObscuredAbove:
        case ItemVisibility::AboveViewport:
            absolute_scroll(item_start_position - stuck_headers_height());
            break;
        case ItemVisibility::ObscuredBelow:
        case ItemVisibility::BelowViewport:
            absolute_scroll(item_end_position - item_area_height);
            break;
        case ItemVisibility::FullyVisible:
            break;
        }
    }
}

void ListView::absolute_scroll(
    int new_position, ScrollAxis axis, bool supress_smooth_scroll, SmoothScrollHelper::Duration duration)
{
    if (m_use_smooth_scroll && !supress_smooth_scroll) {
        const auto clamped_position = clamp_scroll_position(get_wnd(), axis, new_position);
        m_smooth_scroll_helper->absolute_scroll(axis, clamped_position, duration);
        return;
    }

    if (supress_smooth_scroll)
        m_smooth_scroll_helper->abandon_animation(axis);

    internal_scroll(new_position, axis);
}

void ListView::delta_scroll(int delta, ScrollAxis axis, bool supress_smooth_scroll)
{
    if (m_use_smooth_scroll && !supress_smooth_scroll) {
        const auto clamped_delta = clamp_scroll_delta(get_wnd(), axis, delta);
        m_smooth_scroll_helper->delta_scroll(axis, clamped_delta);
        return;
    }

    if (supress_smooth_scroll)
        m_smooth_scroll_helper->abandon_animation(axis);

    internal_scroll(get_scroll_position(axis) + delta, axis);
}

void ListView::internal_scroll(int new_position, ScrollAxis axis)
{
    if (!m_initialised)
        return;

    auto& scroll_position = get_scroll_position(axis);
    const int original_scroll_position = scroll_position;

    scroll_position = set_scroll_position(get_wnd(), axis, original_scroll_position, new_position);

    if (scroll_position == original_scroll_position)
        return;

    const auto items_rect = get_items_rect();
    int dx = 0;
    int dy = 0;
    (axis == ScrollAxis::Vertical ? dy : dx) = original_scroll_position - scroll_position;

    hide_tooltip();

    if (axis == ScrollAxis::Horizontal)
        reposition_header();

    std::vector<RECT> invalidate_after_scroll_window{};

    if (m_group_count > 0 && get_show_group_info_area() && m_is_group_info_area_sticky && dy != 0) {
        const auto first_items
            = std::unordered_set{gsl::narrow_cast<size_t>(get_first_or_previous_visible_item(original_scroll_position)),
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
        RECT old_stuck_headers_rect{
            items_rect.left, items_rect.top, items_rect.right, items_rect.top + old_stuck_group_headers_info.height};

        const auto new_stuck_group_headers_info = get_stuck_group_headers_info();
        RECT new_stuck_headers_rect{
            items_rect.left, items_rect.top, items_rect.right, items_rect.top + new_stuck_group_headers_info.height};

        if (old_stuck_group_headers_info != new_stuck_group_headers_info) {
            RECT invalidate_rect{};

            if (IntersectRect(&invalidate_rect, &items_rect, &old_stuck_headers_rect))
                RedrawWindow(get_wnd(), &invalidate_rect, nullptr, RDW_INVALIDATE);

            if (IntersectRect(&invalidate_rect, &items_rect, &new_stuck_headers_rect))
                invalidate_after_scroll_window.push_back(invalidate_rect);
        }

        clip_rect.top += dy > 0 ? old_stuck_group_headers_info.height : new_stuck_group_headers_info.height;
    }

    RECT rc_invalidated{};

    const int rgn_type
        = ScrollWindowEx(get_wnd(), dx, dy, &clip_rect, &clip_rect, nullptr, &rc_invalidated, SW_INVALIDATE);

    const auto skip_update_now = dx == 0 && rgn_type == SIMPLEREGION && rc_invalidated == clip_rect;

    for (const auto& rect : invalidate_after_scroll_window) {
        RedrawWindow(get_wnd(), &rect, nullptr, RDW_INVALIDATE);
    }

    if (!skip_update_now)
        RedrawWindow(get_wnd(), nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void ListView::scroll_from_scroll_bar(short scroll_bar_command, ScrollAxis axis)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(get_wnd(), scroll_axis_to_win32_type(axis), &si);

    const auto unclamped_current_target_position = m_smooth_scroll_helper->current_target(axis);
    const auto current_target_position = std::clamp(unclamped_current_target_position, si.nMin, si.nMax);

    switch (scroll_bar_command) {
    case SB_LINEDOWN:
        delta_scroll(m_item_height, axis);
        break;
    case SB_LINEUP:
        delta_scroll(-m_item_height, axis);
        break;
    case SB_PAGEUP: {
        int delta = -gsl::narrow_cast<int>(si.nPage);

        if (axis == ScrollAxis::Vertical && current_target_position > si.nMin)
            delta += get_stuck_group_headers_height(current_target_position);

        delta_scroll(delta, axis);
        break;
    }
    case SB_PAGEDOWN: {
        int delta = gsl::narrow_cast<int>(si.nPage);

        if (axis == ScrollAxis::Vertical)
            delta -= get_stuck_group_headers_height(std::min(si.nMax, current_target_position + delta));

        delta_scroll(delta, axis);
        break;
    }
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        absolute_scroll(si.nTrackPos, axis, false, 50.ms);
        break;
    case SB_BOTTOM:
        absolute_scroll(si.nMax, axis);
        break;
    case SB_TOP:
        absolute_scroll(si.nMin, axis);
        break;
    default:
        break;
    }
}

int ListView::get_items_viewport_height() const
{
    const auto items_rect = get_items_rect();
    return static_cast<int>(wil::rect_height(items_rect));
}

void ListView::update_vertical_scroll_info(bool redraw, std::optional<int> new_vertical_position)
{
    m_smooth_scroll_helper->abandon_animation(ScrollAxis::Vertical, !new_vertical_position);

    const auto old_scroll_position = m_scroll_position;

    SCROLLINFO scroll{};
    scroll.cbSize = sizeof(SCROLLINFO);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;

    const auto count = m_items.size();
    scroll.nMax = count > 0 ? get_item_group_bottom(count - 1) : 0;

    scroll.nPage = get_items_viewport_height();
    scroll.nPos = new_vertical_position.value_or(m_scroll_position);

    m_scroll_position = SetScrollInfo(get_wnd(), SB_VERT, &scroll, redraw);

    if (new_vertical_position || m_scroll_position != old_scroll_position)
        invalidate_all();
}

void ListView::update_horizontal_scroll_info(bool redraw)
{
    m_smooth_scroll_helper->abandon_animation(ScrollAxis::Horizontal);

    auto rc = get_items_rect();

    const auto old_scroll_position = m_horizontal_scroll_position;
    const auto cx = get_columns_display_width() + get_total_indentation();

    SCROLLINFO horizontal_si{};
    horizontal_si.cbSize = sizeof(SCROLLINFO);
    horizontal_si.fMask = SIF_RANGE | SIF_PAGE;
    horizontal_si.nMin = 0;
    horizontal_si.nMax = m_autosize || cx == 0 ? 0 : cx - 1;
    horizontal_si.nPage = m_autosize ? 0 : wil::rect_width(rc);
    bool b_old_show = (GetWindowLongPtr(get_wnd(), GWL_STYLE) & WS_HSCROLL) != 0;

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
