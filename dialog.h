#pragma once

namespace uih {

INT_PTR modal_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message);

HWND modeless_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message);

} // namespace uih
