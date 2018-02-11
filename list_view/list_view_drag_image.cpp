#include "../stdafx.h"

namespace uih {

bool ListView::render_drag_image(LPSHDRAGIMAGE lpsdi)
{
    ColourData p_data;
    render_get_colour_data(p_data);

    pfc::string8 drag_text;
    auto show_text = format_drag_text(get_drag_item_count(), drag_text);

    auto icon = get_drag_image_icon();

    LOGFONT lf;
    if (m_lf_items_valid)
        lf = m_lf_items;
    else
        GetObject(m_font, sizeof(lf), &lf);

    return uih::create_drag_image(get_wnd(), true, m_dd_theme, p_data.m_selection_background, p_data.m_selection_text,
               icon, &lf, show_text, drag_text, lpsdi)
        != 0;
}

bool ListView::format_drag_text(t_size selection_count, pfc::string8& p_out)
{
    auto show_text = should_show_drag_text(selection_count);
    if (show_text) {
        p_out.reset();
        p_out << mmh::IntegerFormatter(selection_count) << " "
              << (selection_count != 1 ? get_drag_unit_plural() : get_drag_unit_singular());
    }
    return show_text;
}

} // namespace uih