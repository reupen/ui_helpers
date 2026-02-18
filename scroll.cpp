#include "stdafx.h"

#include "scroll.h"

#include "dcomp_utils.h"
#include "dxgi_utils.h"

using namespace std::chrono_literals;

namespace uih {

void SmoothScrollHelper::absolute_scroll(ScrollAxis axis, int target_position, Duration duration)
{
    auto& state = axis_state(axis);
    const auto current_position = state.current_position();
    const auto now = std::chrono::high_resolution_clock::now();
    update_state(axis, target_position - current_position, false, duration, now);

    const auto has_exiting_timer = m_timer_thread.has_value();
    start_timer_thread();

    if (!has_exiting_timer)
        on_message();
}

void SmoothScrollHelper::delta_scroll(ScrollAxis axis, int delta, Duration duration)
{
    const auto now = std::chrono::high_resolution_clock::now();
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

    const auto now = std::chrono::high_resolution_clock::now();
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
    std::chrono::time_point<std::chrono::high_resolution_clock> now)
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

    m_timer_thread = std::jthread([this](std::stop_token stop_token) {
        mmh::set_thread_description(GetCurrentThread(), thread_name.c_str());

        dcomp::DcompApi dcomp_api;
        wil::com_ptr<IDXGIOutput> primary_output;

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

        std::optional<MMRESULT> time_begin_period_result;

        auto _ = wil::scope_exit([&] {
            if (time_begin_period_result == TIMERR_NOERROR)
                timeEndPeriod(1);
        });

        auto last_vblank_time_point = std::chrono::high_resolution_clock::now() - 16ms;

        while (!stop_token.stop_requested()) {
            const auto second_last_vblank_time_point = last_vblank_time_point;

            if (dcomp_api.has_wait_for_composition_clock()) {
                [[maybe_unused]] const auto dcomp_status = dcomp_api.wait_for_composition_clock(0, nullptr, 50);
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

            if (stop_token.stop_requested())
                return;

            const auto vblank_time_point = std::chrono::high_resolution_clock::now();
            const auto vblank_gap_ms = (vblank_time_point - last_vblank_time_point) / 1ms;
            last_vblank_time_point = vblank_time_point;

            if (m_timer_active.load(std::memory_order_acquire))
                SendMessageTimeout(m_wnd, m_message_id, 0, 0, SMTO_BLOCK, 50, nullptr);

            if (stop_token.stop_requested())
                return;

            if (vblank_gap_ms >= 10)
                continue;

            if (!time_begin_period_result)
                time_begin_period_result = timeBeginPeriod(1);

            auto now = std::chrono::high_resolution_clock::now();
            const auto time_since_second_last_vblank
                = gsl::narrow_cast<uint32_t>(std::clamp((now - second_last_vblank_time_point) / 1ms, 0ll, 16ll));

            Sleep(16 - time_since_second_last_vblank);
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
