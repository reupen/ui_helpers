#include "..\stdafx.h"

void t_list_view::render_drag_image_default(const colour_data_t & p_data, HDC dc, const RECT & rc, bool draw_text, const char * text)
{
	// Draw background
	uih::DrawDragImageBackground(get_wnd(), m_dd_theme, dc, rc, p_data);

	render_drag_image_icon(dc, rc);

	// Draw label
	if (draw_text)
		uih::DrawDragImageLabel(get_wnd(), m_dd_theme, dc, rc, p_data, text);
}
void t_list_view::render_drag_image(HDC dc, const RECT & rc, bool show_text, const char * text)
{
	colour_data_t p_data;
	render_get_colour_data(p_data);

	LOGFONT lf;
	if (m_lf_items_valid) lf = m_lf_items;
	else
		GetObject(m_font, sizeof(lf), &lf);

	lf.lfWeight = FW_BOLD;
	//lf.lfQuality = NONANTIALIASED_QUALITY;
	HFONT fnt = CreateFontIndirect(&lf);

	HFONT font_old = SelectFont(dc, fnt);

	render_drag_image_default(p_data, dc, rc, show_text, text);

	SelectFont(dc, font_old);
	DeleteFont(fnt);

}

bool t_list_view::format_drag_text(t_size selection_count, pfc::string8 & p_out)
{
	auto show_text = selection_count > 1;
	if (show_text)
	{
		p_out.reset();
		p_out << mmh::format_integer(selection_count) << " " << get_drag_unit_plural();
	}
	return show_text;
}
