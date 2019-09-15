#include "../stdafx.h"

namespace uih {

void ListView::get_selection_state(pfc::bit_array_var& out)
{
    storage_get_selection_state(out);
}

void ListView::set_selection_state(const pfc::bit_array& p_affected, const pfc::bit_array& p_status, bool b_notify,
    notification_source_t p_notification_source)
{
    pfc::bit_array_bittable p_changed(get_item_count());
    if (storage_set_selection_state(p_affected, p_status, &p_changed)) {
        invalidate_items(p_changed);
        if (b_notify)
            notify_on_selection_change(p_changed, p_status, p_notification_source);
    }
}

t_size ListView::get_focus_item()
{
    t_size ret = storage_get_focus_item();
    if (ret >= get_item_count())
        ret = pfc_infinite;
    return ret;
}

void ListView::set_focus_item(t_size index, bool b_notify)
{
    t_size old = storage_get_focus_item();
    if (old != index) {
        storage_set_focus_item(index);
        on_focus_change(old, index);
        if (b_notify)
            notify_on_focus_item_change(index);
    }
};

bool ListView::get_item_selected(t_size index)
{
    return storage_get_item_selected(index);
}

t_size ListView::get_selection_count(t_size max)
{
    return storage_get_selection_count(max);
}

t_size ListView::storage_get_selection_count(t_size max)
{
    t_size i;
    t_size count = m_items.size();
    t_size ret = 0;
    ;
    for (i = 0; i < count; i++) {
        if (get_item_selected(i))
            ret++;
        if (ret == max)
            break;
    }
    return ret;
}

void ListView::set_item_selected(t_size index, bool b_state)
{
    if (b_state)
        set_selection_state(pfc::bit_array_one(index), pfc::bit_array_one(index));
    else
        set_selection_state(pfc::bit_array_one(index), pfc::bit_array_false());
}
void ListView::set_item_selected_single(t_size index, bool b_notify, notification_source_t p_notification_source)
{
    if (index < m_items.size()) {
        set_selection_state(pfc::bit_array_true(), pfc::bit_array_one(index), b_notify, p_notification_source);
        set_focus_item(index, b_notify);
        // ensure_visible(index);
    }
}

// DEFAULT STORAGE
t_size ListView::storage_get_focus_item()
{
    return m_focus_index;
}

void ListView::storage_set_focus_item(t_size index)
{
    m_focus_index = index;
};

void ListView::storage_get_selection_state(pfc::bit_array_var& out) // storage
{
    t_size i;
    t_size count = m_items.size();
    for (i = 0; i < count; i++)
        out.set(i, m_items[i]->m_selected);
}

bool ListView::storage_set_selection_state(const pfc::bit_array& p_affected, const pfc::bit_array& p_status,
    pfc::bit_array_var* p_changed) // storage, returns hint if sel actually changed
{
    bool b_changed = false;
    t_size i;
    t_size count = m_items.size();
    for (i = 0; i < count; i++) {
        if (p_affected[i] && p_status[i] != get_item_selected(i)) {
            b_changed = true;
            m_items[i]->m_selected = p_status[i];
            if (p_changed)
                p_changed->set(i, true);
        }
    }
    return b_changed;
}

bool ListView::storage_get_item_selected(t_size index)
{
    return m_items[index]->m_selected;
}

} // namespace uih
