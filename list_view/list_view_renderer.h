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

class RendererBase {
public:
    virtual void render_group(ColourData p_data, HDC dc, HTHEME theme, size_t item_index, size_t group_index,
        std::string_view text, int indentation, t_size level, RECT rc)
        = 0;

    virtual ~RendererBase() = default;
};

class DefaultRenderer : public RendererBase {
public:
    void render_group(ColourData p_data, HDC dc, HTHEME theme, size_t item_index, size_t group_index,
        std::string_view text, int indentation, t_size level, RECT rc) override;

private:
    void render_group_line(const ColourData& p_data, HDC dc, HTHEME theme, const RECT* rc);

    void render_group_background(const ColourData& p_data, HDC dc, const RECT* rc);
};

} // namespace uih::lv
