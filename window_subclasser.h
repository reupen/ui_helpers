#pragma once

namespace uih {

void subclass_window(
    HWND wnd, std::function<std::optional<LRESULT>(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> message_handler);

}
