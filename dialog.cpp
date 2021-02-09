#include "stdafx.h"

namespace uih {

namespace {
struct DialogBoxData {
    std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message;
    std::shared_ptr<DialogBoxData> self;
};

BOOL WINAPI on_dialog_box_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    DialogBoxData* data{};
    if (msg == WM_INITDIALOG) {
        data = reinterpret_cast<DialogBoxData*>(lp);
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
    const auto data = std ::make_unique<DialogBoxData>(DialogBoxData{std::move(on_message), {}});

    return DialogBoxParam(mmh::get_current_instance(), MAKEINTRESOURCE(resource_id), wnd, on_dialog_box_message,
        reinterpret_cast<LPARAM>(data.get()));
}

HWND modeless_dialog_box(
    UINT resource_id, HWND wnd, std::function<BOOL(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)> on_message)
{
    std::shared_ptr<DialogBoxData> data = std ::make_shared<DialogBoxData>(DialogBoxData{std::move(on_message), {}});
    data->self = data;

    return CreateDialogParam(mmh::get_current_instance(), MAKEINTRESOURCE(resource_id), wnd, on_dialog_box_message,
        reinterpret_cast<LPARAM>(data.get()));
}

} // namespace uih
