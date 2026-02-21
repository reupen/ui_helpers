#pragma once

namespace uih {

enum class ScrollAxis {
    Vertical,
    Horizontal
};

constexpr auto scroll_axis_to_win32_type(ScrollAxis axis)
{
    return axis == ScrollAxis::Vertical ? SB_VERT : SB_HORZ;
}

int set_scroll_position(HWND wnd, ScrollAxis axis, int old_position, int new_position);

class SmoothScrollHelper {
public:
    using Duration = std::chrono::duration<double, std::milli>;
    static constexpr auto default_duration = Duration(250.);

    SmoothScrollHelper(HWND wnd, uint32_t message_id, uint32_t timer_id, std::function<int()> current_vertical_position,
        std::function<int()> current_horizontal_position,
        std::function<void(ScrollAxis axis, int new_position)> handle_scroll)
        : m_wnd(wnd)
        , m_message_id(message_id)
        , m_timer_id(timer_id)
        , m_vertical_state(std::move(current_vertical_position))
        , m_horizontal_state(std::move(current_horizontal_position))
        , m_handle_scroll(std::move(handle_scroll))

    {
    }
    ~SmoothScrollHelper() { stop_timer_thread(); }

    int current_target(ScrollAxis axis) const
    {
        auto& state = axis_state(axis);
        return state.scroll_state ? state.scroll_state->start_position + state.scroll_state->target_delta
                                  : state.current_position();
    }

    void absolute_scroll(ScrollAxis axis, int target_position, Duration duration = default_duration);
    void delta_scroll(ScrollAxis axis, int delta, Duration duration = default_duration);
    void abandon_animation(ScrollAxis axis, bool update_scroll_position = true);

    void on_timer()
    {
        stop_timer_thread();
        stop_thread_shutdown_timer();
    }

    void on_message()
    {
        assert(m_vertical_state.scroll_state || m_horizontal_state.scroll_state);

        if (m_vertical_state.scroll_state)
            scroll(ScrollAxis::Vertical);

        if (m_horizontal_state.scroll_state)
            scroll(ScrollAxis::Horizontal);
    }

    bool should_smooth_scroll_mouse_wheel(ScrollAxis axis, int wheel_delta);

    void shut_down()
    {
        stop_thread_shutdown_timer();
        stop_timer_thread();
    }

private:
    enum class EasingFunction {
        Linear,
        CubicEaseOut,
    };

    struct ScrollState {
        std::chrono::time_point<std::chrono::steady_clock> start_time{};
        int start_position{};
        int target_delta{};
        Duration duration{default_duration};
        EasingFunction easing_function{EasingFunction::CubicEaseOut};
    };

    struct AxisState {
        std::function<int()> current_position;
        std::optional<ScrollState> scroll_state;
        std::optional<uint64_t> last_mouse_wheel_tick_count;
    };

    static constexpr wil::zwstring_view thread_name{L"[UI helpers] Smooth scroll thread"};

    AxisState& axis_state(ScrollAxis axis)
    {
        return axis == ScrollAxis::Vertical ? m_vertical_state : m_horizontal_state;
    }
    const AxisState& axis_state(ScrollAxis axis) const
    {
        return axis == ScrollAxis::Vertical ? m_vertical_state : m_horizontal_state;
    }

    void scroll(ScrollAxis axis);

    void update_state(ScrollAxis axis, int delta, bool accumulate, Duration duration,
        std::chrono::time_point<std::chrono::steady_clock> now);

    int remaining_delta(ScrollAxis axis) const
    {
        auto& state = axis_state(axis);
        return state.scroll_state->target_delta - state.current_position() + state.scroll_state->start_position;
    }

    void start_timer_thread();
    void pause_timer_thread();
    void stop_timer_thread();

    void start_thread_shutdown_timer();
    void stop_thread_shutdown_timer();

    HWND m_wnd{};
    uint32_t m_message_id{};
    uint32_t m_timer_id{};
    AxisState m_vertical_state{};
    AxisState m_horizontal_state{};
    std::function<void(ScrollAxis axis, int new_position)> m_handle_scroll;
    std::atomic<bool> m_timer_active{};
    bool m_shutdown_timer_active{};
    wil::unique_event_nothrow m_shutdown_event;
    std::optional<std::jthread> m_timer_thread;
};

} // namespace uih
