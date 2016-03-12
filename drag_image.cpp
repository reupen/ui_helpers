#include "stdafx.h"

namespace uih {
	// Not used – the shell draws the background for us
	void DrawDragImageBackground(HWND wnd, bool isThemed, HTHEME theme, HDC dc, const RECT & rc)
	{
		int themeState = 0;

		bool useTheming = theme && isThemed && IsThemePartDefined(theme, DD_IMAGEBG, themeState);

		COLORREF cr_text = NULL;
		if (useTheming)
		{
			{
				if (IsThemeBackgroundPartiallyTransparent(theme, DD_IMAGEBG, themeState))
					DrawThemeParentBackground(wnd, dc, &rc);
				DrawThemeBackground(theme, dc, DD_IMAGEBG, themeState, &rc, nullptr);
			}
		}
	}
	void DrawDragImageLabel(HWND wnd, bool isThemed, HTHEME theme, HDC dc, const RECT & rc, COLORREF selectionBackgroundColour, COLORREF selectionTextColour, const char * text)
	{
		int theme_state = 0;
		bool useTheming = theme && isThemed && IsThemePartDefined(theme, DD_TEXTBG, theme_state);

		COLORREF cr_text = NULL;
		if (useTheming)
		{
			pfc::stringcvt::string_os_from_utf8 wtext(text);
			DWORD text_flags = DT_CENTER | DT_WORDBREAK;
			RECT rc_text = { 0 };
			GetThemeTextExtent(theme, dc, DD_TEXTBG, theme_state, wtext, -1, text_flags, &rc, &rc_text);

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
			FillRect(dc, &rc_background, gdi_object_t<HBRUSH>::ptr_t(CreateSolidBrush(selectionBackgroundColour)));
			ui_helpers::text_out_colours_tab(dc, text, pfc_infinite, 0, 0, &rc, true, selectionTextColour, false, false, false, ui_helpers::ALIGN_CENTRE);
		}
	}
	void DrawDragImageIcon(HDC dc, const RECT & rc, HICON icon)
	{
		// We may want to use better scaling.
		DrawIconEx(dc, 0, 0, icon, RECT_CX(rc), RECT_CY(rc), NULL, NULL, DI_NORMAL);
	}
	SIZE GetDragImageContentSize(HDC dc, bool isThemed, HTHEME theme)
	{
		auto sz = uih::GetSystemDpiCached();
		auto margins = MARGINS{ 0 };
		auto themeState = 0;
		auto hr = HRESULT{ S_OK };

		auto useTheming = theme && isThemed && IsThemePartDefined(theme, DD_IMAGEBG, themeState);
		if (useTheming) {
			hr = GetThemePartSize(theme, dc, DD_IMAGEBG, themeState, nullptr, TS_DRAW, &sz);
			if (SUCCEEDED(hr)) {
				hr = GetThemeMargins(theme, dc, DD_IMAGEBG, themeState, TMT_CONTENTMARGINS, nullptr, &margins);
			}
			if (SUCCEEDED(hr)) {
				sz.cx -= margins.cxLeftWidth + margins.cxRightWidth;
				sz.cy -= margins.cyBottomHeight + margins.cyTopHeight;
			}
		}

		return sz;
	}
	BOOL CreateDragImage(HWND wnd, bool isThemed, HTHEME theme, COLORREF selectionBackgroundColour, COLORREF selectionTextColour,
		HICON icon, const LPLOGFONT font, bool showText, const char * text, LPSHDRAGIMAGE lpsdi)
	{
		HDC dc = GetDC(wnd);
		HDC dc_mem = CreateCompatibleDC(dc);

		SIZE size = GetDragImageContentSize(dc, isThemed, theme);
		RECT rc = { 0, 0, size.cx, size.cy };

		HBITMAP bm_mem = CreateCompatibleBitmap(dc, size.cx, size.cy); // Not deleted - the shell takes ownership.
		HBITMAP bm_old = SelectBitmap(dc_mem, bm_mem);

		pfc::string8 drag_text;

		LOGFONT lf = *font;

		lf.lfWeight = FW_BOLD;
		//lf.lfQuality = NONANTIALIASED_QUALITY;
		HFONT fnt = CreateFontIndirect(&lf);

		HFONT font_old = SelectFont(dc_mem, fnt);

		if (icon)
			uih::DrawDragImageIcon(dc_mem, rc, icon);

		// Draw label
		if (showText)
			uih::DrawDragImageLabel(wnd, isThemed, theme, dc_mem, rc, selectionBackgroundColour, selectionTextColour, text);

		SelectFont(dc_mem, font_old);
		DeleteFont(fnt);

		SelectObject(dc_mem, bm_old);
		DeleteDC(dc_mem);
		ReleaseDC(wnd, dc);

		lpsdi->sizeDragImage.cx = size.cx;
		lpsdi->sizeDragImage.cy = size.cy;
		lpsdi->ptOffset.x = size.cx / 2;
		lpsdi->ptOffset.y = size.cy - size.cy / 10;
		lpsdi->hbmpDragImage = bm_mem;
		lpsdi->crColorKey = 0xffffffff;

		return TRUE;
	}
}