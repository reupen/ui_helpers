#include "stdafx.h"

#include "list_view.h"

namespace uih {

void ListView::hit_test_ex(POINT pt_client, HitTestResult& result, bool exclude_stuck_headers)
{
    const RECT rc_item_area = get_items_rect();
    const int item_area_height = RECT_CY(rc_item_area);

    result.column = pfc_infinite;
    const int first_column_left = -m_horizontal_scroll_position + get_total_indentation();
    const size_t column_count = m_columns.size();
    int last_column_right = first_column_left;

    for (size_t column_index{0}; column_index < column_count; column_index++) {
        const int left = last_column_right;
        const int right = last_column_right + m_columns[column_index].m_display_size;

        if (pt_client.x >= left && pt_client.x < right)
            result.column = column_index;

        last_column_right = right;
    }

    if (pt_client.y < rc_item_area.top + (exclude_stuck_headers ? get_stuck_group_headers_height() : 0)) {
        result.index = get_first_unobscured_item();
        result.insertion_index = result.index;
        result.category = HitTestCategory::AboveViewport;
        return;
    }

    if (pt_client.y >= rc_item_area.bottom) {
        result.index = get_last_unobscured_item();
        result.insertion_index = result.index;
        result.category = HitTestCategory::BelowViewport;
        return;
    }

    const int header_height = rc_item_area.top;
    const int y_position = pt_client.y - header_height + m_scroll_position;
    const auto vertical_hit_test_result = visible_items_vertical_hit_test(y_position);

    if (vertical_hit_test_result.position_category == VerticalPositionCategory::OnItem) {
        const int item_top = get_item_position(vertical_hit_test_result.item_leftmost);
        const int item_bottom = get_item_position_bottom(vertical_hit_test_result.item_leftmost);
        const int item_height = get_item_height(result.index);

        result.index = vertical_hit_test_result.item_leftmost;
        result.insertion_index = result.index;

        if (pt_client.x < first_column_left)
            result.category = HitTestCategory::LeftOfItem;
        else if (pt_client.x >= last_column_right)
            result.category = HitTestCategory::RightOfItem;
        else if (item_top < m_scroll_position + get_stuck_group_headers_height())
            result.category = HitTestCategory::OnItemObscuredAbove;
        else if (item_bottom > m_scroll_position + item_area_height)
            result.category = HitTestCategory::OnItemObscuredBelow;
        else
            result.category = HitTestCategory::OnUnobscuredItem;

        if (item_height >= 2 && y_position - item_top >= item_height / 2)
            result.insertion_index++;

        return;
    }

    if (vertical_hit_test_result.position_category == VerticalPositionCategory::BetweenGroupHeaderAndItem) {
        assert(m_group_count > 0);

        const auto item_index = vertical_hit_test_result.item_leftmost;

        result.index = item_index > 0 ? item_index - 1 : item_index;
        result.insertion_index
            = vertical_hit_test_result.is_on_stuck_group_header ? get_first_unobscured_item() : item_index;
        result.category = HitTestCategory::NotOnItem;
        return;
    }

    if (vertical_hit_test_result.position_category == VerticalPositionCategory::OnGroupHeader) {
        assert(m_group_count > 0);

        result.category = HitTestCategory::OnGroupHeader;
        result.index = vertical_hit_test_result.item_leftmost;
        result.insertion_index
            = vertical_hit_test_result.is_on_stuck_group_header ? get_first_unobscured_item() : result.index;
        result.group_level = vertical_hit_test_result.group_index;

        assert(result.group_level < m_group_count);

        if (pt_client.x < 0)
            result.category = HitTestCategory::LeftOfGroupHeader;
        else if (pt_client.x >= last_column_right)
            result.category = HitTestCategory::RightOfGroupHeader;

        result.is_stuck = vertical_hit_test_result.is_on_stuck_group_header;

        return;
    }

    result.index = vertical_hit_test_result.item_leftmost;
    result.insertion_index = vertical_hit_test_result.item_rightmost;
    result.category = HitTestCategory::NotOnItem;
}

ListView::ItemVisibility ListView::get_item_visibility(size_t index)
{
    const auto item_area_height = get_item_area_height();
    const auto item_start_position = get_item_position(index);
    const auto item_end_position = get_item_position_bottom(index);
    const auto stuck_headers_height = get_stuck_group_headers_height();

    if (item_end_position < m_scroll_position + stuck_headers_height)
        return ItemVisibility::AboveViewport;

    if (item_start_position >= m_scroll_position + item_area_height)
        return ItemVisibility::BelowViewport;

    // The case where both the top and bottom of the item is obscured is ignored as it has little practical use
    if (item_start_position < m_scroll_position + stuck_headers_height) {
        return ItemVisibility::ObscuredAbove;
    }

    if (item_end_position > m_scroll_position + item_area_height) {
        return ItemVisibility::ObscuredBelow;
    }

    return ItemVisibility::FullyVisible;
}

bool ListView::is_partially_visible(size_t index)
{
    const auto item_visibility = get_item_visibility(index);

    return item_visibility == ItemVisibility::FullyVisible || item_visibility == ItemVisibility::ObscuredAbove
        || item_visibility == ItemVisibility::ObscuredBelow;
}

bool ListView::is_fully_visible(size_t index)
{
    return get_item_visibility(index) == ItemVisibility::FullyVisible;
}

int ListView::get_last_unobscured_item()
{
    if (!m_items.size())
        return 0;

    const auto item_area_height = get_item_area_height();
    const auto vertical_hit_test_result = underlying_items_vertical_hit_test(m_scroll_position + item_area_height);

    const int index = vertical_hit_test_result.item_leftmost;
    const auto item_visibility = get_item_visibility(index);

    if (item_visibility == ItemVisibility::BelowViewport || item_visibility == ItemVisibility::ObscuredBelow)
        return (std::max)(0, index - 1);

    return index;
}

int ListView::get_first_or_previous_visible_item(std::optional<int> scroll_position)
{
    const auto resolved_scroll_position = scroll_position.value_or(m_scroll_position);
    return get_item_at_or_before(resolved_scroll_position + get_stuck_group_headers_height(resolved_scroll_position));
}

int ListView::get_first_unobscured_item()
{
    const auto item_count = gsl::narrow<int>(m_items.size());
    if (item_count == 0)
        return 0;

    const auto vertical_hit_test_result
        = underlying_items_vertical_hit_test(m_scroll_position + get_stuck_group_headers_height());

    const int index = vertical_hit_test_result.item_rightmost;
    const auto item_visibility = get_item_visibility(index);

    if (item_visibility == ItemVisibility::AboveViewport || item_visibility == ItemVisibility::ObscuredAbove)
        return (std::min)(index + 1, item_count);

    return index;
}

ListView::VerticalHitTestResult ListView::underlying_items_vertical_hit_test(int y) const
{
    const auto item_count = gsl::narrow<int>(m_items.size());

    if (item_count == 0)
        return {VerticalPositionCategory::NoItems};

    auto max = item_count;
    auto min = 0;

    while (min <= max) {
        const auto middle = (min + max) / 2;
        const auto item_group_headers_top = get_item_position(middle, true);
        const auto item_top = get_item_position(middle);
        const auto item_bottom = get_item_position_bottom(middle);
        if (y >= item_bottom)
            min = middle + 1;
        else if (y < item_group_headers_top)
            max = middle - 1;
        else if (y < item_top) {
            assert(m_group_count > 0);

            if (y >= item_top - get_leaf_group_header_bottom_margin())
                return {VerticalPositionCategory::BetweenGroupHeaderAndItem, middle, middle};

            const auto group_headers_bottom = item_top - get_leaf_group_header_bottom_margin() - 1;
            const auto display_group_reverse_index = (group_headers_bottom - y) / m_group_height;
            const auto group_index = display_group_reverse_index_to_group_index(middle, display_group_reverse_index);

            return {
                VerticalPositionCategory::OnGroupHeader,
                middle,
                middle,
                group_index,
            };
        } else
            return {VerticalPositionCategory::OnItem, middle, middle};
    }

    const int index_before = std::clamp(max, 0, item_count - 1);
    const int index_after = std::clamp(min, 0, item_count);

    return {VerticalPositionCategory::BetweenItems, index_before, index_after};
}

ListView::VerticalHitTestResult ListView::visible_items_vertical_hit_test(int y) const
{
    const auto result = underlying_items_vertical_hit_test(y);

    if (!m_are_group_headers_sticky || m_group_count == 0
        || result.position_category == VerticalPositionCategory::NoItems)
        return result;

    const auto first_item = underlying_items_vertical_hit_test(m_scroll_position).item_leftmost;
    const auto items_viewport_y = y - m_scroll_position;
    GroupHeaderRenderInfo render_info;

    const auto check_render_info =
        [this, &items_viewport_y](const GroupHeaderRenderInfo& render_info) -> std::optional<VerticalPositionCategory> {
        if (render_info.is_hidden)
            return {};

        const auto header_bottom = render_info.items_viewport_y + m_group_height;

        if (items_viewport_y >= render_info.items_viewport_y && items_viewport_y < header_bottom)
            return VerticalPositionCategory::OnGroupHeader;

        if (render_info.is_display_leaf && items_viewport_y >= header_bottom
            && items_viewport_y < header_bottom + get_stuck_leaf_group_header_bottom_margin())
            return VerticalPositionCategory::BetweenGroupHeaderAndItem;

        return {};
    };

    for (auto group_index : std::views::iota(size_t{}, m_group_count)) {
        render_info = get_group_header_render_info(first_item, group_index);

        if (!render_info.is_stuck)
            return result;

        if (const auto category = check_render_info(render_info))
            return {*category, result.item_leftmost, result.item_rightmost, group_index, true};
    }

    if (m_group_count == 1)
        return result;

    const auto next_child_group_item_index = render_info.group_start + render_info.group_count;

    if (next_child_group_item_index >= m_items.size())
        return result;

    const auto next_child_group_item_index_as_int = gsl::narrow<int>(next_child_group_item_index);

    for (auto group_index : std::views::iota(size_t{1}, m_group_count)) {
        const auto next_render_info = get_group_header_render_info(first_item, group_index);

        if (!next_render_info.is_stuck)
            return result;

        if (const auto category = check_render_info(next_render_info))
            return {
                *category, next_child_group_item_index_as_int, next_child_group_item_index_as_int, group_index, true};
    }

    return result;
}

} // namespace uih
