#pragma once

#include "d2d_bitmap_renderer.h"
#include "smooth_scroll.h"

namespace uih {

struct AutoscrollCursorInfo {
    UINT initial_vertical_horizontal{};
    UINT initial_vertical{};
    UINT initial_horizontal{};
    UINT scroll_up{};
    UINT scroll_down{};
    UINT scroll_left{};
    UINT scroll_right{};
    UINT scroll_up_right{};
    UINT scroll_up_left{};
    UINT scroll_down_right{};
    UINT scroll_down_left{};
};

class AutoscrollHelper {
public:
    using ScrollCallback = std::function<void(int delta_x, int delta_y)>;

    AutoscrollHelper(
        const AutoscrollCursorInfo& cursor_info, UINT message_id, bool is_dark, ScrollCallback scroll_callback)
        : m_cursor_info(cursor_info)
        , m_message_id(message_id)
        , m_is_dark(is_dark)
        , m_scroll_callback(std::move(scroll_callback))
    {
    }

    void start(HWND wnd, bool set_focus = false);

    std::optional<LRESULT> handle_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

    std::tuple<float, float> get_scroll_velocity() const;

    void set_is_dark(bool is_dark);
    void reset();

private:
    HCURSOR get_hcursor(UINT resource_id);

    SizedHbitmap create_overlay_image();

    void set_cursor();

    void stop();

    AutoscrollCursorInfo m_cursor_info{};
    UINT m_message_id{};
    bool m_is_dark{};
    ScrollCallback m_scroll_callback{};
    HWND m_wnd{};
    HWND m_previously_focused_wnd{};
    bool m_active{};
    bool m_can_scroll_vertically{};
    bool m_can_scroll_horizontally{};
    POINT m_start_screen_pt{};
    float m_x_displacement{};
    float m_y_displacement{};
    bool m_has_scrolled{};
    UINT m_current_cursor_id{};
    std::unordered_map<UINT, wil::unique_hcursor> m_cursors{};
    SmoothScrollTimingThread m_timing_thread{};
    std::chrono::time_point<std::chrono::steady_clock> m_last_tick_time{};
    std::optional<ContainerWindow> m_overlay_window;
    std::optional<D2DBitmapRenderer> m_d2d_bitmap_renderer;
};

} // namespace uih
