#pragma once

namespace uih {

using SubclassedWindowHandler
    = std::function<std::optional<LRESULT>(WNDPROC window_proc, HWND wnd, UINT msg, WPARAM wp, LPARAM lp)>;

void subclass_window(HWND wnd, SubclassedWindowHandler message_handler);

} // namespace uih
