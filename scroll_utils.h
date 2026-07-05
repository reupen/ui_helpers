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
int clamp_scroll_position(HWND wnd, ScrollAxis axis, int position);
bool is_scrollable(HWND wnd, ScrollAxis axis);

} // namespace uih
