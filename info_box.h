#pragma once

#include <utility>

namespace uih {

class InfoBox {
public:
    InfoBox(std::function<void(HWND)> on_creation = nullptr, std::function<void(HWND)> on_destruction = nullptr,
        alignment text_alignment = ALIGN_LEFT)
        : m_wnd_edit(nullptr)
        , m_wnd_button(nullptr)
        , m_wnd_static(nullptr)
        , m_text_alignment{text_alignment}
        , m_on_creation(std::move(on_creation))
        , m_on_destruction(std::move(on_destruction))
    {
        auto window_config = uih::ContainerWindowConfig{L"uih_message_window"};
        window_config.window_styles = uih::window_styles::style_popup_default;
        window_config.window_ex_styles = uih::window_styles::ex_style_popup_default;
        m_container_window = std::make_unique<uih::ContainerWindow>(
            window_config, [this](auto&&... args) { return on_message(std::forward<decltype(args)>(args)...); });
    }

    static void s_run(HWND wnd_parent, const char* p_title, const char* p_text, INT icon = OIC_INFORMATION,
        std::function<void(HWND)> on_creation = nullptr, std::function<void(HWND)> on_destruction = nullptr,
        alignment text_alignment = ALIGN_LEFT);

    int calc_height() const;

    int get_text_height() const { return Edit_GetLineCount(m_wnd_edit) * uih::get_font_height(m_font.get()); }

    int get_icon_height() const
    {
        RECT rc_icon;
        GetWindowRect(m_wnd_static, &rc_icon);
        return RECT_CY(rc_icon);
    }

private:
    static int get_large_padding() { return uih::scale_dpi_value(11); }
    static int get_small_padding() { return uih::scale_dpi_value(7); }
    static int get_button_width() { return uih::scale_dpi_value(75); }

    void create(HWND wnd_parent, const char* p_title, const char* p_text, INT oem_icon = OIC_INFORMATION);

    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    DWORD get_edit_alignment_style() const
    {
        switch (m_text_alignment) {
        case ALIGN_LEFT:
        default:
            return ES_LEFT;
        case ALIGN_CENTRE:
            return ES_CENTER;
        case ALIGN_RIGHT:
            return ES_RIGHT;
        }
    }

    HWND m_wnd_edit, m_wnd_button, m_wnd_static;
    alignment m_text_alignment{ALIGN_LEFT};
    wil::unique_hfont m_font;
    icon_ptr m_icon;
    std::function<void(HWND)> m_on_creation;
    std::function<void(HWND)> m_on_destruction;
    std::unique_ptr<uih::ContainerWindow> m_container_window;
};

} // namespace uih
