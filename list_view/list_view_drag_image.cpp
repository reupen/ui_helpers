#include "stdafx.h"

#include "list_view.h"

namespace uih {

bool ListView::render_drag_image(LPSHDRAGIMAGE lpsdi)
{
    pfc::string8 drag_text;
    auto show_text = format_drag_text(get_drag_item_count(), drag_text);

    if (mmh::is_windows_10_or_newer()) {
        if (!m_direct_write_context)
            return false;

        if (!m_drag_image_creator)
            m_drag_image_creator.emplace(static_cast<float>(get_system_dpi_cached().cx));

        return m_drag_image_creator->create_drag_image(
            get_wnd(), m_use_dark_mode, *m_direct_write_context, mmh::to_utf16(drag_text.c_str()), lpsdi);
    }

    ColourData p_data;
    render_get_colour_data(p_data);

    auto icon = get_drag_image_icon();

    LOGFONT lf{};
    if (m_items_log_font)
        lf = *m_items_log_font;
    else
        GetObject(m_items_font.get(), sizeof(lf), &lf);

    return uih::create_drag_image(get_wnd(), true, m_dd_theme.get(), p_data.m_selection_background,
               p_data.m_selection_text, icon.get(), &lf, show_text, drag_text, lpsdi)
        != 0;
}

bool ListView::format_drag_text(size_t selection_count, pfc::string8& p_out)
{
    auto show_text = should_show_drag_text(selection_count);
    if (show_text) {
        p_out.reset();
        p_out << mmh::format_integer(selection_count).c_str() << " "
              << (selection_count != 1 ? get_drag_unit_plural() : get_drag_unit_singular());
    }
    return show_text;
}

} // namespace uih
