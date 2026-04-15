#pragma once

#include "list_view_renderer.h"
#include "../direct_write.h"

namespace uih::lv {

static constexpr short IDC_SEARCH_BAR_EDIT = 2000;
static constexpr short IDC_SEARCH_BAR_LEFT_TOOLBAR = 2001;
static constexpr short IDC_SEARCH_BAR_RIGHT_TOOLBAR = 2002;

static constexpr short ID_SEARCH_BAR_UP = 2100;
static constexpr short ID_SEARCH_BAR_DOWN = 2101;
static constexpr short ID_SEARCH_BAR_CLOSE = 2102;

enum class SearchBarIconId {
    Up,
    Down,
    Close,
};

struct SearchBarHost {
    using Ptr = std::shared_ptr<SearchBarHost>;

    virtual void on_previous() = 0;
    virtual void on_next() = 0;
    virtual void on_return() = 0;
    virtual void on_char(const wchar_t chr) {}
    virtual void on_string_replaced(const wchar_t* text) {}
    virtual void on_close() {}
    virtual bool on_keydown(WPARAM wp) { return false; }
    virtual std::variant<wil::unique_hbitmap, wil::unique_hicon> create_icon(
        SearchBarIconId icon_id, int width, int height, bool is_dark) = 0;

    virtual ~SearchBarHost() {}
};

struct SearchBarToolbarState {
    wil::unique_hwnd wnd;
    wil::unique_himagelist imagelist;
    int width{};
    int height{};
};

class SearchBar {
public:
    using OnFocusCallback = std::function<void(HWND)>;
    using OnDestroyCallback = std::function<void()>;

    operator bool() const { return m_edit_control.is_valid(); }

    void create(HWND parent_wnd, const char* label, HFONT font, int item_height, bool is_dark);
    void destroy();
    void shut_down();

    void set_host(SearchBarHost::Ptr host) { m_search_bar_host = std::move(host); }
    void focus() const { SetFocus(m_edit_control.get()); }
    void select_all() const { SendMessage(m_edit_control.get(), EM_SETSEL, 0, -1); }

    void set_destroy_callback(OnDestroyCallback on_destroy) { m_on_destroy_func = std::move(on_destroy); }
    void set_kill_focus_callback(OnFocusCallback on_kill_focus) { m_on_kill_focus_func = std::move(on_kill_focus); }
    void set_set_focus_callback(OnFocusCallback on_set_focus) { m_on_set_focus_func = std::move(on_set_focus); }

    struct Metrics {
        int edit_width{};
        int edit_height{};
        int total_height{};
    };

    [[nodiscard]] Metrics get_metrics() const;
    [[nodiscard]] int get_total_height() const;
    [[nodiscard]] HWND get_edit_wnd() const { return m_edit_control.get(); }

    void reposition() const;
    void reposition(int client_width, int client_height) const;

    void previous() const
    {
        if (m_search_bar_host)
            m_search_bar_host->on_previous();
    }

    void next() const
    {
        if (m_search_bar_host)
            m_search_bar_host->on_next();
    }

    void on_string_change();

    void set_use_dark_mode(bool is_dark);
    void set_font(HFONT font);

    void set_results_text(std::wstring_view new_text);
    void invalidate() const;
    RECT render(HDC dc, int items_width, int items_bottom, const ColourData& colours,
        const std::optional<direct_write::TextFormat>& text_format);

private:
    static int get_horizontal_padding();
    static int get_vertical_padding();

    wil::unique_hwnd m_edit_control;
    bool m_ignore_next_wm_char_message{};
    bool m_ignore_next_wm_syschar_message{};
    SearchBarToolbarState m_left_toolbar;
    SearchBarToolbarState m_right_toolbar;
    HWND m_parent_wnd{};
    bool m_is_dark{};
    pfc::string8 m_search_label;
    int m_edit_height{};
    std::wstring m_results_text;
    SearchBarHost::Ptr m_search_bar_host;
    std::wstring m_last_string{};
    OnDestroyCallback m_on_destroy_func;
    OnFocusCallback m_on_set_focus_func;
    OnFocusCallback m_on_kill_focus_func;
};

} // namespace uih::lv
