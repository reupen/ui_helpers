#pragma once

class ListView  {
public:
    enum {
        IDC_HEADER = 1001,
        IDC_TOOLTIP = 1002,
        IDC_INLINEEDIT = 667,
        IDC_SEARCHBOX = 668,
    };

    enum {
        TIMER_SCROLL_UP = 1001,
        TIMER_SCROLL_DOWN = 1002,
        TIMER_END_SEARCH,EDIT_TIMER_ID,
        TIMER_BASE
    };

    enum {
        MSG_KILL_INLINE_EDIT = WM_USER + 3
    };

    struct ColourData {
        bool m_themed;
        bool m_use_custom_active_item_frame;
        COLORREF
        m_text,
        m_selection_text,
        m_inactive_selection_text,
        m_background,
        m_selection_background,
        m_inactive_selection_background,
        m_active_item_frame,
        m_group_background,
        m_group_text;

    };

    class Column {
    public:
        pfc::string8 m_title;
        t_size m_size;
        t_size m_display_size;
        t_size m_autosize_weight;
        uih::alignment m_alignment;

        Column(const char* title, t_size cx, t_size p_autosize_weight = 1, uih::alignment alignment = uih::ALIGN_LEFT)
            : m_title(title), m_size(cx), m_display_size(cx), m_autosize_weight(p_autosize_weight),
              m_alignment(alignment) {};

        Column() : m_size(0), m_display_size(0), m_autosize_weight(1), m_alignment(uih::ALIGN_LEFT) {};
    };

    using string_array = pfc::array_t<pfc::string_simple>;

    class InsertItem {
    public:
        string_array m_subitems;
        string_array m_groups;

        InsertItem() {};

        InsertItem(const string_array& text, const string_array& p_groups)
        {
            m_subitems = text;
            m_groups = p_groups;
        };
        InsertItem(size_t subitem_count, size_t group_count)
        {
            m_subitems.set_size(subitem_count);
            m_groups.set_size(group_count);
        };
    };

    template<size_t subitem_count, size_t group_count>
    class SizedInsertItem : public InsertItem {
    public:
        SizedInsertItem() : InsertItem(subitem_count, group_count) {}
    };

protected:
    enum EdgeStyle {
        edge_none,
        edge_sunken,
        edge_grey,
        edge_solid,
    };

    class Item;
    class Group;

    typedef pfc::refcounted_object_ptr_t<Group> t_group_ptr;
    typedef pfc::refcounted_object_ptr_t<Item> t_item_ptr;

    class Group : public pfc::refcounted_object_root {
    public:
        pfc::string8 m_text;

        Group(const char* p_text) : m_text(p_text) {}

        Group() {}

    private:
    };

    class Item : public pfc::refcounted_object_root {
    public:
        //pfc::list_t<string_array> m_subitems_v2;
        t_uint8 m_line_count;
        string_array m_subitems;
        pfc::array_t<t_group_ptr> m_groups;

        //t_size m_position;
        t_size m_display_index;
        t_size m_display_position;
        bool m_selected;

        void update_line_count()
        {
            m_line_count = 1;
            for (t_size i = 0, count = m_subitems.get_count(); i < count; i++) {
                t_uint8 lc = 1;
                const char* ptr = m_subitems[i];
                while (*ptr) {
                    if (*ptr == '\n') {
                        if (++lc == 255)
                            break;
                    }
                    ptr++;
                }
                m_line_count = std::max(m_line_count, lc);
            }
        }

        /*Item(const t_string_list_const_fast & p_text)
            : m_position(0), m_selected(false) 
            {
                m_subitems.add_items(p_text);
            };
        Item(const t_string_list_const_fast & p_text, t_size group_count)
            : m_position(0), m_selected(false) 
            {
                m_subitems.add_items(p_text);
                m_groups.set_count(group_count);
            };*/
        Item() : /*m_position(0), */m_line_count(1) , m_display_index(0), m_display_position(0), m_selected(false) { };

    private:
    };

public:
    ListView()
    {
        m_dragging_initial_point.x = 0;
        m_dragging_initial_point.y = 0;
        m_dragging_rmb_initial_point.x = 0;
        m_dragging_rmb_initial_point.y = 0;

        auto window_config = uih::ContainerWindowConfig{L"NGLV"};
        window_config.window_styles = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP | WS_BORDER;
        window_config.class_styles = CS_DBLCLKS | CS_HREDRAW;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config,
            [this](auto &&... args) {return on_message(std::forward<decltype(args)>(args)...); }
        );

    }

    /**
    * \brief               Creates a new list view window
    * \param wnd_parent    Handle of the parent window
    * \param window_pos    Initial window position
    * \return              Window handle of the created list view
    */
    HWND create(HWND wnd_parent, uih::WindowPosition window_pos = {}, bool use_dialog_units = false)
    {
        return m_container_window->create(wnd_parent, window_pos, nullptr, use_dialog_units);
    }

    /**
    * \brief   Destroys the list view window.
    */
    void destroy()
    {
        m_container_window->destroy();
    }

    /**
    * \brief   Gets the list view window handle.
    * \return  List view window handle
    */
    HWND get_wnd() const
    {
        return m_container_window->get_wnd();
    }

    unsigned calculate_header_height();
    void set_columns(const pfc::list_base_const_t<Column>& columns);
    void set_column_widths(const pfc::list_base_const_t<t_size>& widths);
    void set_group_count(t_size count, bool b_update_columns = true);

    t_size get_group_count() const
    {
        return m_group_count;
    }

    t_size get_columns_width();
    t_size get_columns_display_width();
    t_size get_column_display_width(t_size index);
    uih::alignment get_column_alignment(t_size index);
    t_size get_column_count();

    void _set_scroll_position(t_size val)
    {
        m_scroll_position = val;
    }

    t_size _get_scroll_position() const
    {
        return m_scroll_position;
    }

    void set_show_header(bool b_val);
    void set_show_tooltips(bool b_val);
    void set_limit_tooltips_to_clipped_items(bool b_val);
    void set_autosize(bool b_val);
    void set_always_show_focus(bool b_val);

    void set_variable_height_items(bool b_variable_height_items)
    {
        m_variable_height_items = b_variable_height_items;
    }

    void set_single_selection(bool b_single_selection)
    {
        m_single_selection = b_single_selection;
    }

    void set_alternate_selection_model(bool b_alternate_selection)
    {
        m_alternate_selection = b_alternate_selection;
    }

    void set_allow_header_rearrange(bool b_allow_header_rearrange)
    {
        m_allow_header_rearrange = b_allow_header_rearrange;
    }

    void set_vertical_item_padding(int val);
    void set_font(const LPLOGFONT lplf);
    void set_group_font(const LPLOGFONT lplf);
    void set_header_font(const LPLOGFONT lplf);
    void set_sorting_enabled(bool b_val);
    void set_show_sort_indicators(bool b_val);
    void set_edge_style(t_size b_val);

    void on_size(bool b_update = true, bool b_update_scroll = true);
    void on_size(int cx, int cy, bool b_update = true, bool b_update_scroll = true);

    void reposition_header();

    void get_column_sizes(pfc::list_t<Column>& p_out);
    void update_column_sizes();

    //void insert_item(t_size index, const t_string_list_const_fast & text, const t_string_list_const_fast & p_groups, t_size size);
    //void insert_items(t_size index_start, const pfc::list_base_const_t<InsertItem> & items, bool b_update_display = true);
    void insert_items(t_size index_start, t_size count, const InsertItem* items, bool b_update_display = true);

    template<class TItems>
    void replace_items(t_size index_start, const TItems& items, bool b_update_display = true)
    {
        replace_items(index_start, items.get_size(), items.get_ptr(), b_update_display);
    }
    void replace_items(t_size index_start, t_size count, const InsertItem* items, bool b_update_display = true);
    void remove_item(t_size index);
    void remove_items(const bit_array& p_mask, bool b_update_display = true);

    enum t_hit_test_value {
        hit_test_nowhere,
        hit_test_on,
        hit_test_above,
        hit_test_below,
        hit_test_obscured_above,
        hit_test_obscured_below,
        hit_test_below_items,
        hit_test_on_group,
        hit_test_right_of_item,
        hit_test_left_of_item,
        hit_test_right_of_group,
        hit_test_left_of_group
    };

    struct t_hit_test_result {
        t_size index;
        t_size insertion_index;
        t_size group_level;
        t_size column;
        //t_size index_visible;
        //t_size index_partially_obscured;
        t_hit_test_value result;

        t_hit_test_result() : index(NULL), insertion_index(NULL), group_level(NULL), column(NULL), result(hit_test_nowhere) {};
    };

    void hit_test_ex(POINT pt_client, t_hit_test_result& result);
    void update_scroll_info(bool b_update = true, bool b_vertical = true, bool b_horizontal = true);
    void _update_scroll_info_vertical();
    void _update_scroll_info_horizontal();
    bool is_visible(t_size index);
    void ensure_visible(t_size index);
    void scroll(bool b_sb, int val, bool b_horizontal = false);

    void get_item_group(t_size index, t_size level, t_size& index_start, t_size& count);
    void set_insert_mark(t_size index);
    void remove_insert_mark();

    void set_highlight_item(t_size index);
    void remove_highlight_item();
    void set_highlight_selected_item(t_size index);
    void remove_highlight_selected_item();

    /** Rect relative to main window client area */
    void get_header_rect(LPRECT rc);

    /** Current height*/
    unsigned get_header_height();
    void get_items_rect(LPRECT rc);
    void get_items_size(LPRECT rc);

    t_size get_items_top()
    {
        RECT rc;
        get_items_rect(&rc);
        return rc.top;
    }

    void get_search_box_rect(LPRECT rc);
    unsigned get_search_box_height();

    void invalidate_all(bool b_update = true, bool b_children = false);
    void invalidate_items(t_size index, t_size count, bool b_update_display = true);

    void invalidate_items(const bit_array& mask, bool b_update_display = true);
    void invalidate_item_group_info_area(t_size index, bool b_update_display = true);

    void update_items(t_size index, t_size count, bool b_update_display = true);
    void update_all_items(bool b_update_display = true);

    // Current implementation clears sub-items.
    void reorder_items_partial(size_t base, const size_t * order, size_t count, bool update_focus_item = true, 
        bool update_display = true);

    t_size get_previous_item(t_size y, bool b_include_headers = false);
    t_size get_next_item(t_size y, bool b_include_headers = false);
    t_size get_last_viewable_item();
    t_size get_last_item();
    int get_default_item_height();
    int get_default_group_height();

    t_size get_item_height() const
    {
        return m_item_height;
    }

    t_size get_item_height(t_size index) const
    {
        t_size ret = 1;
        if (m_variable_height_items && index < m_items.get_count())
            ret = m_items[index]->m_line_count * get_item_height();
        else
            ret = get_item_height();
        return ret;
    }

    void clear_all_items()
    {
        m_items.remove_all();
    }

    t_size get_item_position(t_size index, bool b_include_headers = false)
    {
        t_size ret = 0;
        if (index < m_items.get_count())
            ret = m_items[index]->m_display_position;
        return ret;
    }

    t_size get_item_position_bottom(t_size index, bool b_include_headers = false)
    {
        return get_item_position(index, b_include_headers) + get_item_height(index);
    }

    t_size get_group_minimum_inner_height()
    {
        return get_show_group_info_area() ? get_group_info_area_total_height() : 0;
    }

    t_size get_item_group_bottom(t_size index, bool b_include_headers = false)
    {
        t_size gstart = index, gcount = 0;
        get_item_group(index, m_group_count ? m_group_count - 1 : 0, gstart, gcount);
        t_size ret = 0;
        if (gcount)
            index = gstart + gcount - 1;
        ret += get_item_position(index, b_include_headers);
        ret += m_item_height - 1;
        if (get_show_group_info_area() && m_group_count) {
            t_size gheight = gcount * m_item_height;
            t_size group_cy = get_group_info_area_total_height();
            if (gheight < group_cy)
                ret += group_cy - gheight;
        }
        return ret;
    }

    void refresh_item_positions(bool b_update_display = true)
    {
        __calculate_item_positions();
        update_scroll_info();
        if (b_update_display)
            RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    enum notification_source_t {
        notification_source_unknown,
        notification_source_rmb,
    };

    bool copy_selected_items_as_text(t_size default_single_item_column = pfc_infinite);

    //CLIENT FUNCTIONS
    void get_selection_state(bit_array_var& out);
    void set_selection_state(const bit_array& p_affected, const bit_array& p_status, bool b_notify = true, bool b_update_display = true, notification_source_t p_notification_source = notification_source_unknown);
    t_size get_focus_item();
    void set_focus_item(t_size index, bool b_notify = true, bool b_update_display = true);
    bool get_item_selected(t_size index);

    bool is_range_selected(t_size index, t_size count)
    {
        t_size i;
        for (i = 0; i < count; i++)
            if (!get_item_selected(i + index))
                return false;
        return count != 0;
    }

    t_size get_selection_count(t_size max = pfc_infinite);

    t_size get_selected_item_single()
    {
        t_size numSelected = get_selection_count(2), index = 0;
        if (numSelected == 1) {
            bit_array_bittable mask(get_item_count());
            get_selection_state(mask);
            t_size count = get_item_count();
            while (index < count) {
                if (mask[index])
                    break;
                index++;
            }
        } else
            index = pfc_infinite;
        return index;
    }

    void sort_by_column(t_size index, bool b_descending, bool b_selection_only = false)
    {
        notify_sort_column(index, b_descending, b_selection_only);
        if (!b_selection_only)
            set_sort_column(index, b_descending);
    }

    size_t get_sort_column() const { return m_sort_column_index; }
    bool get_sort_direction() const { return m_sort_direction; }

    void update_item_data(t_size index)
    {
        notify_update_item_data(index);
        //if (m_variable_height_items) 
        {
            //m_items[index]->update_line_count();
            //__calculate_item_positions(index+1);
            //scrollbars?
        }
    };

    //SELECTION HELPERS
    void set_item_selected(t_size index, bool b_state);
    void set_item_selected_single(t_size index, bool b_notify = true, notification_source_t p_notification_source = notification_source_unknown);

    //CLIENT NOTIFICATION
    virtual void notify_on_selection_change(const bit_array& p_affected, const bit_array& p_status, notification_source_t p_notification_source) {};

    virtual void notify_on_focus_item_change(t_size new_index) {};

    virtual void notify_on_initialisation() {}; //set settings here
    virtual void notify_on_create() {}; //populate list here
    virtual void notify_on_destroy() {};

    virtual bool notify_on_keyboard_keydown_filter(UINT msg, WPARAM wp, LPARAM lp, bool& b_processed)
    {
        return false;
    };

    virtual bool notify_on_contextmenu_header(const POINT& pt, const HDHITTESTINFO& ht)
    {
        return false;
    };

    virtual bool notify_on_contextmenu(const POINT& pt)
    {
        return false;
    };

    virtual bool notify_on_timer(UINT_PTR timerid)
    {
        return false;
    };

    virtual void notify_on_time_change() {};

    virtual void notify_on_menu_select(WPARAM wp, LPARAM lp) {};

    virtual bool notify_on_middleclick(bool on_item, t_size index)
    {
        return false;
    };

    virtual bool notify_on_doubleleftclick_nowhere()
    {
        return false;
    };

    virtual void notify_sort_column(t_size index, bool b_descending, bool b_selection_only) {};

    virtual bool notify_on_keyboard_keydown_remove()
    {
        return false;
    };

    virtual bool notify_on_keyboard_keydown_undo()
    {
        return false;
    };

    virtual bool notify_on_keyboard_keydown_redo()
    {
        return false;
    };

    virtual bool notify_on_keyboard_keydown_cut()
    {
        return false;
    };

    virtual bool notify_on_keyboard_keydown_copy()
    {
        copy_selected_items_as_text();
        return true;
    };

    virtual bool notify_on_keyboard_keydown_paste()
    {
        return false;
    };

    virtual bool notify_on_keyboard_keydown_search()
    {
        return false;
    };

    virtual void notify_on_set_focus(HWND wnd_lost) {};

    virtual void notify_on_kill_focus(HWND wnd_receiving) {};

    virtual void notify_on_column_size_change(t_size index, t_size new_width) {};

    virtual void notify_on_group_info_area_size_change(t_size new_width) {};

    virtual void notify_on_header_rearrange(t_size index_from, t_size index_to) {};

    string_array& get_item_subitems(t_size index)
    {
        return m_items[index]->m_subitems;
    } //hmmm
    //const Item * get_item(t_size index) {return m_items[index].get_ptr();} 
    virtual void notify_update_item_data(t_size index) {};

    virtual t_size get_highlight_item()
    {
        return pfc_infinite;
    }

    virtual void execute_default_action(t_size index, t_size column, bool b_keyboard, bool b_ctrl) {};

    virtual void move_selection(int delta) {};

    virtual bool do_drag_drop(WPARAM wp)
    {
        return false;
    };

    bool disable_redrawing();
    void enable_redrawing();

    void update_window()
    {
        UpdateWindow(get_wnd());
    }

    const char* get_item_text(t_size index, t_size column);

    t_size get_item_count()
    {
        return m_items.get_count();
    }

    void activate_inline_editing(t_size column_start = 0);
    void activate_inline_editing(const pfc::list_base_const_t<t_size>& indices, t_size column);
    void activate_inline_editing(t_size index, t_size column);
protected:
    //STORAGE
    virtual t_size storage_get_focus_item();
    virtual void storage_set_focus_item(t_size index);
    virtual void storage_get_selection_state(bit_array_var& out);
    virtual bool storage_set_selection_state(const bit_array& p_affected, const bit_array& p_status, bit_array_var* p_changed = nullptr); //return: hint if sel didnt change
    virtual bool storage_get_item_selected(t_size index);
    virtual t_size storage_get_selection_count(t_size max);

    /*virtual void storage_insert_items(t_size index_start, const pfc::list_base_const_t<InsertItem> & items);
    virtual void storage_replace_items(t_size index_start, const pfc::list_base_const_t<InsertItem> & items);
    virtual void storage_remove_items(const bit_array & p_mask);
    virtual void storaget_set_item_subitems(t_size index, t_string_list_cref_fast p_subitems);
    virtual t_string_list_cref_fast storaget_get_item_subitems(t_size index);*/

    virtual Item* storage_create_item()
    {
        return new Item;
    }

    virtual Group* storage_create_group()
    {
        return new Group;
    }

    Item* get_item(t_size index)
    {
        return m_items[index].get_ptr();
    }

    t_size get_item_display_index(t_size index)
    {
        return m_items[index]->m_display_index;
    }

    t_size get_item_display_group_count(t_size index)
    {
        if (index == 0)
            return m_group_count;
        else {
            t_size counter = 0, i = m_group_count;
            while (i && m_items[index]->m_groups[i - 1] != m_items[index - 1]->m_groups[i - 1]) {
                i--;
                counter++;
            }
            return counter;
        }
    }

    void on_focus_change(t_size index_prev, t_size index_new, bool b_update_display = true);

    void set_group_level_indentation_enabled(bool b_val)
    {
        m_group_level_indentation_enabled = b_val;

        //FIXME set after creation?
    }

    t_size get_item_indentation();
    t_size get_default_indentation_step();

    void set_group_info_area_size(t_size width, t_size height)
    {
        m_group_info_area_width = width;
        m_group_info_area_height = height;
        if (m_initialised) {
            update_column_sizes();
            update_header();
            refresh_item_positions();
        }
    }

    t_size get_group_info_area_width()
    {
        return get_show_group_info_area() ? m_group_info_area_width : 0;
    }

    t_size get_group_info_area_height()
    {
        return get_show_group_info_area() ? m_group_info_area_height : 0;
    }

    t_size get_group_info_area_total_width()
    {
        return get_show_group_info_area() ? m_group_info_area_width + get_default_indentation_step() : 0;
    }

    t_size get_group_info_area_total_height()
    {
        return get_show_group_info_area() ? m_group_info_area_height + get_default_indentation_step() : 0;
    }

    void set_show_group_info_area(bool val)
    {
        bool b_old = get_show_group_info_area();
        m_show_group_info_area = val;
        bool b_new = get_show_group_info_area();
        if (m_initialised && (b_old != b_new)) {
            update_column_sizes();
            update_header();
            refresh_item_positions();
            on_size();
        }
    }

    bool is_header_column_real(t_size index)
    {
        if (m_have_indent_column)
            return index != 0;
        else
            return true;
    }

    t_size header_column_to_real_column(t_size index)
    {
        if (m_have_indent_column && index != pfc_infinite)
            return index - 1;
        else
            return index;
    }

    bool get_show_group_info_area()
    {
        return m_group_count ? m_show_group_info_area : false;
    }

    t_size get_total_indentation()
    {
        return get_item_indentation() + get_group_info_area_total_width();
    }

    virtual void render_get_colour_data(ColourData& p_out);

    void render_group_line_default(const ColourData& p_data, HDC dc, const RECT* rc);
    void render_group_background_default(const ColourData& p_data, HDC dc, const RECT* rc);
    COLORREF get_group_text_colour_default();
    bool get_group_text_colour_default(COLORREF& cr);

    void render_group_default(const ColourData& p_data, HDC dc, const char* text, t_size indentation, t_size level, const RECT& rc);
    void render_item_default(const ColourData& p_data, HDC dc, t_size index, t_size indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool b_focused, const RECT* rc);
    void render_background_default(const ColourData& p_data, HDC dc, const RECT* rc);

    virtual void render_group_info(HDC dc, t_size index, t_size group_count, const RECT& rc) {};

    virtual void render_group(HDC dc, t_size index, t_size group, const char* text, t_size indentation, t_size level, const RECT& rc);
    virtual void render_item(HDC dc, t_size index, t_size indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool b_focused, const RECT* rc);
    virtual void render_background(HDC dc, const RECT* rc);
    virtual bool render_drag_image(LPSHDRAGIMAGE lpsdi);

    virtual icon_ptr get_drag_image_icon()
    {
        return nullptr;
    }

    virtual t_size get_drag_item_count() /*const*/
    {
        return get_selection_count();
    };

    virtual bool should_show_drag_text(t_size selection_count)
    {
        return selection_count > 1;
    }

    virtual bool format_drag_text(t_size selection_count, pfc::string8& p_out);

    virtual const char* get_drag_unit_singular() const
    {
        return "item";
    }

    virtual const char* get_drag_unit_plural() const
    {
        return "items";
    }

    HTHEME get_theme()
    {
        return m_theme;
    }

    void set_sort_column(t_size index, bool b_direction);

    void clear_sort_column()
    {
        set_sort_column(pfc_infinite, false);
    }

    void show_search_box(const char* label, bool b_focus = true);
    void close_search_box(bool b_notify = true);
    bool is_search_box_open();
    void focus_search_box();

    void __search_box_update_hot_status(const POINT& pt);

    virtual void notify_on_search_box_contents_change(const char* p_str) {};

    virtual void notify_on_search_box_close() {};

public:
    void create_timer_scroll_up();
    void create_timer_scroll_down();
    void destroy_timer_scroll_up();
    void destroy_timer_scroll_down();

    enum inline_edit_flags_t {
        inline_edit_uppercase = 1 << 0,
        inline_edit_autocomplete = 1 << 1,
    };

private:
    void create_timer_search();
    void destroy_timer_search();

    void process_keydown(int offset, bool alt_down, bool repeat);
    bool on_wm_notify_header(LPNMHDR lpnm, LRESULT& ret);
    bool on_wm_keydown(WPARAM wp, LPARAM lp, LRESULT& ret, bool& b_processed);

    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    void render_items(HDC dc, const RECT& rc_update, t_size cx);
    void __insert_items_v3(t_size index_start, t_size count, const InsertItem* items);
    void __replace_items_v2(t_size index_start, t_size count, const InsertItem* items);
    void __remove_item(t_size index);
    void __calculate_item_positions(t_size index_start = 0);

    static LRESULT WINAPI g_on_inline_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT on_inline_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    void create_inline_edit(const pfc::list_base_const_t<t_size>& indices, unsigned column);
    void save_inline_edit();
    void exit_inline_edit();

    virtual bool notify_before_create_inline_edit(const pfc::list_base_const_t<t_size>& indices, unsigned column, bool b_source_mouse)
    {
        return false;
    };

    virtual bool notify_create_inline_edit(const pfc::list_base_const_t<t_size>& indices, unsigned column, pfc::string_base& p_test, t_size& p_flags, mmh::ComPtr<IUnknown>& pAutocompleteEntries)
    {
        return true;
    };

    virtual void notify_save_inline_edit(const char* value) {};

    virtual void notify_exit_inline_edit() {};

    void on_search_string_change(WCHAR c);

    static LRESULT WINAPI g_on_search_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT on_search_edit_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    void reset_columns();

    void create_header();
    void destroy_header();
    void build_header();
    void update_header(bool b_update = true);

    void create_tooltip(/*t_size index, t_size column, */const char* str);
    void destroy_tooltip();
    bool is_item_clipped(t_size index, t_size column);
    t_size get_text_width(const char* text, t_size length);

    gdi_object_t<HFONT>::ptr_t m_font, m_font_header, m_group_font;

    HTHEME m_theme{nullptr};
    HTHEME m_dd_theme{nullptr};

    HWND m_wnd_header{nullptr};
    HWND m_wnd_inline_edit{nullptr};
    WNDPROC m_proc_inline_edit{nullptr};
    WNDPROC m_proc_original_inline_edit{nullptr};
    bool m_inline_edit_save{false};
    bool m_inline_edit_saving{false};
    bool m_timer_inline_edit{false};
    bool m_inline_edit_prevent{false};
    bool m_inline_edit_prevent_kill{false};
    t_size m_inline_edit_column{std::numeric_limits<t_size>::max()};
    pfc::list_t<t_size> m_inline_edit_indices;
    //mmh::ComPtr<IUnknown> m_inline_edit_autocomplete_entries;
    mmh::ComPtr<IAutoComplete> m_inline_edit_autocomplete;

    LOGFONT m_lf_items{0};
    LOGFONT m_lf_header{0};
    LOGFONT m_lf_group_header{0};
    bool m_lf_items_valid{false};
    bool m_lf_header_valid{false};
    bool m_lf_group_header_valid{false};

    bool m_selecting{false};
    bool m_selecting_move{false};
    bool m_selecting_moved{false};
    bool m_dragging_rmb{false};
    t_size m_selecting_start{std::numeric_limits<t_size>::max()};
    t_size m_selecting_start_column{std::numeric_limits<t_size>::max()};
    t_hit_test_result m_lbutton_down_hittest;
    int m_scroll_position{0};
    int m_horizontal_scroll_position{0};
    t_size m_group_count{0};
    t_size m_item_height{1};
    t_size m_group_height{1};
    t_size m_shift_start{std::numeric_limits<t_size>::max()};
    bool m_timer_scroll_up{false};
    bool m_timer_scroll_down{false};
    bool m_lbutton_down_ctrl{false};
    t_size m_insert_mark_index{std::numeric_limits<t_size>::max()};
    t_size m_highlight_item_index{std::numeric_limits<t_size>::max()};
    t_size m_highlight_selected_item_index{std::numeric_limits<t_size>::max()};
    POINT m_dragging_initial_point{0};
    POINT m_dragging_rmb_initial_point{0};
    bool m_shown{false};
    t_size m_focus_index{std::numeric_limits<t_size>::max()};
    bool m_autosize{false};
    bool m_initialised{false};
    bool m_always_show_focus{false};
    bool m_show_header{true};
    bool m_ignore_column_size_change_notification{false};
    int m_vertical_item_padding{4};

    bool m_variable_height_items{false};

    bool m_prevent_wm_char_processing{false};
    bool m_timer_search{false};
    pfc::string8 m_search_string;

    bool m_show_tooltips{true};
    bool m_limit_tooltips_to_clipped_items{true};
    HWND m_wnd_tooltip{nullptr};
    RECT m_rc_tooltip{0};
    t_size m_tooltip_last_index{std::numeric_limits<t_size>::max()};
    t_size m_tooltip_last_column{std::numeric_limits<t_size>::max()};

    bool m_sorting_enabled{false};
    bool m_show_sort_indicators{true};
    t_size m_sort_column_index{std::numeric_limits<t_size>::max()};
    bool m_sort_direction{false};
    EdgeStyle m_edge_style{edge_grey};
    bool m_sizing{false};

    bool m_single_selection{false};
    bool m_alternate_selection{false};
    bool m_allow_header_rearrange{false};

    t_size m_group_info_area_width{0};
    t_size m_group_info_area_height{0};
    bool m_show_group_info_area{false};
    bool m_have_indent_column{false};

    HWND m_search_editbox{nullptr};
    WNDPROC m_proc_search_edit{nullptr};
    pfc::string8 m_search_label;
    bool m_search_box_hot{false};
    //HTHEME m_search_box_theme;
    //gdi_object_t<HBRUSH>::ptr_t m_search_box_hot_brush, m_search_box_nofocus_brush;

    bool m_group_level_indentation_enabled{true};

    pfc::list_t<t_item_ptr, pfc::alloc_fast> m_items;
    pfc::list_t<Column> m_columns;

    /**
    * \brief The underlying container window.
    */
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};
