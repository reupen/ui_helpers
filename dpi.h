#pragma once

namespace uih::dpi {

using SetThreadDpiAwarenessContextProc = DPI_AWARENESS_CONTEXT(__stdcall*)(DPI_AWARENESS_CONTEXT);

class SetThreadDpiAwarenessContextHandle {
public:
    SetThreadDpiAwarenessContextHandle() = default;

    SetThreadDpiAwarenessContextHandle(SetThreadDpiAwarenessContextHandle&) = delete;
    SetThreadDpiAwarenessContextHandle& operator=(SetThreadDpiAwarenessContextHandle&) = delete;

    SetThreadDpiAwarenessContextHandle(SetThreadDpiAwarenessContextHandle&&) = default;
    SetThreadDpiAwarenessContextHandle& operator=(SetThreadDpiAwarenessContextHandle&&) = default;

    SetThreadDpiAwarenessContextHandle(wil::unique_hmodule&& user32,
        SetThreadDpiAwarenessContextProc set_thread_dpi_awareness_context, DPI_AWARENESS_CONTEXT previous_awareness)
        : m_user32(std::move(user32))
        , m_set_thread_dpi_awareness_context(set_thread_dpi_awareness_context)
        , m_previous_awareness(previous_awareness)
    {
    }

    ~SetThreadDpiAwarenessContextHandle()
    {
        if (m_set_thread_dpi_awareness_context)
            m_set_thread_dpi_awareness_context(m_previous_awareness);
    }

private:
    wil::unique_hmodule m_user32;
    SetThreadDpiAwarenessContextProc m_set_thread_dpi_awareness_context{};
    DPI_AWARENESS_CONTEXT m_previous_awareness{};
};

BOOL system_parameters_info_for_dpi(unsigned action, unsigned param, void* data, unsigned dpi);

[[nodiscard]] int get_system_metrics_for_dpi(int index, unsigned dpi);

[[nodiscard]] unsigned get_dpi_for_window(HWND wnd);

[[nodiscard]] std::unique_ptr<SetThreadDpiAwarenessContextHandle> set_thread_per_monitor_dpi_aware();

/*
 * Per monitor DPI-aware version of DialogBox() (for modal dialogs).
 * Making DialogBox() per-monitor DPI-aware is more complicated as DialogBox() blocks and has its own message
 * loop, while the DPI-awareness context needs to be reset straight away.
 */
INT_PTR modal_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message);

/**
 * Per monitor DPI-aware version of CreateDialog() (for modeless dialogs).
 */
HWND modeless_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message);

} // namespace uih::dpi
