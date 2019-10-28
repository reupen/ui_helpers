#pragma once

namespace uih::lv {

struct ColourData {
    bool m_themed;
    bool m_use_custom_active_item_frame;
    COLORREF m_text;
    COLORREF m_selection_text;
    COLORREF m_inactive_selection_text;
    COLORREF m_background;
    COLORREF m_selection_background;
    COLORREF m_inactive_selection_background;
    COLORREF m_active_item_frame;
    COLORREF m_group_background;
    COLORREF m_group_text;
};

struct RendererSubItem {
    std::string_view text;
    int width;
    alignment alignment;
};

struct RendererContext {
    ColourData colours;
    HWND wnd;
    HDC dc;
    HTHEME list_view_theme;
    HTHEME items_view_theme;
};

class RendererBase {
public:
    virtual void render_background(RendererContext context, const RECT* rc) = 0;

    virtual void render_group_info(RendererContext context, t_size index, RECT rc) {}

    virtual void render_group(RendererContext context, size_t item_index, size_t group_index, std::string_view text,
        int indentation, t_size level, RECT rc)
        = 0;

    virtual void render_item(RendererContext context, t_size index, std::vector<RendererSubItem> sub_items,
        int indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus,
        bool b_focused, RECT rc)
        = 0;

    virtual ~RendererBase() = default;
};

class DefaultRenderer : public RendererBase {
public:
    void render_background(RendererContext context, const RECT* rc) override;

    void render_group(RendererContext context, size_t item_index, size_t group_index, std::string_view text,
        int indentation, t_size level, RECT rc) override;

    void render_item(RendererContext context, t_size index, std::vector<RendererSubItem> sub_items, int indentation,
        bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus, bool b_focused,
        RECT rc) override;

protected:
    void render_group_line(RendererContext context, const RECT* rc);

    void render_group_background(RendererContext context, const RECT* rc);

    void render_focus_rect(RendererContext context, bool should_hide_focus, RECT rc) const;
};

} // namespace uih::lv
