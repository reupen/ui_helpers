#include "stdafx.h"

#include "scroll.h"

#include "dcomp_utils.h"
#include "dxgi_utils.h"

using namespace std::chrono_literals;

namespace uih {

namespace {

class FrameTimeAverager {
public:
    void add_frame(double frame_time)
    {
        m_sum -= m_frame_times[m_index];
        m_frame_times[m_index] = frame_time;
        m_sum += frame_time;

        m_index = (m_index + 1) % num_frames;

        if (m_count < num_frames)
            ++m_count;
    }

    double get_average() const
    {
        if (m_count == 0)
            return 0.;

        return m_sum / m_count;
    }

private:
    static constexpr size_t num_frames{25};

    std::array<double, num_frames> m_frame_times{};
    double m_sum{0.};
    size_t m_index{};
    size_t m_count{};
};

class FrameTimeMinimum {
public:
    void add_frame_time(double frame_time)
    {
        const double evicted = m_frame_times[m_index];
        m_frame_times[m_index] = frame_time;
        m_index = (m_index + 1) % num_frames;

        if (m_count < num_frames)
            ++m_count;

        if (frame_time <= m_minimum)
            m_minimum = frame_time;
        else if (evicted == m_minimum)
            update_minimum();
    }

    double get_minimum() const { return m_minimum; }

private:
    void update_minimum()
    {
        m_minimum = m_frame_times[0];

        for (size_t i{1}; i < m_count; ++i)
            m_minimum = std::min(m_minimum, m_frame_times[i]);
    }

    static constexpr size_t num_frames{10};

    std::array<double, num_frames> m_frame_times{};
    double m_minimum{std::numeric_limits<double>::max()};
    size_t m_index{};
    size_t m_count{};
};

} // namespace

int set_scroll_position(HWND wnd, ScrollAxis axis, int old_position, int new_position)
{
    if (old_position == new_position)
        return old_position;

    const auto obj_id = axis == ScrollAxis::Vertical ? OBJID_VSCROLL : OBJID_HSCROLL;

    SCROLLBARINFO old_sbi{};
    old_sbi.cbSize = sizeof(old_sbi);
    GetScrollBarInfo(wnd, obj_id, &old_sbi);

    SCROLLINFO position_si{};
    position_si.cbSize = sizeof(SCROLLINFO);
    position_si.fMask = SIF_POS;
    position_si.nPos = new_position;

    const auto actual_new_position = SetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &position_si, FALSE);

    if (actual_new_position == old_position)
        return old_position;

    SCROLLBARINFO new_sbi{};
    new_sbi.cbSize = sizeof(new_sbi);
    GetScrollBarInfo(wnd, obj_id, &new_sbi);

    if (old_sbi.xyThumbTop != new_sbi.xyThumbTop) {
        SCROLLINFO redraw_si{};
        redraw_si.cbSize = sizeof(SCROLLINFO);
        SetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &redraw_si, TRUE);
    }

    return actual_new_position;
}

int clamp_scroll_delta(HWND wnd, ScrollAxis axis, int delta)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    GetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &si);

    if (si.nPos + delta < si.nMin)
        return si.nMin - si.nPos;

    const auto page_size = gsl::narrow_cast<int>(si.nPage);
    const auto adjusted_max = si.nMax - page_size + (page_size > 0 ? 1 : 0);

    if (si.nPos + delta > adjusted_max)
        return adjusted_max - si.nPos;

    return delta;
}

int clamp_scroll_position(HWND wnd, ScrollAxis axis, int position)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE;
    GetScrollInfo(wnd, scroll_axis_to_win32_type(axis), &si);

    if (position < si.nMin)
        return si.nMin;

    const auto page_size = gsl::narrow_cast<int>(si.nPage);
    const auto adjusted_max = si.nMax - page_size + (page_size > 0 ? 1 : 0);

    if (position > adjusted_max)
        return adjusted_max;

    return position;
}

void SmoothScrollHelper::absolute_scroll(ScrollAxis axis, int target_position, Duration duration)
{
    auto& state = axis_state(axis);
    const auto current_position = state.current_position();
    const auto now = std::chrono::steady_clock::now();
    update_state(axis, target_position - current_position, false, duration, now);

    const auto has_exiting_timer = m_timer_thread.has_value();
    start_timer_thread();

    if (!has_exiting_timer)
        on_message();
}

void SmoothScrollHelper::delta_scroll(ScrollAxis axis, int delta, Duration duration)
{
    const auto now = std::chrono::steady_clock::now();
    update_state(axis, delta, true, duration, now);

    const auto has_exiting_timer = m_timer_thread.has_value();
    start_timer_thread();

    if (!has_exiting_timer)
        on_message();
}

void SmoothScrollHelper::abandon_animation(ScrollAxis axis, bool update_scroll_position)
{
    auto& state = axis_state(axis);

    if (!state.scroll_state)
        return;

    pause_timer_thread();

    if (update_scroll_position)
        m_handle_scroll(axis, state.scroll_state->start_position + state.scroll_state->target_delta);

    state.scroll_state.reset();
}

bool SmoothScrollHelper::should_smooth_scroll_mouse_wheel(ScrollAxis axis, int wheel_delta)
{
    auto& saved_tick_count = axis_state(axis).last_mouse_wheel_tick_count;
    const auto last_tick_count = saved_tick_count;
    const auto new_tick_count = GetTickCount64();
    saved_tick_count = new_tick_count;

    // Windows send high frequency mouse wheel messages for high precision touchpads,
    // effectively scrolling smoothly for us. Unfortunately, what kind of scroll device
    // was used isn’t clear from a mouse wheel message, and so it’s inferred here

    if (abs(wheel_delta) < WHEEL_DELTA)
        return false;

    if (!last_tick_count || new_tick_count - *last_tick_count >= 2500)
        return wheel_delta % WHEEL_DELTA == 0;

    return new_tick_count - *last_tick_count > 50;
}

void SmoothScrollHelper::scroll(ScrollAxis axis)
{
    auto& state = axis_state(axis);
    auto& scroll_state = *state.scroll_state;

    const auto now = std::chrono::steady_clock::now();
    const auto normalised_time = (now - scroll_state.start_time) / scroll_state.duration;

    const auto scroll_distance = scroll_state.target_delta;
    const auto progress = scroll_state.easing_function == EasingFunction::Linear
        ? normalised_time
        : 1. - std::pow(1. - normalised_time, 3);

    const auto scroll_amount = static_cast<int>(std::round(scroll_distance * progress));

    const auto new_scroll_position = scroll_state.start_position + scroll_amount;
    const auto bounded_new_scroll_position
        = std::clamp(new_scroll_position, scroll_state.start_position + std::min(scroll_state.target_delta, 0),
            scroll_state.start_position + std::max(scroll_state.target_delta, 0));
    m_handle_scroll(axis, bounded_new_scroll_position);

    const auto updated_position = state.current_position();

    if (updated_position != new_scroll_position
        || updated_position == scroll_state.start_position + scroll_state.target_delta) {
        pause_timer_thread();
        state.scroll_state.reset();
    }
}

void SmoothScrollHelper::update_state(ScrollAxis axis, int delta, bool accumulate, Duration duration,
    std::chrono::time_point<std::chrono::steady_clock> now)
{
    auto& state = axis_state(axis);
    auto& scroll_state = state.scroll_state;
    auto current_position = state.current_position();

    if (!scroll_state || !accumulate) {
        scroll_state.emplace(now - 8ms, current_position, delta, duration);
        return;
    }

    if (scroll_state->easing_function == EasingFunction::Linear) {
        scroll_state->target_delta += delta;
        scroll_state->duration = now - scroll_state->start_time + duration / 2.;
    } else {
        scroll_state->target_delta += scroll_state->start_position - current_position + delta;
        scroll_state->start_position = current_position;
        scroll_state->duration = duration / 2.;
        scroll_state->start_time = now;
        scroll_state->easing_function = EasingFunction::Linear;
    }
}

void SmoothScrollHelper::start_timer_thread()
{
    m_timer_active.store(true, std::memory_order_release);
    stop_thread_shutdown_timer();

    if (m_timer_thread)
        return;

    if (m_shutdown_event)
        m_shutdown_event.ResetEvent();
    else
        m_shutdown_event.create();

    m_timer_thread = std::jthread([this](std::stop_token stop_token) {
        mmh::set_thread_description(GetCurrentThread(), thread_name.c_str());

        dcomp::DcompApi dcomp_api;
        wil::com_ptr<IDXGIOutput> primary_output;
        FrameTimeAverager frame_time_averager;
        FrameTimeMinimum vblank_time_minimum;

        if (!dcomp_api.has_wait_for_composition_clock() && mmh::is_windows_8_or_newer()) {
            try {
                const auto dxgi_factory = dxgi::create_dxgi_factory();
                primary_output = dxgi::get_primary_output(dxgi_factory);
            } catch (const wil::ResultException&) {
#ifdef _DEBUG
                LOG_CAUGHT_EXCEPTION();
#endif
            }
        }

        auto last_vblank_time_point = std::chrono::steady_clock::now() - 16ms;

        while (!stop_token.stop_requested()) {
            double vblank_time_ms{};

            const auto wait_for_vblank = [&] {
                if (dcomp_api.has_wait_for_composition_clock()) {
                    const auto event = m_shutdown_event.get();
                    [[maybe_unused]] const auto dcomp_status
                        = dcomp_api.wait_for_composition_clock(m_shutdown_event ? 1 : 0, &event, 50);

#ifdef _DEBUG
                    LOG_IF_NTSTATUS_FAILED(dcomp_status);
#endif
                } else if (primary_output) {
                    [[maybe_unused]] const auto hr = primary_output->WaitForVBlank();
#ifdef _DEBUG
                    LOG_IF_FAILED(hr);
#endif
                } else {
                    [[maybe_unused]] const auto hr = DwmFlush();
#ifdef _DEBUG
                    LOG_IF_FAILED(hr);
#endif
                }

                const auto vblank_time_point = std::chrono::steady_clock::now();

                vblank_time_ms = (vblank_time_point - last_vblank_time_point) / 1.ms;

                if (vblank_time_ms > 1.)
                    vblank_time_minimum.add_frame_time(vblank_time_ms);

                last_vblank_time_point = vblank_time_point;
            };

            wait_for_vblank();

            // Possibly this could happen in scenarios like the display being off or RDP being in use
            const auto need_to_sleep = vblank_time_ms < 1.;

            if (stop_token.stop_requested())
                return;

            if (m_timer_active.load(std::memory_order_acquire)) {
                if (!need_to_sleep) {
                    // Throttle if the average frame time is more than 50% of minimum time between vblanks
                    const auto num_frames_to_skip = std::max(
                        0, static_cast<int>(frame_time_averager.get_average() * 2 / vblank_time_minimum.get_minimum()));

                    for (int index{}; index < num_frames_to_skip; ++index)
                        wait_for_vblank();
                }

                const auto frame_start = std::chrono::steady_clock::now();
                SendMessageTimeout(m_wnd, m_message_id, 0, 0, SMTO_BLOCK, 50, nullptr);
                const auto frame_time = (std::chrono::steady_clock::now() - frame_start) / 1.ms;
                frame_time_averager.add_frame(frame_time);
            }

            if (stop_token.stop_requested())
                return;

            // Will typically sleep for longer in practice (as the usual Windows timer resolution is 15.6ms)
            if (need_to_sleep)
                Sleep(15);
        }
    });
}

void SmoothScrollHelper::pause_timer_thread()
{
    m_timer_active.store(false, std::memory_order_release);
    start_thread_shutdown_timer();
}

void SmoothScrollHelper::stop_timer_thread()
{
    if (m_timer_thread) {
        m_timer_thread->request_stop();
        m_shutdown_event.SetEvent();
    }

    m_timer_thread.reset();
}

void SmoothScrollHelper::start_thread_shutdown_timer()
{
    if (m_shutdown_timer_active)
        stop_thread_shutdown_timer();

    SetTimer(m_wnd, m_timer_id, 2500, nullptr);
    m_shutdown_timer_active = true;
}

void SmoothScrollHelper::stop_thread_shutdown_timer()
{
    if (!m_shutdown_timer_active)
        return;

    m_shutdown_timer_active = false;
    KillTimer(m_wnd, m_timer_id);
}

} // namespace uih
