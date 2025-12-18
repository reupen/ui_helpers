#include "stdafx.h"

#include "list_view.h"

#define GROUP_STRING_COMPARE strcmp

namespace uih {

const char* ListView::get_item_text(size_t index, size_t column)
{
    if (index >= m_items.size())
        return "";
    if (m_items[index]->m_subitems.size() != get_column_count())
        update_item_data(index);
    if (column >= m_items[index]->m_subitems.size())
        return "";
    return m_items[index]->m_subitems[column];
}

bool ListView::get_is_new_group(size_t index) const
{
    if (m_group_count == 0)
        return false;

    if (index == 0)
        return true;

    return m_items[index - 1]->m_groups.back() != m_items[index]->m_groups.back();
}

size_t ListView::get_item_display_group_count(size_t index) const
{
    if (index == 0) {
        return ranges::count_if(m_items[index]->m_groups, [](auto& group) { return !group->is_hidden(); });
    }

    auto zipped_groups = ranges::views::zip(m_items[index - 1]->m_groups, m_items[index]->m_groups);
    return ranges::count_if(zipped_groups, [](const auto& pair) {
        const auto& [previous_item_group, this_item_group] = pair;

        return previous_item_group != this_item_group && !this_item_group->is_hidden();
    });
}

bool ListView::is_group_visible(size_t item_index, size_t group_index) const
{
    const auto& group = m_items[item_index]->m_groups[group_index];

    if (item_index == 0)
        return !group->is_hidden();

    return group != m_items[item_index - 1]->m_groups[group_index] && !group->is_hidden();
}

ListView::ItemTransaction::~ItemTransaction() noexcept
{
    if (!m_start_index)
        return;

    m_list_view.calculate_item_positions(*m_start_index);
    m_list_view.update_scroll_info(true, true, false);
    m_list_view.invalidate_all(false, true);
}

void ListView::ItemTransaction::insert_items(size_t index_start, size_t count, const InsertItem* items)
{
    m_start_index = std::min(index_start, m_start_index.value_or(index_start));
    m_list_view.insert_items_in_internal_state(index_start, count, items);
}

void ListView::ItemTransaction::remove_items(const pfc::bit_array& mask)
{
    m_list_view.remove_items_in_internal_state(mask);
    m_start_index = 0;
}

ListView::ItemTransaction ListView::start_transaction()
{
    return {*this};
}

void ListView::insert_items(size_t index_start, size_t count, const InsertItem* items,
    const std::optional<lv::SavedScrollPosition>& saved_scroll_position)
{
    insert_items_in_internal_state(index_start, count, items);
    calculate_item_positions(index_start);

    if (saved_scroll_position)
        restore_scroll_position(*saved_scroll_position);
    else
        update_scroll_info();

    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

bool ListView::replace_items(size_t index_start, size_t count, const InsertItem* items)
{
    assert(count > 0);

    if (count == 0)
        return false;

    const auto subsequent_display_indices_changed = replace_items_in_internal_state(index_start, count, items);
    calculate_item_positions(index_start);

    if (m_group_count > 0 || m_variable_height_items) {
        update_scroll_info();
        invalidate_all();
    } else {
        invalidate_items(index_start, count);
    }

    return subsequent_display_indices_changed;
}

void ListView::remove_items(const pfc::bit_array& mask)
{
    remove_items_in_internal_state(mask);
    calculate_item_positions();
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

bool ListView::replace_items_in_internal_state(size_t index_start, size_t replace_count, const InsertItem* items)
{
    std::vector items_prev(m_items);

    const size_t total_items = m_items.size();
    size_t old_group_display_count{};

    for (const auto item_index :
        std::views::iota(index_start, std::min(index_start + replace_count + 1, total_items))) {
        old_group_display_count += get_item_display_group_count(item_index);
    }

    for (const auto relative_index : std::views::iota(size_t{}, replace_count)) {
        const auto absolute_index = relative_index + index_start;
        t_item_ptr item;
        t_item_ptr item_old;
        item_old = m_items[absolute_index];
        {
            item = storage_create_item();
            item->m_selected = m_items[absolute_index]->m_selected;
            item->m_subitems = items[relative_index].m_subitems;
            m_items[absolute_index] = item;
            item->m_display_index = absolute_index ? m_items[absolute_index - 1]->m_display_index + 1 : 0;
            item->m_groups.resize(m_group_count);
        }
        bool b_new = false;

        bool b_left_same_above = true;
        bool b_right_same_above = true;
        bool b_self_same_above = true;
        for (size_t i = 0; i < m_group_count; i++) {
            bool b_left_same = false;
            bool b_right_same = false;
            bool b_self_same = false;

            if (!b_new && absolute_index) {
                b_left_same = b_left_same_above
                    && !GROUP_STRING_COMPARE(
                        items[relative_index].m_groups[i], m_items[absolute_index - 1]->m_groups[i]->m_text);
            }

            if (!b_new && absolute_index + 1 < total_items && relative_index + 1 >= replace_count) {
                b_right_same = b_right_same_above
                    && !GROUP_STRING_COMPARE(
                        items[relative_index].m_groups[i], m_items[absolute_index + 1]->m_groups[i]->m_text);
            }

            if (!b_new && item_old.is_valid()) {
                b_self_same = b_self_same_above
                    && !GROUP_STRING_COMPARE(items[relative_index].m_groups[i], item_old->m_groups[i]->m_text);
            }

            if (b_new || (!b_left_same && !b_right_same && !b_self_same)) {
                item->m_groups[i] = storage_create_group();
                item->m_groups[i]->m_text = items[relative_index].m_groups[i];
                b_new = true;

                if (!item->m_groups[i]->is_hidden())
                    ++item->m_display_index;
            }

            if (b_left_same && b_right_same) {
                item->m_groups[i] = m_items[absolute_index - 1]->m_groups[i];
                t_group_ptr test;
                {
                    test = m_items[absolute_index + 1]->m_groups[i];
                    size_t j = absolute_index + 1;
                    while (j < total_items && test == m_items[j]->m_groups[i]) {
                        m_items[j]->m_groups[i] = item->m_groups[i];
                        j++;
                    }
                }
            } else if (b_left_same)
                item->m_groups[i] = m_items[absolute_index - 1]->m_groups[i];
            else if (b_right_same) {
                item->m_groups[i] = m_items[absolute_index + 1]->m_groups[i];

                if (!item->m_groups[i]->is_hidden())
                    item->m_display_index++;
            } else if (b_self_same) {
                item->m_groups[i] = item_old->m_groups[i];

                if (!item->m_groups[i]->is_hidden())
                    item->m_display_index++;
            }
            b_right_same_above = b_right_same;
            b_left_same_above = b_left_same;
            b_self_same_above = b_self_same;
        }
        if (relative_index + 1 == replace_count && absolute_index + 1 < total_items) {
            for (const auto group_index : std::views::iota(size_t{}, m_group_count)) {
                const auto old_item = items_prev[absolute_index];
                const auto old_next_item = items_prev[absolute_index + 1];
                const auto next_item = m_items[absolute_index + 1];

                if (old_item->m_groups[group_index] == old_next_item->m_groups[group_index]
                    && item->m_groups[group_index] != next_item->m_groups[group_index]) {
                    t_group_ptr new_group = storage_create_group();
                    new_group->m_text = (items_prev[absolute_index]->m_groups[group_index]->m_text);
                    size_t item_index = absolute_index + 1;

                    while (item_index < total_items
                        && items_prev[absolute_index]->m_groups[group_index]
                            == items_prev[item_index]->m_groups[group_index]) {
                        m_items[item_index]->m_groups[group_index] = new_group;
                        item_index++;
                    }
                }
            }
        }
    }

    size_t new_group_display_count{};

    for (const auto item_index :
        std::views::iota(index_start, std::min(index_start + replace_count + 1, total_items))) {
        new_group_display_count += get_item_display_group_count(item_index);
    }

    size_t item_index = index_start + replace_count;

    while (item_index < total_items) {
        m_items[item_index]->m_display_index += new_group_display_count - old_group_display_count;
        item_index++;
    }

    return new_group_display_count != old_group_display_count && index_start + replace_count < total_items;
}

void ListView::insert_items_in_internal_state(size_t index_start, size_t insert_count, const InsertItem* items)
{
    const auto total_items = m_items.size();
    const auto old_group_display_count = index_start < total_items ? get_item_display_group_count(index_start) : 0;

    m_items.insert(m_items.begin() + index_start, insert_count, t_item_ptr());

    if (m_highlight_selected_item_index != pfc_infinite && m_highlight_selected_item_index >= index_start)
        m_highlight_selected_item_index += insert_count;

    const std::optional<std::vector<t_item_ptr>> items_prev
        = m_group_count > 0 ? std::make_optional(m_items) : std::nullopt;

    // Determine grouping
    {
        t_item_ptr* p_items = m_items.data();

        concurrency::parallel_for(size_t{0}, insert_count, [this, p_items, index_start, items](size_t l) {
            size_t count = m_group_count;
            size_t index = l + index_start;
            Item* item;
            item = storage_create_item();
            p_items[index] = item;
            item->m_subitems = items[l].m_subitems;
            item->m_groups.resize(count);
        });

        for (size_t l = 0; l < insert_count; l++) {
            size_t count = m_group_count;
            size_t index = l + index_start;
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
            for (size_t i = 0; i < count; i++) {
                bool b_left_same = false;
                bool b_right_same = false;
                if (!b_new && index) {
                    b_left_same = b_left_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index - 1]->m_groups[i]->m_text);
                }
                if (!b_new && index + 1 < total_items && l + 1 >= insert_count) {
                    b_right_same = b_right_same_above
                        && !GROUP_STRING_COMPARE(items[l].m_groups[i], m_items[index + 1]->m_groups[i]->m_text);
                }
                if (b_new || (!b_left_same && !b_right_same)) {
                    t_group_ptr group = storage_create_group();
                    group->m_text = items[l].m_groups[i];

                    if (!group->is_hidden())
                        item->m_display_index++;

                    item->m_groups[i] = std::move(group);
                    b_new = true;
                }

                if (b_left_same && b_right_same) {
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                    t_group_ptr test;
                    {
                        test = m_items[index + 1]->m_groups[i];
                        size_t j = index + 1;
                        while (j < total_items && test == m_items[j]->m_groups[i]) {
                            m_items[j]->m_groups[i] = item->m_groups[i];
                            j++;
                        }
                    }
                } else if (b_left_same)
                    item->m_groups[i] = m_items[index - 1]->m_groups[i];
                else if (b_right_same) {
                    item->m_groups[i] = m_items[index + 1]->m_groups[i];

                    if (!item->m_groups[i]->is_hidden())
                        item->m_display_index++;
                }
                b_right_same_above = b_right_same;
                b_left_same_above = b_left_same;
            }
        }
    }
    {
        size_t index = index_start + insert_count;
        if (m_group_count > 0 && index_start && index < total_items) {
            const auto& items_prev_value = *items_prev;

            const size_t index_prev = index_start - 1;
            const size_t count = m_group_count;
            for (size_t i = 0; i < count; i++) {
                if (items_prev_value[index_prev]->m_groups[i] == items_prev_value[index]->m_groups[i]) {
                    if (m_items[index]->m_groups[i] != m_items[index - 1]->m_groups[i]) {
                        t_group_ptr newgroup = storage_create_group();
                        newgroup->m_text = (items_prev_value[index_prev]->m_groups[i]->m_text);
                        size_t j = index;
                        while (j < total_items
                            && items_prev_value[index_prev]->m_groups[i] == items_prev_value[j]->m_groups[i]) {
                            m_items[j]->m_groups[i] = newgroup;
                            j++;
                        }
                    }
                }
            }
        }
    }

    // Determine new group count
    size_t new_group_display_count{};

    for (const auto item_index : std::views::iota(index_start, std::min(index_start + insert_count + 1, total_items))) {
        new_group_display_count += get_item_display_group_count(item_index);
    }

    // Correct subsequent display indices
    {
        const size_t j_start = index_start + insert_count;
        size_t j = j_start;

        while (j < total_items) {
            m_items[j]->m_display_index += insert_count + new_group_display_count - old_group_display_count;
            j++;
        }
    }
}

void ListView::calculate_item_positions(size_t index_start)
{
    if (index_start >= get_item_count())
        return;

    int y_pointer = 0;
    if (m_group_count)
        while (index_start && !get_is_new_group(index_start))
            index_start--;
    if (!index_start)
        y_pointer += 0; // m_item_height * m_group_count;
    else {
        if (m_group_count)
            y_pointer = get_item_group_bottom(index_start - 1) + 1;
        else
            y_pointer = get_item_position(index_start - 1) + get_item_height(index_start - 1);
    }
    size_t count = m_items.size();
    int group_height_counter = 0;
    const auto group_minimum_inner_height = get_group_minimum_inner_height();
    for (size_t i = index_start; i < count; i++) {
        const auto is_new_group = get_is_new_group(i);
        const auto display_group_count = gsl::narrow<int>(get_item_display_group_count(i));

        if (is_new_group) {
            const auto bottom_margin = i > index_start ? get_group_items_bottom_margin(i - 1) : 0;

            if (group_height_counter > 0) {
                if (group_height_counter < group_minimum_inner_height)
                    y_pointer += std::max(bottom_margin, group_minimum_inner_height - group_height_counter);
                else
                    y_pointer += bottom_margin;
            }

            group_height_counter = 0;

            y_pointer += get_leaf_group_header_bottom_margin(i);
        }
        group_height_counter += get_item_height(i);
        y_pointer += display_group_count * m_group_height;
        m_items[i]->m_display_position = y_pointer;
        y_pointer += get_item_height(i);
    }
}

void ListView::remove_item(size_t index)
{
    if (m_timer_inline_edit)
        exit_inline_edit();
    remove_item_in_internal_state(index);
    calculate_item_positions();
    update_scroll_info();
    RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
}

void ListView::remove_items_in_internal_state(const pfc::bit_array& mask)
{
    if (m_timer_inline_edit)
        exit_inline_edit();

    for (size_t i = m_items.size(); i; i--) {
        if (mask[i - 1])
            remove_item_in_internal_state(i - 1);
    }
}

void ListView::remove_item_in_internal_state(size_t remove_index)
{
    const size_t total_items = m_items.size();
    size_t old_group_display_count{};

    for (const auto item_index : std::views::iota(remove_index, std::min(remove_index + 2, total_items))) {
        old_group_display_count += get_item_display_group_count(item_index);
    }

    m_items.erase(m_items.begin() + remove_index);

    if (remove_index < m_items.size()) {
        if (remove_index) {
            const size_t count = m_items[remove_index]->m_groups.size();
            for (size_t group_index = 0; group_index < count; group_index++) {
                if (GROUP_STRING_COMPARE(m_items[remove_index - 1]->m_groups[group_index]->m_text,
                        m_items[remove_index]->m_groups[group_index]->m_text)
                    != 0)
                    break;

                t_group_ptr new_group = m_items[remove_index - 1]->m_groups[group_index];
                size_t item_index = remove_index;
                while (item_index < m_items.size()
                    && (!group_index
                        || m_items[remove_index - 1]->m_groups[group_index - 1]
                            == m_items[item_index]->m_groups[group_index - 1])
                    && !GROUP_STRING_COMPARE(m_items[remove_index - 1]->m_groups[group_index]->m_text,
                        m_items[item_index]->m_groups[group_index]->m_text)) {
                    m_items[item_index]->m_groups[group_index] = new_group;
                    item_index++;
                }
            }
        }

        const auto new_group_display_count = get_item_display_group_count(remove_index);

        for (const auto& item : m_items | ranges::views::drop(remove_index))
            item->m_display_index += new_group_display_count - old_group_display_count - 1;
    }

    if (m_highlight_selected_item_index != pfc_infinite) {
        if (m_highlight_selected_item_index > remove_index)
            m_highlight_selected_item_index--;
        else if (m_highlight_selected_item_index == remove_index)
            m_highlight_selected_item_index = pfc_infinite;
    }
}

} // namespace uih
