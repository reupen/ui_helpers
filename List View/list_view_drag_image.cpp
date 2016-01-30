#include "..\stdafx.h"


void t_list_view::render_drag_image_default(const colour_data_t & p_data, HDC dc, const RECT & rc, const char * text)
{
	// Draw background
	{
		int theme_state = 0;

		bool b_themed = m_dd_theme && p_data.m_themed && IsThemePartDefined(m_dd_theme, DD_IMAGEBG, theme_state);

		COLORREF cr_text = NULL;
		if (b_themed)
		{
			{
				if (IsThemeBackgroundPartiallyTransparent(m_dd_theme, DD_IMAGEBG, theme_state))
					DrawThemeParentBackground(get_wnd(), dc, &rc);
				DrawThemeBackground(m_dd_theme, dc, DD_IMAGEBG, theme_state, &rc, NULL);
			}
		}
		else
		{
			FillRect(dc, &rc, gdi_object_t<HBRUSH>::ptr_t(CreateSolidBrush(p_data.m_selection_background)));
		}
	}

	// Draw label
	{
		int theme_state = 0;
		bool b_themed = m_dd_theme && p_data.m_themed && IsThemePartDefined(m_dd_theme, DD_TEXTBG, theme_state);

		COLORREF cr_text = NULL;
		if (b_themed)
		{
			pfc::stringcvt::string_os_from_utf8 wtext(text);
			DWORD text_flags = DT_SINGLELINE;
			RECT rc_text = { 0 };
			GetThemeTextExtent(m_dd_theme, dc, DD_TEXTBG, theme_state, wtext, -1, text_flags | DT_CALCRECT, NULL, &rc_text);

			auto x_offset = (RECT_CX(rc) - RECT_CX(rc_text)) / 2;
			auto y_offset = (RECT_CY(rc) - RECT_CY(rc_text)) / 2;
			rc_text.left += x_offset;
			rc_text.right += x_offset;
			rc_text.top += y_offset;
			rc_text.bottom += y_offset;

			MARGINS margins = { 0 };
			GetThemeMargins(m_dd_theme, dc, DD_TEXTBG, theme_state, TMT_CONTENTMARGINS, &rc_text, &margins);

			RECT background_rect = rc_text;
			background_rect.left -= margins.cxLeftWidth;
			background_rect.top -= margins.cyTopHeight;
			background_rect.bottom += margins.cyBottomHeight;
			background_rect.right += margins.cxRightWidth;

			if (IsThemeBackgroundPartiallyTransparent(m_dd_theme, DD_TEXTBG, 0))
				DrawThemeParentBackground(get_wnd(), dc, &background_rect);
			DrawThemeBackground(m_dd_theme, dc, DD_TEXTBG, theme_state, &background_rect, NULL);
			DrawThemeText(m_dd_theme, dc, DD_TEXTBG, theme_state, wtext, -1, text_flags, 0, &rc_text);
		}
		else
		{
			ui_helpers::text_out_colours_tab(dc, text, pfc_infinite, 0, 0, &rc, true, p_data.m_selection_text, false, false, false, ui_helpers::ALIGN_CENTRE);
		}

	}

}
void t_list_view::render_drag_image(HDC dc, const RECT & rc, const char * text)
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

	render_drag_image_default(p_data, dc, rc, text);

	SelectFont(dc, font_old);
	DeleteFont(fnt);

}

void t_list_view::format_drag_text(t_size selection_count, pfc::string8 & p_out)
{
	p_out.reset();
	p_out << mmh::format_integer(selection_count) << " " << (selection_count == 1 ? get_drag_unit_singular() : get_drag_unit_plural());
}
