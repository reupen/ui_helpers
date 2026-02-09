#pragma once
#include "../direct_write.h"

namespace uih::lv {

struct ColourData {
    bool m_themed{};
    bool m_use_custom_active_item_frame{};
    COLORREF m_text{};
    COLORREF m_selection_text{};
    COLORREF m_inactive_selection_text{};
    COLORREF m_background{};
    COLORREF m_selection_background{};
    COLORREF m_inactive_selection_background{};
    COLORREF m_active_item_frame{};
    COLORREF m_group_background{};
    COLORREF m_group_text{};
};

struct RendererSubItem {
    std::string_view text;
    int width{};
    alignment alignment{};
};

struct RendererContext {
    ColourData colours{};
    bool use_dark_mode{};
    bool high_contrast_active{};
    HWND wnd{};
    HDC dc{};
    HTHEME list_view_theme{};
    HTHEME items_view_theme{};
    std::optional<direct_write::TextFormat>& item_text_format;
    std::optional<direct_write::TextFormat>& group_text_format;
    wil::com_ptr<IDWriteBitmapRenderTarget> bitmap_render_target;
};

class RendererBase {
public:
    [[nodiscard]] virtual bool are_tab_columns_enabled() const { return false; }

    virtual void render_begin(const RendererContext& context) {}

    virtual void render_background(const RendererContext& context, const RECT* rc) = 0;

    virtual void render_group_info(const RendererContext& context, size_t index, RECT rc) {}

    virtual void render_group(const RendererContext& context, size_t item_index, size_t group_index,
        std::string_view text, int indentation, RECT rc)
        = 0;

    virtual void render_item(const RendererContext& context, size_t index, std::vector<RendererSubItem> sub_items,
        int indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus,
        bool b_focused, RECT rc)
        = 0;

    virtual ~RendererBase() = default;
};

class DefaultRenderer : public RendererBase {
public:
    DefaultRenderer(bool enable_item_tab_columns = false) : m_enable_item_tab_columns{enable_item_tab_columns} {}

    bool are_tab_columns_enabled() const override { return m_enable_item_tab_columns; }

    void render_background(const RendererContext& context, const RECT* rc) override;

    void render_group(const RendererContext& context, size_t item_index, size_t group_index, std::string_view text,
        int indentation, RECT rc) override;

    void render_item(const RendererContext& context, size_t index, std::vector<RendererSubItem> sub_items,
        int indentation, bool b_selected, bool b_window_focused, bool b_highlight, bool should_hide_focus,
        bool b_focused, RECT rc) override;

protected:
    void render_group_line(const RendererContext& context, const RECT* rc);

    void render_group_background(const RendererContext& context, const RECT* rc);

    void render_focus_rect(const RendererContext& context, bool should_hide_focus, RECT rc) const;

private:
    bool m_enable_item_tab_columns{};
};

} // namespace uih::lv
