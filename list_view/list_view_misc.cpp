#include "stdafx.h"

#include "list_view.h"
#include "../text_style.h"

using namespace std::chrono_literals;
using namespace uih::literals::spx;

namespace uih {

void ListView::on_first_show()
{
    if (const size_t focus = get_focus_item(); focus != std::numeric_limits<size_t>::max())
        ensure_visible(focus);
}

int ListView::get_group_items_bottom_margin(size_t index) const
{
    if (!get_show_group_info_area() || !m_is_group_info_area_header_spacing_enabled || index + 1 == m_items.size())
        return 0;

    return m_group_height / 6;
}

int ListView::get_leaf_group_header_bottom_margin(std::optional<size_t> index) const
{
    if (!get_show_group_info_area() || !m_is_group_info_area_header_spacing_enabled)
        return 0;

    if (index && !get_is_new_group(*index))
        return 0;

    if (m_group_level_indentation_enabled)
        return m_group_count > 1 ? m_group_height / 4 : m_group_height / 8;

    return m_group_count > 1 ? m_group_height / 5 : m_group_height / 8;
}

int ListView::get_stuck_leaf_group_header_bottom_margin() const
{
    return get_leaf_group_header_bottom_margin() / 3;
}

ListView::GroupInfoAreaPadding ListView::get_group_info_area_padding() const
{
    if (!get_show_group_info_area())
        return {};

    const auto min_left_padding = 2_spx + 3_spx;
    const auto min_right_padding = 4_spx;
    const auto min_bottom_padding = m_item_height / 2;
    const auto indentation_step = get_indentation_step();

    return GroupInfoAreaPadding{
        std::max(min_left_padding, m_group_count > 1 ? indentation_step : 0),
        0,
        std::max(min_right_padding, indentation_step),
        std::max(min_bottom_padding, indentation_step),
    };
}

int ListView::get_total_indentation() const
{
    if (m_group_count == 0)
        return 0;

    return m_root_group_indentation_amount
        + get_indentation_step() * gsl::narrow<int>(m_group_count - (get_show_group_info_area() ? 1 : 0))
        + get_group_info_area_total_width();
}

ListView::GroupHeaderRenderInfo ListView::get_group_header_render_info(
    size_t item_index, size_t group_index, std::optional<int> scroll_position) const
{
    assert(item_index < m_items.size());
    assert(group_index < m_group_count);

    const auto resolved_scroll_position = scroll_position.value_or(m_scroll_position);
    auto [group_start, group_item_count] = get_item_group_range(item_index, group_index);

    const auto display_group_count = get_item_display_group_count(group_start);
    const auto display_group_index = get_item_display_group_count(group_start, group_index);
    const auto is_leaf = display_group_index + 1 == display_group_count;
    const auto is_hidden = m_items[item_index]->m_groups[group_index]->is_hidden();
    const auto height = is_hidden ? 0 : m_group_height;

    const auto min_group_top = get_item_position(group_start) - resolved_scroll_position
        - m_group_height * gsl::narrow<int>(display_group_count - display_group_index)
        - get_leaf_group_header_bottom_margin();

    if (!m_are_group_headers_sticky)
        return GroupHeaderRenderInfo{group_start, group_item_count, min_group_top, height, is_leaf, is_hidden, false};

    auto [final_leaf_group_start, final_leaf_group_item_count]
        = get_item_group_range(group_start + group_item_count - 1, m_group_count - 1);
    const auto final_leaf_group_first_item_top = get_item_position(final_leaf_group_start) - resolved_scroll_position;
    const auto group_bottom = final_leaf_group_first_item_top
        + std::max(get_group_minimum_inner_height(), gsl::narrow<int>(final_leaf_group_item_count) * m_item_height);

    const auto cumulative_display_group_index = get_item_cumulative_display_group_count(group_start, group_index);

    const auto max_group_top = group_bottom
        - m_group_height * gsl::narrow<int>(display_group_count - display_group_index)
        - get_stuck_leaf_group_header_bottom_margin();

    const auto render_pos = std::max(
        min_group_top, std::min(gsl::narrow<int>(cumulative_display_group_index) * m_group_height, max_group_top));

    const auto is_stuck = render_pos != min_group_top;

    return GroupHeaderRenderInfo{group_start, group_item_count, render_pos, height, is_leaf, is_hidden, is_stuck};
}

int ListView::get_stuck_group_headers_height(std::optional<int> scroll_position) const
{
    return get_stuck_group_headers_info(scroll_position).height;
}

ListView::StuckGroupHeadersInfo ListView::get_stuck_group_headers_info(std::optional<int> scroll_position) const
{
    if (!m_are_group_headers_sticky || m_group_count == 0)
        return {};

    const auto resolved_scroll_position = scroll_position.value_or(m_scroll_position);

    if (resolved_scroll_position == 0)
        return {};

    const auto vht_result = underlying_items_vertical_hit_test(resolved_scroll_position);

    if (vht_result.position_category == VerticalPositionCategory::NoItems)
        return {};

    const auto item_index = vht_result.item_leftmost;
    const auto render_info = get_group_header_render_info(item_index, m_group_count - 1, resolved_scroll_position);

    if (!render_info.is_stuck)
        return {};

    const auto next_child_group_index = render_info.group_start + render_info.group_count;

    if (next_child_group_index < m_items.size()) {
        const auto next_render_info
            = get_group_header_render_info(next_child_group_index, m_group_count - 1, resolved_scroll_position);

        if (next_render_info.is_stuck)
            return {next_render_info.items_viewport_y + next_render_info.height
                    + get_stuck_leaf_group_header_bottom_margin(),
                next_render_info.group_start};
    }

    return {render_info.items_viewport_y + render_info.height + get_stuck_leaf_group_header_bottom_margin(),
        gsl::narrow<size_t>(render_info.group_start)};
}

void ListView::refresh_item_positions()
{
    const auto position = save_scroll_position();
    calculate_item_positions();
    restore_scroll_position(position);
}

bool ListView::copy_selected_items_as_text(size_t default_single_item_column)
{
    pfc::string8 text;
    pfc::string8 cleanedText;
    size_t selected_count = get_selection_count(2);
    if (selected_count == 0) {
        // return false;
    } else if (default_single_item_column != pfc_infinite && selected_count == 1) {
        size_t index = get_selected_item_single();
        if (index != pfc_infinite)
            text = get_item_text(index, default_single_item_column);
    } else {
        size_t column_count = get_column_count();
        size_t item_count = get_item_count();
        pfc::bit_array_bittable mask_selected(get_item_count());
        get_selection_state(mask_selected);
        bool b_first = true;
        for (size_t i = 0; i < item_count; i++)
            if (mask_selected[i]) {
                if (!b_first)
                    text << "\r\n";
                b_first = false;
                for (size_t j = 0; j < column_count; j++)
                    text << (j ? "\t" : "") << get_item_text(i, j);
            }
    }

    const auto cleaned_text = text_style::remove_colour_and_font_codes(text.c_str());
    set_clipboard_text(cleaned_text.c_str());
    return selected_count > 0;
}

void ListView::set_sort_column(std::optional<size_t> index, bool b_direction)
{
    m_sort_column_index = index;
    m_sort_direction = b_direction;

    if (!m_initialised || !m_wnd_header)
        return;

    auto header_item_index = index;
    if (m_have_indent_column && header_item_index)
        ++*header_item_index;

    HDITEM hdi{};
    hdi.mask = HDI_FORMAT;

    for (const auto iter_index : std::views::iota(0, gsl::narrow<int>(m_columns.size()))) {
        Header_GetItem(m_wnd_header, iter_index, &hdi);
        const auto old_fmt = hdi.fmt;
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

        if (m_show_sort_indicators && header_item_index && iter_index == *header_item_index)
            hdi.fmt |= (b_direction ? HDF_SORTDOWN : HDF_SORTUP);

        if (hdi.fmt != old_fmt)
            Header_SetItem(m_wnd_header, iter_index, &hdi);
    }
}

void ListView::set_autosize(bool b_val)
{
    m_autosize = b_val;
    if (m_initialised) {
        /*if (m_wnd_header)
        {
            LONG_PTR style = GetWindowLongPtr(m_wnd_header, GWL_STYLE);
            SetWindowLongPtr(m_wnd_header, GWL_STYLE, b_val ? (style|0x0800) : (style & ~0x0800) );
        }*/
        update_column_sizes();
        update_header();
        invalidate_all();
        update_scroll_info();
    }
}
void ListView::set_always_show_focus(bool b_val)
{
    m_always_show_focus = b_val;
    if (m_initialised)
        invalidate_all();
}
void ListView::on_size(bool b_update_scroll)
{
    RECT rc;
    GetClientRect(get_wnd(), &rc);
    on_size(RECT_CX(rc), RECT_CY(rc), b_update_scroll);
}

void ListView::on_size(int cxd, int cyd, bool b_update_scroll)
{
    if (m_sizing)
        return;

    pfc::vartoggle_t<bool> toggler(m_sizing, true);

    hide_tooltip();

    RECT rc_header;
    get_header_rect(&rc_header);

    RECT rc{};
    GetClientRect(get_wnd(), &rc);
    int cx = RECT_CX(rc);

    auto old_search_height = get_search_box_height();
    auto new_search_height = m_search_editbox ? m_item_height + scale_dpi_value(4) : 0;

    if (m_search_editbox) {
        SetWindowPos(m_search_editbox, nullptr, 0, 0, cx, new_search_height, SWP_NOZORDER);
    }

    auto new_header_height = calculate_header_height();
    if (new_header_height != RECT_CY(rc_header) && m_wnd_header)
        // Update height because affects scroll info
        SetWindowPos(m_wnd_header, nullptr, -m_horizontal_scroll_position, new_search_height,
            cx + m_horizontal_scroll_position, new_header_height, SWP_NOZORDER);

    if (b_update_scroll)
        update_scroll_info();

    if (m_autosize)
        update_column_sizes();
    if (m_autosize)
        update_header();

    GetClientRect(get_wnd(), &rc);
    cx = RECT_CX(rc);

    // Reposition again due to potential vertical scrollbar changes
    if (m_wnd_header)
        SetWindowPos(m_wnd_header, nullptr, -m_horizontal_scroll_position, new_search_height,
            cx + m_horizontal_scroll_position, new_header_height, SWP_NOZORDER);

    if (m_search_editbox) {
        SetWindowPos(m_search_editbox, nullptr, 0, 0, cx, new_search_height, SWP_NOZORDER);
    }

    if (new_header_height != RECT_CY(rc_header))
        RedrawWindow(m_wnd_header, nullptr, nullptr, RDW_INVALIDATE);
    if (m_autosize || new_header_height != RECT_CY(rc_header) || get_search_box_height() != old_search_height)
        invalidate_all();
}

RECT ListView::get_items_rect() const
{
    RECT rc{};

    GetClientRect(get_wnd(), &rc);
    rc.top += get_header_height();
    rc.top += get_search_box_height();

    if (rc.bottom < rc.top)
        rc.bottom = rc.top;

    return rc;
}

int ListView::get_item_area_height() const
{
    const auto rc = get_items_rect();
    return RECT_CY(rc);
}

void ListView::reset_columns()
{
    m_columns.clear();
}

void ListView::set_group_count(size_t count, bool b_update_columns)
{
    m_group_count = count;
    if (m_initialised && b_update_columns) {
        update_column_sizes();
        build_header();
        // update_header();
    }
}
void ListView::process_navigation_keydown(WPARAM wp, bool alt_down, bool repeat)
{
    auto focus = static_cast<int>(get_focus_item());
    const auto total = gsl::narrow<int>(m_items.size());

    if (!total)
        return;

    if (focus == pfc_infinite)
        focus = 0;
    else if (focus >= total)
        focus = total - 1;

    SCROLLINFO si{};
    si.cbSize = sizeof(si);

    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    GetScrollInfo(get_wnd(), SB_VERT, &si);

    const auto focused_item_is_visible = is_partially_visible(focus);

    const auto first_visible_item = get_first_unobscured_item();
    const auto last_visible_item = get_last_unobscured_item();
    const auto focus_top = get_item_position(focus);

    int target_item{};

    switch (wp) {
    case VK_HOME:
        target_item = 0;
        break;
    case VK_END:
        target_item = total - 1;
        break;
    case VK_PRIOR:
        if (focused_item_is_visible && focus > first_visible_item) {
            target_item = first_visible_item;
        } else {
            auto target_pos = focus_top - gsl::narrow<int>(si.nPage);

            if (focus_top > si.nMin)
                target_pos += get_stuck_group_headers_height(focus_top);

            target_item = get_item_at_or_after(target_pos);

            if (target_item == focus && target_item > 0)
                --target_item;
        }
        break;
    case VK_NEXT:
        if (focused_item_is_visible && focus < last_visible_item) {
            target_item = last_visible_item;
        } else {
            auto target_pos = focus_top + gsl::narrow<int>(si.nPage);
            target_pos -= get_stuck_group_headers_height(std::min(si.nMax, target_pos));
            target_item = get_item_at_or_before(target_pos);

            if (target_item == focus && target_item + 1 < total)
                ++target_item;
        }
        break;
    case VK_UP:
        target_item = (std::max)(0, focus - 1);
        break;
    case VK_DOWN:
        target_item = (std::min)(focus + 1, total - 1);
        break;
    }

    {
        auto _ = suspend_ensure_visible();
        bool is_focus_selected = get_item_selected(focus);

        if ((GetKeyState(VK_SHIFT) & KF_UP) && (GetKeyState(VK_CONTROL) & KF_UP)) {
            // if (!repeat) playlist_api->activeplaylist_undo_backup();
            move_selection(target_item - focus);
        } else if ((GetKeyState(VK_CONTROL) & KF_UP)) {
            set_focus_item(target_item);
        } else if (m_selection_mode == SelectionMode::Multiple && (GetKeyState(VK_SHIFT) & KF_UP)) {
            const size_t start = m_alternate_selection ? focus : m_shift_start;
            const pfc::bit_array_range array_select(
                std::min(start, size_t(target_item)), abs(int(start - (target_item))) + 1);
            if (m_alternate_selection && !is_focus_selected)
                set_selection_state(array_select, pfc::bit_array_not(array_select), true);
            else if (m_alternate_selection)
                set_selection_state(array_select, array_select, true);
            else
                set_selection_state(pfc::bit_array_true(), array_select, true);
            set_focus_item(target_item, true);
        } else {
            set_item_selected_single(target_item);
        }
    }

    ensure_visible(target_item, EnsureVisibleMode::PreferMinimalScrolling);
}

int ListView::get_default_item_height()
{
    const auto font_height = m_items_text_format ? m_items_text_format->get_minimum_height() : 0;
    int ret = font_height + m_vertical_item_padding;
    if (ret < 1)
        ret = 1;
    return ret;
}

int ListView::get_default_group_height()
{
    const auto font_height = m_group_text_format ? m_group_text_format->get_minimum_height() : 0;
    int ret = font_height + m_vertical_item_padding;
    if (ret < 1)
        ret = 1;
    return ret;
}

void ListView::on_focus_change(size_t index_prev, size_t index_new)
{
    size_t count = m_items.size();
    if (index_prev < count)
        invalidate_items(index_prev, 1);
    if (index_new < count)
        invalidate_items(index_new, 1);
}

void ListView::invalidate_all(bool b_children, bool non_client)
{
    auto flags = RDW_INVALIDATE | (b_children ? RDW_ALLCHILDREN : 0) | (non_client ? RDW_FRAME : 0);
    RedrawWindow(get_wnd(), nullptr, nullptr, flags);
}

void ListView::update_items(size_t index, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++)
        m_items[i + index]->m_subitems.resize(0);
    invalidate_items(index, count);
}

void ListView::reorder_items_partial(size_t base, const size_t* order, size_t count, bool update_focus_item)
{
    pfc::reorder_partial_t(m_items, base, order, count);
    pfc::list_t<InsertItem> insert_items;
    insert_items.set_size(count);
    replace_items(base, insert_items);

    if (update_focus_item) {
        auto focus_item = storage_get_focus_item();
        if (focus_item >= base && focus_item < base + count) {
            focus_item = base + order[focus_item - base];
            storage_set_focus_item(focus_item);
        }
    }
}

void ListView::update_all_items()
{
    update_items(0, m_items.size());
}

void ListView::invalidate_items(size_t index, size_t count) const
{
    if (count == 0)
        return;

    const auto items_rect = get_items_rect();
    const auto has_group_info_area = get_show_group_info_area();

    const auto items_left
        = has_group_info_area ? get_total_indentation() - m_horizontal_scroll_position : items_rect.left;

    if (items_left >= items_rect.right)
        return;

    const RECT items_invalidate_rect = {items_left, get_item_position(index) - m_scroll_position + items_rect.top,
        items_rect.right, get_item_position_bottom(index + count - 1) - m_scroll_position + items_rect.top};

    RECT visible_invalidate_rect{};
    if (IntersectRect(&visible_invalidate_rect, &items_rect, &items_invalidate_rect))
        RedrawWindow(get_wnd(), &visible_invalidate_rect, nullptr, RDW_INVALIDATE);
}

void ListView::invalidate_items(const pfc::bit_array& mask)
{
    size_t i;
    size_t start;
    size_t count = get_item_count();
    for (i = 0; i < count; i++) {
        start = i;
        while (i < count && mask[i]) {
            i++;
        }
        if (i > start) {
            invalidate_items(start, i - start);
        }
    }
}

void ListView::set_are_group_headers_sticky(bool value)
{
    if (m_are_group_headers_sticky == value)
        return;

    if (!m_initialised) {
        m_are_group_headers_sticky = value;
        return;
    }

    const auto old_stuck_headers_height = get_stuck_group_headers_height();

    m_are_group_headers_sticky = value;

    const auto new_stuck_headers_height = get_stuck_group_headers_height();

    if (old_stuck_headers_height != new_stuck_headers_height)
        invalidate_all();
}

void ListView::set_is_group_info_area_sticky(bool group_info_area_sticky)
{
    if (group_info_area_sticky == m_is_group_info_area_sticky)
        return;

    size_t first_item_index{};
    bool is_invalidation_needed{};

    if (m_initialised && get_show_group_info_area()) {
        first_item_index = gsl::narrow_cast<size_t>(get_first_or_previous_visible_item());

        if (first_item_index < m_items.size()) {
            is_invalidation_needed = true;
            invalidate_item_group_info_area(first_item_index);
        }
    }

    m_is_group_info_area_sticky = group_info_area_sticky;

    if (is_invalidation_needed) {
        invalidate_item_group_info_area(first_item_index);
    }
}

void ListView::set_is_group_info_area_header_spacing_enabled(bool value)
{
    if (value == m_is_group_info_area_header_spacing_enabled)
        return;

    m_is_group_info_area_header_spacing_enabled = value;

    if (m_initialised)
        refresh_item_positions();
}

RECT ListView::get_item_group_info_area_render_rect(
    size_t index, const std::optional<RECT>& items_rect, std::optional<int> scroll_position)
{
    if (!get_show_group_info_area()) {
        assert(false);
        return {};
    }

    const auto resolved_items_rect = items_rect ? *items_rect : get_items_rect();
    const auto resolved_scroll_position = scroll_position.value_or(m_scroll_position);

    const auto leaf_group_header_render_info
        = get_group_header_render_info(index, m_group_count - 1, resolved_scroll_position);

    const auto artwork_indentation
        = m_root_group_indentation_amount + get_indentation_step() * (gsl::narrow<int>(m_group_count) - 1);
    const auto group_info_area_padding = get_group_info_area_padding();

    const auto [group_first_item, group_item_count] = get_item_group_range(index, m_group_count - 1);

    const auto group_first_item_top = get_item_position(group_first_item) - resolved_scroll_position;
    const auto group_leaf_header_bottom = m_is_group_info_area_sticky && leaf_group_header_render_info.is_stuck
        ? leaf_group_header_render_info.items_viewport_y + leaf_group_header_render_info.height
            + get_leaf_group_header_bottom_margin()
        : group_first_item_top;
    const auto group_bottom = group_first_item_top
        + std::max(get_group_minimum_inner_height(), gsl::narrow<int>(group_item_count) * m_item_height);

    const auto left = 0 - m_horizontal_scroll_position + artwork_indentation + group_info_area_padding.left;
    const auto right = left + get_group_info_area_width();

    int top = group_leaf_header_bottom + resolved_items_rect.top + group_info_area_padding.top;

    if (top < resolved_items_rect.top && m_is_group_info_area_sticky) {
        const auto sticky_pos = static_cast<int>(resolved_items_rect.top);
        top = std::max(top, sticky_pos);
    }

    const auto items_bottom_minus_info_height = group_bottom - get_group_info_area_height()
        + static_cast<int>(resolved_items_rect.top) - group_info_area_padding.bottom;
    top = std::min(top, items_bottom_minus_info_height);

    const auto bottom = top + get_group_info_area_height();

    return {left, top, right, bottom};
}

void ListView::invalidate_item_group_info_area(size_t index)
{
    const auto items_rect = get_items_rect();
    RECT rc_invalidate = get_item_group_info_area_render_rect(index, items_rect);

    if (IntersectRect(&rc_invalidate, &items_rect, &rc_invalidate)) {
        RedrawWindow(get_wnd(), &rc_invalidate, nullptr, RDW_INVALIDATE);
    }
}

std::tuple<size_t, size_t> ListView::get_item_group_range(size_t index, size_t level) const
{
    if (m_group_count == 0)
        return {size_t{}, m_items.size()};

    const auto& group = m_items[index]->m_groups[level];
    size_t start{index};

    while (start > 0 && m_items[start - 1]->m_groups[level] == group)
        --start;

    size_t end{index};

    while (end + 1 < m_items.size() && m_items[end + 1]->m_groups[level] == group)
        ++end;

    return {start, end - start + 1};
}

void ListView::set_highlight_item(size_t index)
{
    if (m_highlight_item_index != index) {
        m_highlight_item_index = index;
        invalidate_all();
    }
}

void ListView::remove_highlight_item()
{
    if (m_highlight_item_index != pfc_infinite) {
        m_highlight_item_index = pfc_infinite;
        invalidate_all();
    }
}

void ListView::set_highlight_selected_item(size_t index)
{
    if (m_highlight_selected_item_index != index) {
        m_highlight_selected_item_index = index;
        invalidate_all();
    }
}

void ListView::remove_highlight_selected_item()
{
    if (m_highlight_selected_item_index != pfc_infinite) {
        m_highlight_selected_item_index = pfc_infinite;
        invalidate_all();
    }
}

void ListView::set_insert_mark(size_t index)
{
    if (m_insert_mark_index != index) {
        m_insert_mark_index = index;
        invalidate_all();
    }
}
void ListView::remove_insert_mark()
{
    if (m_insert_mark_index != pfc_infinite) {
        m_insert_mark_index = pfc_infinite;
        invalidate_all();
    }
}

bool ListView::disable_redrawing()
{
    if (IsWindowVisible(get_wnd())) {
        SendMessage(get_wnd(), WM_SETREDRAW, FALSE, 0);
        return true;
    }
    return false;
}
void ListView::enable_redrawing()
{
    // if (IsWindowVisible(get_wnd()))
    {
        SendMessage(get_wnd(), WM_SETREDRAW, TRUE, 0);
        invalidate_all(true);
    }
}

void ListView::create_timer_search()
{
    destroy_timer_search();
    if (!m_timer_search) {
        SetTimer(get_wnd(), TIMER_END_SEARCH, 1000, nullptr);
        m_timer_search = true;
    }
}
void ListView::destroy_timer_search()
{
    if (m_timer_search) {
        KillTimer(get_wnd(), TIMER_END_SEARCH);
        m_timer_search = false;
    }
}

void ListView::on_search_string_change(WCHAR new_char)
{
    const auto b_all_same = std::ranges::all_of(m_search_string, [new_char](auto chr) { return chr == new_char; });

    m_search_string.push_back(new_char);

    create_timer_search();
    if (m_columns.empty()) {
        destroy_timer_search();
        return;
    }

    size_t focus = get_focus_item();
    size_t count = m_items.size();
    if (focus == pfc_infinite || focus > m_items.size())
        focus = 0;
    else if (b_all_same) {
        if (focus + 1 == count)
            focus = 0;
        else
            focus++;
    }
    const auto context = create_search_context();

    std::wstring item_text_utf16;

    for (auto offset : std::views::iota(size_t{}, count)) {
        size_t item_index = (offset + focus) % count;
        t_item_ptr item = m_items[item_index];

        const char* item_text = context->get_item_text(item_index);

        if (strchr(item_text, 3)) {
            const auto cleaned_item_text = uih::text_style::remove_colour_and_font_codes(item_text);
            mmh::to_utf16(cleaned_item_text, item_text_utf16);
        } else {
            mmh::to_utf16(item_text, item_text_utf16);
        }

        if ((b_all_same && mmh::search_starts_with({&new_char, 1}, item_text_utf16, false))
            || mmh::search_starts_with(m_search_string, item_text_utf16, false)) {
            if (!is_partially_visible(item_index)) {
                absolute_scroll(get_item_position(item_index), ScrollAxis::Vertical, false, 200.ms);
            }
            set_item_selected_single(item_index);
            break;
        }
    }
}

void ListView::set_window_theme() const
{
    SetWindowTheme(get_wnd(), m_use_dark_mode ? L"DarkMode_Explorer" : nullptr, nullptr);
}

void ListView::reopen_themes()
{
    close_themes();

    if (IsThemeActive() && IsAppThemed()) {
        const auto theme_wnd = m_dummy_theme_window->get_wnd();
        m_list_view_theme.reset(
            OpenThemeData(theme_wnd, m_use_dark_mode ? L"DarkMode_ItemsView::ListView" : L"ItemsView::ListView"));
        if (!m_use_dark_mode)
            m_items_view_theme.reset(OpenThemeData(theme_wnd, L"ItemsView"));
        if (m_use_dark_mode)
            m_header_theme.reset(OpenThemeData(theme_wnd, L"DarkMode_ItemsView::Header"));
        m_dd_theme.reset(OpenThemeData(get_wnd(), VSCLASS_DRAGDROP));
    }

    m_is_high_contrast_active = is_high_contrast_active();
}

void ListView::close_themes()
{
    m_dd_theme.reset();
    m_items_view_theme.reset();
    m_header_theme.reset();
    m_list_view_theme.reset();
}

void ListView::set_use_dark_mode(bool use_dark_mode)
{
    m_use_dark_mode = use_dark_mode;

    set_window_theme();
    set_header_window_theme();
    set_inline_edit_window_theme();
    set_inline_edit_rect();
    set_tooltip_window_theme();
}

void ListView::set_vertical_item_padding(int val)
{
    m_vertical_item_padding = val;
    if (m_initialised) {
        disable_redrawing();

        m_item_height = get_default_item_height();
        m_group_height = get_default_group_height();
        on_size(false);
        refresh_item_positions();

        enable_redrawing();
    }
}

void ListView::set_font_from_log_font(const LOGFONT& log_font)
{
    std::optional<direct_write::TextFormat> text_format;

    if (m_direct_write_context) {
        text_format = m_direct_write_context->create_text_format_with_fallback(log_font);
    }

    set_font(text_format, log_font);
}

void ListView::set_font(std::optional<direct_write::TextFormat> text_format, const LOGFONT& log_font)
{
    m_items_log_font = log_font;
    m_items_text_format = std::move(text_format);
    m_space_width.reset();

    if (m_initialised) {
        exit_inline_edit();
        hide_tooltip();

        refresh_items_font();

        if (m_group_count)
            update_header();

        refresh_item_positions();
    }
}

void ListView::set_header_font(std::optional<direct_write::TextFormat> text_format, const LOGFONT& log_font)
{
    m_header_log_font = log_font;
    m_header_text_format = std::move(text_format);

    if (m_initialised && m_wnd_header) {
        SetWindowFont(m_wnd_header, nullptr, FALSE);
        m_header_font.reset(CreateFontIndirect(&log_font));
        SetWindowFont(m_wnd_header, m_header_font.get(), TRUE);
        on_size();
    }
}
std::optional<float> ListView::get_group_font_size_pt() const
{
    if (!m_group_text_format)
        return {};

    return m_group_text_format->get_font_size_pt();
}

std::optional<float> ListView::get_items_font_size_pt() const
{
    if (!m_items_text_format)
        return {};

    return m_items_text_format->get_font_size_pt();
}

void ListView::set_group_font(std::optional<direct_write::TextFormat> text_format)
{
    m_group_text_format = text_format;

    if (m_initialised) {
        exit_inline_edit();
        hide_tooltip();

        refresh_group_font();
        refresh_item_positions();
    }
}

void ListView::refresh_items_font()
{
    if (m_items_log_font)
        m_items_font.reset(CreateFontIndirect(&*m_items_log_font));

    m_item_height = get_default_item_height();
}

void ListView::refresh_group_font()
{
    m_group_height = get_default_group_height();
}

void ListView::set_sorting_enabled(bool b_val)
{
    m_sorting_enabled = b_val;
    if (m_initialised && m_wnd_header) {
        SetWindowLong(m_wnd_header, GWL_STYLE,
            (GetWindowLongPtr(m_wnd_header, GWL_STYLE) & ~HDS_BUTTONS) | (b_val ? HDS_BUTTONS : 0));
    }
}

void ListView::set_show_sort_indicators(bool b_val)
{
    m_show_sort_indicators = b_val;
    if (m_initialised && m_wnd_header) {
        set_sort_column(m_sort_column_index, m_sort_direction);
    }
}

void ListView::set_edge_style(uint32_t b_val)
{
    m_edge_style = (EdgeStyle)b_val;
    if (get_wnd()) {
        SetWindowLongPtr(get_wnd(), GWL_EXSTYLE,
            (GetWindowLongPtr(get_wnd(), GWL_EXSTYLE) & ~(WS_EX_STATICEDGE | WS_EX_CLIENTEDGE))
                | (m_edge_style == edge_sunken ? WS_EX_CLIENTEDGE
                                               : (m_edge_style == edge_grey ? WS_EX_STATICEDGE : NULL)));
        SetWindowLongPtr(get_wnd(), GWL_STYLE,
            (GetWindowLongPtr(get_wnd(), GWL_STYLE) & ~(WS_BORDER)) | (m_edge_style == edge_solid ? WS_BORDER : NULL));
        SetWindowPos(get_wnd(), nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }
}
} // namespace uih
