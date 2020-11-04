#include "stdafx.h"

namespace uih::dpi {

BOOL system_parameters_info_for_dpi(unsigned action, unsigned param, void* data, unsigned dpi)
{
    using SystemParametersInfoForDpiProc = BOOL(__stdcall*)(UINT, UINT, PVOID, UINT, UINT);

    const wil::unique_hmodule user32(THROW_LAST_ERROR_IF_NULL(LoadLibrary(L"user32.dll")));

    const auto system_parameters_info_for_dpi_proc
        = reinterpret_cast<SystemParametersInfoForDpiProc>(GetProcAddress(user32.get(), "SystemParametersInfoForDpi"));

    if (!system_parameters_info_for_dpi_proc)
        return SystemParametersInfo(action, param, data, 0);

    return system_parameters_info_for_dpi_proc(action, param, data, 0, dpi);
}

[[nodiscard]] int get_system_metrics_for_dpi(int index, unsigned dpi)
{
    using GetSystemMetricsForDpiProc = int(__stdcall*)(int, UINT);

    const wil::unique_hmodule user32(THROW_LAST_ERROR_IF_NULL(LoadLibrary(L"user32.dll")));

    const auto get_system_metrics_for_dpi_proc
        = reinterpret_cast<GetSystemMetricsForDpiProc>(GetProcAddress(user32.get(), "GetSystemMetricsForDpi"));

    if (!get_system_metrics_for_dpi_proc)
        return GetSystemMetrics(index);

    return get_system_metrics_for_dpi_proc(index, dpi);
}

[[nodiscard]] unsigned get_dpi_for_window(HWND wnd)
{
    using GetDpiForWindowProc = UINT(__stdcall*)(HWND);

    const wil::unique_hmodule user32(THROW_LAST_ERROR_IF_NULL(LoadLibrary(L"user32.dll")));

    const auto get_dpi_for_window_proc
        = reinterpret_cast<GetDpiForWindowProc>(GetProcAddress(user32.get(), "GetDpiForWindow"));

    if (!get_dpi_for_window_proc)
        return get_system_dpi_cached().cx;

    return get_dpi_for_window_proc(wnd);
}

[[nodiscard]] std::unique_ptr<SetThreadDpiAwarenessContextHandle> set_thread_per_monitor_dpi_aware()
{
    wil::unique_hmodule user32(THROW_LAST_ERROR_IF_NULL(LoadLibrary(L"user32.dll")));

    const auto set_thread_dpi_awareness_context_proc = reinterpret_cast<SetThreadDpiAwarenessContextProc>(
        GetProcAddress(user32.get(), "SetThreadDpiAwarenessContext"));

    if (!set_thread_dpi_awareness_context_proc)
        return {};

    const auto previous_awareness = set_thread_dpi_awareness_context_proc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    return std::make_unique<SetThreadDpiAwarenessContextHandle>(
        std::move(user32), set_thread_dpi_awareness_context_proc, previous_awareness);
}

namespace {
struct DialogBoxData {
    std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message;
    std::unique_ptr<SetThreadDpiAwarenessContextHandle> dpi_awareness_context;
    std::shared_ptr<DialogBoxData> self;
};

BOOL WINAPI on_dialog_box_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DialogBoxData* data{};
    if (msg == WM_INITDIALOG) {
        data = reinterpret_cast<DialogBoxData*>(lp);
        data->dpi_awareness_context.reset();
        SetWindowLongPtr(wnd, DWLP_USER, lp);
    } else {
        data = reinterpret_cast<DialogBoxData*>(GetWindowLongPtr(wnd, DWLP_USER));
    }

    const BOOL result = data ? data->on_message(wnd, msg, wp, lp) : FALSE;

    if (msg == WM_NCDESTROY) {
        assert(data);

        if (data)
            data->self.reset();

        SetWindowLongPtr(wnd, DWLP_USER, NULL);
    }

    return result;
}

} // namespace

INT_PTR modal_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message)
{
    const auto data = std ::make_unique<DialogBoxData>(
        DialogBoxData{std::move(on_message), set_thread_per_monitor_dpi_aware(), {}});

    return DialogBoxParam(mmh::get_current_instance(), MAKEINTRESOURCE(resource_id), wnd, on_dialog_box_message,
        reinterpret_cast<LPARAM>(data.get()));
}

HWND modeless_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message)
{
    std::shared_ptr<DialogBoxData> data = std ::make_shared<DialogBoxData>(
        DialogBoxData{std::move(on_message), set_thread_per_monitor_dpi_aware(), {}});
    data->self = data;

    return CreateDialogParam(mmh::get_current_instance(), MAKEINTRESOURCE(resource_id), wnd, on_dialog_box_message,
        reinterpret_cast<LPARAM>(data.get()));
}
} // namespace uih::dpi
