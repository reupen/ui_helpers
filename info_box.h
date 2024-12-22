#pragma once

#include <utility>

namespace uih {

enum class InfoBoxType {
    Neutral,
    Information,
    Warning,
    Error,
};

enum class InfoBoxModalType {
    YesNo,
    OK,
};

class InfoBox : public std::enable_shared_from_this<InfoBox> {
public:
    InfoBox(std::function<std::optional<INT_PTR>(HWND, UINT, WPARAM, LPARAM)> on_before_message,
        alignment text_alignment = ALIGN_LEFT)
        : m_text_alignment{text_alignment}
        , m_on_before_message(std::move(on_before_message))
    {
    }

    static void s_open_modeless(HWND wnd_parent, const char* title, const char* text, InfoBoxType type,
        std::function<std::optional<INT_PTR>(HWND, UINT, WPARAM, LPARAM)> on_before_message = nullptr,
        alignment text_alignment = ALIGN_LEFT);

    static INT_PTR s_open_modal(HWND wnd_parent, const char* title, const char* text, InfoBoxType type,
        InfoBoxModalType modal_type,
        std::function<std::optional<INT_PTR>(HWND, UINT, WPARAM, LPARAM)> on_before_message = nullptr,
        alignment text_alignment = ALIGN_LEFT);

    int calc_height() const;

    int get_text_height() const { return Edit_GetLineCount(m_wnd_edit) * get_font_height(m_font.get()); }

    int get_icon_height() const
    {
        RECT rc_icon;
        GetWindowRect(m_wnd_static, &rc_icon);
        return RECT_CY(rc_icon);
    }

private:
    static int get_large_padding() { return scale_dpi_value(11); }
    static int get_small_padding() { return scale_dpi_value(7); }
    static int get_button_width() { return scale_dpi_value(75); }

    INT_PTR create(HWND wnd_parent, const char* title, const char* text, InfoBoxType type,
        std::optional<InfoBoxModalType> modal_type = {});

    INT_PTR on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    DWORD get_edit_alignment_style() const
    {
        switch (m_text_alignment) {
        default:
            return ES_LEFT;
        case ALIGN_CENTRE:
            return ES_CENTER;
        case ALIGN_RIGHT:
            return ES_RIGHT;
        }
    }

    HWND m_wnd{};
    HWND m_wnd_parent{};
    HWND m_wnd_edit{};
    HWND m_wnd_ok_button{};
    HWND m_wnd_cancel_button{};
    HWND m_wnd_static{};
    InfoBoxType m_type{InfoBoxType::Information};
    std::optional<InfoBoxModalType> m_modal_type;
    int m_button_height{};
    alignment m_text_alignment{ALIGN_LEFT};
    wil::unique_hfont m_font;
    std::string m_message;
    wil::unique_hicon m_icon;
    std::function<std::optional<INT_PTR>(HWND, UINT, WPARAM, LPARAM)> m_on_before_message;
};

} // namespace uih
