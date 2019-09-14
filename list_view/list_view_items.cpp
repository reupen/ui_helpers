#include "../stdafx.h"

#define GROUP_STRING_COMPARE strcmp

namespace uih {

const char* ListView::get_item_text(t_size index, t_size column)
{
    if (index >= m_items.size())
        return "";
    if (m_items[index]->m_subitems.size() != get_column_count())
        update_item_data(index);
    if (column >= m_items[index]->m_subitems.size())
        return "";
    return m_items[index]->m_subitems[column];
}

void ListView::insert_items(t_size index_start, t_size count, const InsertItem* items)
{
    __insert_items_v3(index_start, count, items);
    __calculate_item_positions(index_start);
    // profiler(pvt_render);
    update_scroll_info();
    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

void ListView::replace_items(t_size index_start, t_size count, const InsertItem* items)
{
    __replace_items_v2(index_start, count, items);
    __calculate_item_positions(index_start);
    update_scroll_info();
    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

void ListView::remove_items(const pfc::bit_array& p_mask)
{
    if (m_timer_inline_edit)
        exit_inline_edit();
    t_size i = m_items.size();
    for (; i; i--) {
        if (p_mask[i - 1])
            __remove_item(i - 1);
    }
    __calculate_item_positions();
    update_scroll_info();
    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

void ListView::remove_all_items()
{
    if (m_timer_inline_edit)
        exit_inline_edit();

    m_items.clear();
    update_scroll_info();

    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

void ListView::__replace_items_v2(t_size index_start, t_size countl, const InsertItem* items)
{
    std::vector<t_item_ptr> items_prev(m_items);
    t_size l, countitems = m_items.size();
    t_size newgroupcount = 0, oldgroupcount = 0;

    // Calculate old group count
    {
        t_size countl2 = index_start + countl < countitems ? countl + 1 : countl;
        for (l = 0; l < countl2; l++) {
            t_size i, count = m_group_count;
            t_size index = l + index_start;
            for (i = 0; i < count; i++) {
                if ((!index || m_items[index - 1]->m_groups[i] != m_items[index]->m_groups[i]))
                    oldgroupcount++;
            }
            // oldgroupcount += get_item_display_group_count(index);
        }
    }

    {
        for (l = 0; l < countl; l++) {
            t_size i, count = m_group_count;
            t_size index = l + index_start;
            t_item_ptr item, item_old;
            item_old = m_items[index];
            {
                item = storage_create_item();
                item->m_selected = m_items[index]->m_selected;
                item->m_subitems = items[l].m_subitems;
                m_items[index] = item;
                item->m_display_index = index ? m_items[index - 1]->m_display_index + 1 : 0;
                item->m_groups.resize(count);
            }
            bool b_new = false;

            bool b_left_same_above = true;
            bool b_right_same_above = true;
            bool b_self_same_above = true;
            for (i = 0; i < count; i++) {
                bool b_left_same = false;
                bool b_right_same = false;
                bool b_self_same = false;
                if (!b_new && index) {
                    b_left_same = b_left_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index - 1]->m_groups[i]->m_text);
                }
                if (!b_new && index + 1 < countitems && l + 1 >= countl) {
                    b_right_same = b_right_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index + 1]->m_groups[i]->m_text);
                }
                if (!b_new && item_old.is_valid()) {
                    b_self_same = b_self_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], item_old->m_groups[i]->m_text);
                }
                if (b_new || (!b_left_same && !b_right_same && !b_self_same)) {
                    item->m_groups[i] = storage_create_group();
                    item->m_groups[i]->m_text = items[l].m_groups[i];
                    b_new = true;
                    item->m_display_index++;
                }
                if (b_left_same && b_right_same) {
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                    t_group_ptr test;
                    {
                        test = m_items[index + 1]->m_groups[i];
                        t_size j = index + 1;
                        while (j < countitems && test == m_items[j]->m_groups[i]) {
                            m_items[j]->m_groups[i] = item->m_groups[i];
                            j++;
                        }
                    }
                } else if (b_left_same)
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                else if (b_right_same) {
                    item->m_display_index++;
                    item->m_groups[i] = m_items[index + 1]->m_groups[i];
                } else if (b_self_same) {
                    item->m_groups[i] = item_old->m_groups[i];
                    item->m_display_index++;
                }
                b_right_same_above = b_right_same;
                b_left_same_above = b_left_same;
                b_self_same_above = b_self_same;
            }
            if (l + 1 == countl && index + 1 < countitems) {
                for (i = 0; i < count; i++) {
                    {
                        if (items_prev[index]->m_groups[i] == items_prev[index + 1]->m_groups[i]) {
                            if (m_items[index + 1]->m_groups[i] != m_items[index]->m_groups[i]) {
                                t_group_ptr newgroup = storage_create_group();
                                newgroup->m_text = (items_prev[index]->m_groups[i]->m_text);
                                t_size j = index + 1;
                                while (j < countitems && items_prev[index]->m_groups[i] == items_prev[j]->m_groups[i]) {
                                    m_items[j]->m_groups[i] = newgroup;
                                    j++;
                                };
                            }
                        }
                    }
                }
            }
        }
    }
    {
        t_size countl2 = index_start + countl < countitems ? countl + 1 : countl;
        for (l = 0; l < countl2; l++) {
            t_size i, count = m_group_count;
            t_size index = l + index_start;
            for (i = 0; i < count; i++) {
                if ((!index || m_items[index - 1]->m_groups[i] != m_items[index]->m_groups[i]))
                    newgroupcount++;
            }
        }
    }
    {
        t_size j = index_start + countl;

        // console::formatter() << newgroupcount << " " << oldgroupcount;

        while (j < countitems) {
            m_items[j]->m_display_index += ((newgroupcount - oldgroupcount));
            j++;
        }
    }
}

void ListView::__insert_items_v3(t_size index_start, t_size pcountitems, const InsertItem* items)
{
    t_size countl = pcountitems;
    m_items.insert(m_items.begin() + index_start, countl, t_item_ptr());

    if (m_highlight_selected_item_index != pfc_infinite && m_highlight_selected_item_index >= index_start)
        m_highlight_selected_item_index += countl;

    std::vector<t_item_ptr> items_prev(m_items);
    t_size countitems = m_items.size();
    t_size newgroupcount = 0, oldgroupcount = 0;

    // Calculate old group count
    {
        t_size index = index_start + countl;
        if (index < countitems) {
            t_size i, count = m_group_count;
            for (i = 0; i < count; i++) {
                if ((!index_start || m_items[index_start - 1]->m_groups[i] != m_items[index]->m_groups[i]))
                    oldgroupcount++;
            }
        }
    }

    // Determine grouping

    {
        t_item_ptr* p_items = m_items.data();

        concurrency::parallel_for(size_t{0}, countl, [this, p_items, index_start, items](size_t l) {
            t_size count = m_group_count;
            t_size index = l + index_start;
            Item* item;
            item = storage_create_item();
            p_items[index] = item;
            item->m_subitems = items[l].m_subitems;
            item->m_groups.resize(count);
        });

        for (size_t l = 0; l < countl; l++) {
            t_size i, count = m_group_count;
            t_size index = l + index_start;
            Item* item = p_items[index].get_ptr();

            item->m_display_index = index ? p_items[index - 1]->m_display_index + 1 : 0;
            if (m_variable_height_items) {
                if (item->m_subitems.size() != get_column_count())
                    update_item_data(index);
                item->update_line_count();
            }

            bool b_new = false;
            bool b_left_same_above = true;
            bool b_right_same_above = true;
            for (i = 0; i < count; i++) {
                bool b_left_same = false;
                bool b_right_same = false;
                if (!b_new && index) {
                    b_left_same = b_left_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index - 1]->m_groups[i]->m_text);
                }
                if (!b_new && index + 1 < countitems && l + 1 >= countl) {
                    b_right_same = b_right_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index + 1]->m_groups[i]->m_text);
                }
                if (b_new || (!b_left_same && !b_right_same)) {
                    item->m_groups[i] = storage_create_group();
                    item->m_groups[i]->m_text = (items[l].m_groups[i]);
                    b_new = true;
                    item->m_display_index++;
                }
                if (b_left_same && b_right_same) {
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                    t_group_ptr test;
                    {
                        test = m_items[index + 1]->m_groups[i];
                        t_size j = index + 1;
                        while (j < countitems && test == m_items[j]->m_groups[i]) {
                            m_items[j]->m_groups[i] = item->m_groups[i];
                            j++;
                        }
                    }
                } else if (b_left_same)
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                else if (b_right_same) {
                    item->m_display_index++;
                    item->m_groups[i] = m_items[index + 1]->m_groups[i];
                }
                b_right_same_above = b_right_same;
                b_left_same_above = b_left_same;
            }
        }
    }
    {
        t_size index = index_start + countl;
        if (index_start && index < countitems) {
            t_size index_prev = index_start - 1;
            t_size i, count = m_group_count;
            for (i = 0; i < count; i++) {
                {
                    if (items_prev[index_prev]->m_groups[i] == items_prev[index]->m_groups[i]) {
                        if (m_items[index]->m_groups[i] != m_items[index - 1]->m_groups[i]) {
                            t_group_ptr newgroup = storage_create_group();
                            newgroup->m_text = (items_prev[index_prev]->m_groups[i]->m_text);
                            t_size j = index;
                            while (
                                j < countitems && items_prev[index_prev]->m_groups[i] == items_prev[j]->m_groups[i]) {
                                m_items[j]->m_groups[i] = newgroup;
                                j++;
                            };
                        }
                    }
                }
            }
        }
    }

    // Determine new group count
    {
        t_size countl2 = index_start + countl < countitems ? countl + 1 : countl;
        for (size_t l = 0; l < countl2; l++) {
            t_size i, count = m_group_count;
            t_size index = l + index_start;
            for (i = 0; i < count; i++) {
                if ((!index || m_items[index - 1]->m_groups[i] != m_items[index]->m_groups[i]))
                    newgroupcount++;
            }
        }
    }

    // Correct subsequent display indices
    {
        const t_size j_start = index_start + countl;
        t_size j = j_start;

        // console::formatter() << newgroupcount << " " << oldgroupcount;

        while (j < countitems) {
            m_items[j]->m_display_index += ((countl + newgroupcount - oldgroupcount));
            j++;
        }
    }
}

void ListView::__calculate_item_positions(t_size index_start)
{
    if (index_start >= get_item_count())
        return;

    t_size y_pointer = 0;
    if (m_group_count)
        while (index_start && get_item_display_group_count(index_start) == 0)
            index_start--;
    if (!index_start)
        y_pointer += 0; // m_item_height * m_group_count;
    else {
        if (m_group_count)
            y_pointer = get_item_group_bottom(index_start - 1) + 1;
        else
            y_pointer = get_item_position(index_start - 1) + get_item_height(index_start - 1);
    }
    t_size i, count = m_items.size(), group_height_counter = 0,
              group_minimum_inner_height = get_group_minimum_inner_height();
    for (i = index_start; i < count; i++) {
        t_size groups = get_item_display_group_count(i);
        if (groups) {
            if (group_height_counter && group_height_counter < group_minimum_inner_height)
                y_pointer += group_minimum_inner_height - group_height_counter;
            group_height_counter = 0;
        }
        group_height_counter += get_item_height(i);
        y_pointer += groups * m_group_height;
        m_items[i]->m_display_position = y_pointer;
        y_pointer += get_item_height(i);
    }
}

void ListView::remove_item(t_size index)
{
    if (m_timer_inline_edit)
        exit_inline_edit();
    __remove_item(index);
    __calculate_item_positions();
    update_scroll_info();
    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}
void ListView::__remove_item(t_size index)
{
    t_size gc = 0, k, count2 = m_items[index]->m_groups.size();

    // if (index)
    {
        for (k = 0; k < count2; k++) {
            if ((!index || m_items[index]->m_groups[k] != m_items[index - 1]->m_groups[k])
                && (index + 1 >= m_items.size() || m_items[index]->m_groups[k] != m_items[index + 1]->m_groups[k])) {
                gc++;
            }
            // else break;
        }
    }
    // else gc = count2;

    // t_item_ptr item = m_items[index];
    m_items.erase(m_items.begin() + index);

    if (index < m_items.size()) {
        int item_height = m_item_height;
        t_size j;
        if (index) {
            t_size i, count = m_items[index]->m_groups.size();
            for (i = 0; i < count; i++) {
                if (!GROUP_STRING_COMPARE(
                        m_items[index - 1]->m_groups[i]->m_text, m_items[index]->m_groups[i]->m_text)) {
                    t_group_ptr newgroup = m_items[index - 1]->m_groups[i];
                    j = index;
                    while (j < m_items.size()
                        && (!i || m_items[index - 1]->m_groups[i - 1] == m_items[j]->m_groups[i - 1])
                        && !GROUP_STRING_COMPARE(
                            m_items[index - 1]->m_groups[i]->m_text, m_items[j]->m_groups[i]->m_text)) {
                        if (j == index && m_items[j]->m_groups[i] != newgroup)
                            gc++;
                        m_items[j]->m_groups[i] = newgroup;
                        j++;
                    };
                } else
                    break;
            }
        }
        j = index;
        while (j < m_items.size()) {
            m_items[j]->m_display_index -= (1 + gc);
            j++;
        }
    }

    if (m_highlight_selected_item_index != pfc_infinite) {
        if (m_highlight_selected_item_index > index)
            m_highlight_selected_item_index--;
        else if (m_highlight_selected_item_index == index)
            m_highlight_selected_item_index = pfc_infinite;
    }
}

} // namespace uih
