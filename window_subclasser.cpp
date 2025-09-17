#include "stdafx.h"

namespace uih {

namespace {

struct WindowState {
    WNDPROC wnd_proc{};
    std::vector<SubclassedWindowHandler> message_handlers;
};

std::unordered_map<HWND, WindowState> state_map;

LRESULT __stdcall handle_subclassed_window_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) noexcept
{
    const auto& state = state_map.at(wnd);
    const auto wnd_proc = state.wnd_proc;

    for (auto&& message_handler : state.message_handlers)
        if (const auto result = message_handler(wnd_proc, wnd, msg, wp, lp))
            return *result;

    if (msg == WM_NCDESTROY)
        state_map.erase(wnd);

    return CallWindowProcW(wnd_proc, wnd, msg, wp, lp);
}

} // namespace

void subclass_window(HWND wnd, SubclassedWindowHandler message_handler)
{
    if (const auto iter = state_map.find(wnd); iter != state_map.end()) {
        iter->second.message_handlers.push_back(std::move(message_handler));
        return;
    }

    const auto wnd_proc = SubclassWindow(wnd, handle_subclassed_window_message);
    auto state = WindowState{wnd_proc, {std::move(message_handler)}};
    state_map.emplace(wnd, std::move(state));
}

} // namespace uih
