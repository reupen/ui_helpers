#include "../stdafx.h"

namespace uih {

void ListView::hit_test_ex(POINT pt_client, ListView::t_hit_test_result& result)
{
    result.column = pfc_infinite;
    t_ssize x_left_items = -m_horizontal_scroll_position + get_total_indentation();
    t_size k, kcount = m_columns.get_count();
    t_ssize colcu = x_left_items;
    for (k = 0; k < kcount; k++) {
        int end = colcu + m_columns[k].m_display_size;
        if (pt_client.x >= colcu && pt_client.x < end)
            result.column = k;
        colcu = end;
    }
    RECT rc;
    get_items_rect(&rc);
    if (pt_client.y < rc.top) {
        result.index = get_next_item(m_scroll_position);
        result.insertion_index = result.index;
        // result.index_partially_obscured = get_previous_item(m_scroll_position);
        result.result = hit_test_above;
        return;
    }
    if (pt_client.y > rc.bottom) {
        result.index = get_last_viewable_item();
        result.insertion_index = result.index;
        // result.index_partially_obscured = get_last_item(m_scroll_position);
        result.result = hit_test_below;
        return;
    }
    t_size header_height = rc.top; // get_header_height();
    t_size i, count = m_items.get_count();
    for (i = get_previous_item(pt_client.y - header_height + m_scroll_position); i < count; i++) {
        int start = get_item_position(i)
            + header_height; // m_entries[i].m_position + (m_entries[i].m_new_group ? m_entries[i].m_height : 0);
        int end = start + get_item_height(i); // m_items[i].m_height;
        if (start <= pt_client.y + m_scroll_position && pt_client.y + m_scroll_position < end) {
            result.index = i;
            result.insertion_index = result.index;
            result.result = hit_test_on;
            if (pt_client.x < x_left_items)
                result.result = hit_test_left_of_item;
            else if (pt_client.x >= colcu)
                result.result = hit_test_right_of_item;
            else if (start - m_scroll_position < rc.top)
                result.result = hit_test_obscured_above;
            else if (end - m_scroll_position > rc.bottom)
                result.result = hit_test_obscured_below;

            if (result.result == hit_test_on) {
                if (get_item_height(i) >= 2 && (pt_client.y + m_scroll_position) >= (start + get_item_height(i) / 2)) {
                    result.insertion_index++;
                }
            }
            return;
        }
        if (pt_client.y + m_scroll_position < start) {
            if (pt_client.y + m_scroll_position >= 0 && i == 0) {
                result.result = hit_test_on_group;
                result.index = i; // get_next_item(pt_client.y - header_height + m_scroll_position);
                result.insertion_index = result.index;
                result.group_level = m_group_count
                    ? (m_group_count
                          - (get_item_position(result.index) - (pt_client.y - header_height + m_scroll_position) - 1)
                              / m_group_height
                          - 1)
                    : 0;
                if (pt_client.x < 0)
                    result.result = hit_test_left_of_group;
                else if (pt_client.x >= colcu)
                    result.result = hit_test_right_of_group;
                return;
            }
        }
        if (pt_client.y + m_scroll_position >= end) {
            if (i + 1 < count
                && t_size(pt_client.y + m_scroll_position)
                    >= get_item_position(i + 1) - get_item_display_group_count(i + 1) * m_group_height + header_height
                && t_size(pt_client.y + m_scroll_position) < get_item_position(i + 1) + header_height)
            // if (i+1 < count && t_size(pt_client.y + m_scroll_position) > get_item_group_bottom(i) + header_height &&
            // t_size(pt_client.y + m_scroll_position) < get_item_position(i+1) + header_height)
            {
                result.result = hit_test_on_group;
                result.index = i + 1;
                result.insertion_index = result.index;
                result.group_level = m_group_count
                    ? (m_group_count
                          - (get_item_position(result.index) - (pt_client.y - header_height + m_scroll_position) - 1)
                              / m_group_height
                          - 1)
                    : 0;
                if (pt_client.x < 0)
                    result.result = hit_test_left_of_group;
                else if (pt_client.x >= colcu)
                    result.result = hit_test_right_of_group;
            } else // if (i+1 == count)
            {
                result.index = i;
                result.insertion_index = i + 1;
                result.result = hit_test_below_items;
            }
            // else
            //    result.result = hit_test_nowhere;
            return;
        }
    }
    result.result = hit_test_nowhere;
}

ListView::ItemVisibility ListView::get_item_visibility(t_size index)
{
    const auto item_area_height = get_item_area_height();
    const auto item_start_position = get_item_position(index);
    const auto item_end_position = get_item_position_bottom(index);

    if (item_end_position < m_scroll_position)
        return ItemVisibility::AboveViewport;

    if (item_start_position >= m_scroll_position + item_area_height)
        return ItemVisibility::BelowViewport;

    // The case where both the top and bottom of the item is obscured is ignored as it has little practical use
    if (item_start_position < m_scroll_position) {
        return ItemVisibility::ObscuredAbove;
    }

    if (item_end_position >= m_scroll_position + item_area_height) {
        return ItemVisibility::ObscuredBelow;
    }

    return ItemVisibility::FullyVisible;
}

bool ListView::is_partially_visible(t_size index)
{
    const auto item_visibility = get_item_visibility(index);

    return item_visibility == ItemVisibility::FullyVisible || item_visibility == ItemVisibility::ObscuredAbove
        || item_visibility == ItemVisibility::ObscuredBelow;
}

bool ListView::is_fully_visible(t_size index)
{
    return get_item_visibility(index) == ItemVisibility::FullyVisible;
}

t_size ListView::get_last_viewable_item()
{
    const auto item_area_height = get_item_area_height();
    const auto last_item = get_last_item();
    if (get_item_position_bottom(last_item) > m_scroll_position + item_area_height)
        return last_item - 1;
    return last_item;
}
t_size ListView::get_last_item()
{
    const auto item_area_height = get_item_area_height();
    return get_previous_item(m_scroll_position + item_area_height);
}

t_size ListView::get_previous_item(int y, bool b_include_headers) const
{
    const auto next_item = get_next_item(y, b_include_headers, true);
    return next_item > 0 ? next_item - 1 : next_item;
}

t_size ListView::get_next_item(int y, bool b_include_headers, bool include_after_end) const
{
    {
        t_size max = m_items.get_count();
        t_size min = 0;
        t_size ptr;
        // if (max && y < m_items[0]->m_position)
        //    return 0;
        while (min < max) {
            ptr = min + ((max - min) >> 1);
            if (y > get_item_position(ptr, b_include_headers))
                min = ptr + 1;
            else if (y < get_item_position(ptr, b_include_headers))
                max = ptr;
            else {
                return ptr;
                // return true;
            }
        }
        if (!include_after_end && min > 0 && min == m_items.get_count())
            min--;
        return min;
        // return true;
    }
}
} // namespace uih
