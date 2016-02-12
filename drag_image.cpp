#include "stdafx.h"

namespace uih {
	void DrawDragImageBackground(HWND wnd, HTHEME theme, HDC dc, const RECT & rc, const t_list_view::colour_data_t & colour_data)
	{
		int themeState = 0;

		bool isThemed = theme && colour_data.m_themed && IsThemePartDefined(theme, DD_IMAGEBG, themeState);

		COLORREF cr_text = NULL;
		if (isThemed)
		{
			{
				if (IsThemeBackgroundPartiallyTransparent(theme, DD_IMAGEBG, themeState))
					DrawThemeParentBackground(wnd, dc, &rc);
				DrawThemeBackground(theme, dc, DD_IMAGEBG, themeState, &rc, nullptr);
			}
		}
	}
	void DrawDragImageLabel(HWND wnd, HTHEME theme, HDC dc, const RECT & rc, const t_list_view::colour_data_t & colour_data, const char * text)
	{
		
		int theme_state = 0;
		bool b_themed = theme && colour_data.m_themed && IsThemePartDefined(theme, DD_TEXTBG, theme_state);

		COLORREF cr_text = NULL;
		if (b_themed)
		{
			pfc::stringcvt::string_os_from_utf8 wtext(text);
			DWORD text_flags = DT_SINGLELINE;
			RECT rc_text = { 0 };
			GetThemeTextExtent(theme, dc, DD_TEXTBG, theme_state, wtext, -1, text_flags | DT_CALCRECT, NULL, &rc_text);

			auto x_offset = (RECT_CX(rc) - RECT_CX(rc_text)) / 2;
			auto y_offset = (RECT_CY(rc) - RECT_CY(rc_text)) / 2;
			rc_text.left += x_offset;
			rc_text.right += x_offset;
			rc_text.top += y_offset;
			rc_text.bottom += y_offset;

			MARGINS margins = { 0 };
			GetThemeMargins(theme, dc, DD_TEXTBG, theme_state, TMT_CONTENTMARGINS, &rc_text, &margins);

			RECT background_rect = rc_text;
			background_rect.left -= margins.cxLeftWidth;
			background_rect.top -= margins.cyTopHeight;
			background_rect.bottom += margins.cyBottomHeight;
			background_rect.right += margins.cxRightWidth;

			if (IsThemeBackgroundPartiallyTransparent(theme, DD_TEXTBG, 0))
				DrawThemeParentBackground(wnd, dc, &background_rect);
			DrawThemeBackground(theme, dc, DD_TEXTBG, theme_state, &background_rect, NULL);
			DrawThemeText(theme, dc, DD_TEXTBG, theme_state, wtext, -1, text_flags, 0, &rc_text);
		}
		else
		{
			SIZE sz = { 0 };
			ui_helpers::get_text_size(dc, text, strlen(text), sz);
			auto dpi = uih::GetSystemDpiCached();
			auto padding_cy = MulDiv(sz.cy, dpi.cy, 96 * 14);
			auto padding_cx = MulDiv(sz.cy, dpi.cx, 96 * 4);

			RECT rc_background = {
				RECT_CX(rc) / 2 - sz.cx / 2 - padding_cx,
				RECT_CY(rc) / 2 - sz.cy / 2 - padding_cy,
				RECT_CX(rc) / 2 + sz.cx - sz.cx / 2 + padding_cx,
				RECT_CY(rc) / 2 + sz.cy - sz.cy / 2 + padding_cy
			};
			FillRect(dc, &rc_background, gdi_object_t<HBRUSH>::ptr_t(CreateSolidBrush(colour_data.m_selection_background)));
			ui_helpers::text_out_colours_tab(dc, text, pfc_infinite, 0, 0, &rc, true, colour_data.m_selection_text, false, false, false, ui_helpers::ALIGN_CENTRE);
		}
	}
	void DrawDragImageIcon(HDC dc, const RECT & rc, HICON icon)
	{
		// We may want to use better scaling.
		DrawIconEx(dc, 0, 0, icon, RECT_CX(rc), RECT_CY(rc), NULL, NULL, DI_NORMAL);
	}
}