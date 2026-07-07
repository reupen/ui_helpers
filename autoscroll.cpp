#include "stdafx.h"

#include "autoscroll.h"

namespace uih {

namespace {

void update_overlay_window(HWND overlay_wnd, POINT screen_pt, const AutoscrollHelper::OverlayImage& overlay_image)
{
    const int x = screen_pt.x - overlay_image.width / 2;
    const int y = screen_pt.y - overlay_image.height / 2;

    const auto desktop_dc = wil::GetDC(nullptr);
    wil::unique_hdc memory_dc(CreateCompatibleDC(desktop_dc.get()));
    const auto _ = wil::SelectObject(memory_dc.get(), overlay_image.bitmap.get());

    POINT source_pt{};
    SIZE size{overlay_image.width, overlay_image.height};
    POINT dest_pt{x, y};

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(overlay_wnd, desktop_dc.get(), &dest_pt, &size, memory_dc.get(), &source_pt, 0, &bf, ULW_ALPHA);
}

} // namespace

void AutoscrollHelper::start(HWND wnd, bool set_focus)
{
    if (m_active)
        return;

    m_can_scroll_horizontally = is_scrollable(wnd, ScrollAxis::Horizontal);
    m_can_scroll_vertically = is_scrollable(wnd, ScrollAxis::Vertical);

    if (!m_can_scroll_horizontally && !m_can_scroll_vertically)
        return;

    SetCapture(wnd);

    m_wnd = wnd;
    m_has_scrolled = false;
    m_active = true;
    m_previously_focused_wnd = set_focus ? SetFocus(wnd) : nullptr;

    // Note: WM_MBUTTONDOWN and WM_MBUTTONUP give the wrong pointer position if a popup menu was open
    const auto message_pos = GetMessagePos();
    m_start_screen_pt = {GET_X_LPARAM(message_pos), GET_Y_LPARAM(message_pos)};

    m_x_displacement = 0.f;
    m_y_displacement = 0.f;
    m_current_cursor_id = 0;
    m_last_tick_time = std::chrono::steady_clock::now();

    set_cursor();

    const auto overlay_image = create_overlay_image();

    if (overlay_image.bitmap) {
        ContainerWindowConfig window_config(L"ui_helpers_autoscroll_overlay_j9uNA-Ipjt4");
        window_config.window_ex_styles
            = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
        window_config.window_styles = 0;
        m_overlay_window.emplace(
            window_config, [](auto wnd, auto msg, auto wp, auto lp) { return DefWindowProc(wnd, msg, wp, lp); });
        m_overlay_window->create(GetAncestor(wnd, GA_ROOT), {m_start_screen_pt.x, m_start_screen_pt.y, 64, 64});

        update_overlay_window(m_overlay_window->get_wnd(), m_start_screen_pt, overlay_image);
        ShowWindow(m_overlay_window->get_wnd(), SW_SHOWNOACTIVATE);
    }

    m_timing_thread.start([this, wnd] { SendMessageTimeout(wnd, m_message_id, 0, 0, SMTO_BLOCK, 50, nullptr); });
}

std::optional<LRESULT> AutoscrollHelper::handle_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (!m_active)
        return {};

    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        stop();

        if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK)
            return 0;

        break;
    case WM_MBUTTONUP:
        if (m_has_scrolled)
            stop();

        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            stop();
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return 0;
    case WM_MOUSEMOVE: {
        POINT screen_pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(wnd, &screen_pt);
        m_x_displacement = static_cast<float>(screen_pt.x - m_start_screen_pt.x);
        m_y_displacement = static_cast<float>(screen_pt.y - m_start_screen_pt.y);
        set_cursor();
        break;
    }
    default:
        if (msg == m_message_id) {
            using namespace std::chrono_literals;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsed_time_multiplier = (now - m_last_tick_time) / 16.ms;
            m_last_tick_time = now;

            const auto [x_step, y_step] = get_scroll_velocity();
            const auto x_delta = static_cast<int>(x_step * elapsed_time_multiplier);
            const auto y_delta = static_cast<int>(y_step * elapsed_time_multiplier);

            if (x_delta != 0 || y_delta != 0) {
                m_has_scrolled = true;
                m_scroll_callback(x_delta, y_delta);
            }

            return 0;
        }
        break;
    }

    return {};
}

std::tuple<float, float> AutoscrollHelper::get_scroll_velocity() const
{
    const auto scaling_factor = static_cast<float>(get_system_dpi_cached().cx) / 96.f;
    const auto minimum_distance = 15.f * scaling_factor;

    const auto x_dead_zone_distance = abs(m_x_displacement) - minimum_distance;
    const auto y_dead_zone_distance = abs(m_y_displacement) - minimum_distance;

    const auto x_sign = std::copysign(1.f, m_x_displacement);
    const auto y_sign = std::copysign(1.f, m_y_displacement);

    const auto x_speed = m_can_scroll_horizontally && x_dead_zone_distance > 0
        ? x_sign * std::max(1.f, pow(x_dead_zone_distance, 1.75f) * .003f)
        : 0.f;

    const auto y_speed = m_can_scroll_vertically && y_dead_zone_distance > 0
        ? y_sign * std::max(1.f, pow(y_dead_zone_distance, 1.75f) * .003f)
        : 0.f;

    return {x_speed, y_speed};
}

void AutoscrollHelper::set_is_dark(bool is_dark)
{
    m_overlays.clear();
    m_is_dark = is_dark;

    if (!m_active || !m_overlay_window)
        return;

    const auto overlay_image = create_overlay_image();

    if (overlay_image.bitmap)
        update_overlay_window(m_overlay_window->get_wnd(), m_start_screen_pt, overlay_image);
}

void AutoscrollHelper::reset()
{
    stop();
    m_cursors.clear();
    m_overlays.clear();
    m_d2d_bitmap_renderer.reset();
}

HCURSOR AutoscrollHelper::get_hcursor(UINT resource_id)
{
    if (!m_cursors.contains(resource_id))
        m_cursors.insert_or_assign(resource_id,
            wil::unique_hcursor{static_cast<HCURSOR>(LoadImage(
                wil::GetModuleInstanceHandle(), MAKEINTRESOURCE(resource_id), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE))});

    return m_cursors.at(resource_id).get();
}

AutoscrollHelper::OverlayImage AutoscrollHelper::create_overlay_image()
{
    const auto cache_key = (m_can_scroll_horizontally ? 1 : 0) + (m_can_scroll_vertically ? 2 : 0);

    if (m_overlays.contains(cache_key))
        return OverlayImage{m_overlays.at(cache_key)};

    int win_10_pointer_size{32};
    SystemParametersInfo(SPI_GET_POINTER_SCALE, 0, &win_10_pointer_size, 0);
    const auto pointer_size = static_cast<float>(MulDiv(GetSystemMetrics(SM_CYCURSOR), win_10_pointer_size, 32));

    if (!m_d2d_bitmap_renderer)
        m_d2d_bitmap_renderer.emplace(96.f, pointer_size, pointer_size);

    try {
        auto rendered_image = m_d2d_bitmap_renderer->render([&](const wil::com_ptr<ID2D1DeviceContext>& device_context,
                                                                const auto& factory) {
            const auto make_brush = [&](const D2D1_COLOR_F& colour) {
                wil::com_ptr<ID2D1SolidColorBrush> brush;
                THROW_IF_FAILED(device_context->CreateSolidColorBrush(colour, &brush));
                return brush;
            };

            const auto centre = D2D1::Point2F(pointer_size * .5f, pointer_size * .5f);
            const auto chevron_top_midpoint = D2D1::Point2F(pointer_size * .5f, pointer_size * .16f);
            const auto chevron_width = pointer_size * .312f;
            const auto chevron_height = pointer_size * .147f;
            const auto chevron_thickness = pointer_size * .085f;
            const auto circle_radius = pointer_size * .055f;

            const auto make_chevron_geometry = [&, top_midpoint{chevron_top_midpoint}, width{chevron_width},
                                                   height{chevron_height}, thickness{chevron_thickness}](
                                                   const D2D1::Matrix3x2F& transform = D2D1::Matrix3x2F::Identity()) {
                wil::com_ptr<ID2D1PathGeometry> path_geometry;
                THROW_IF_FAILED(factory->CreatePathGeometry(&path_geometry));

                wil::com_ptr<ID2D1GeometrySink> geometry_sink;
                THROW_IF_FAILED(path_geometry->Open(&geometry_sink));

                const auto height_signed_thickness = std::copysign(thickness, height);

                geometry_sink->BeginFigure(top_midpoint * transform, D2D1_FIGURE_BEGIN_FILLED);
                geometry_sink->AddLine(D2D1::Point2F(top_midpoint.x + width / 2, top_midpoint.y + height) * transform);
                geometry_sink->AddLine(D2D1::Point2F(top_midpoint.x + width / 2 - thickness / 2,
                                           top_midpoint.y + height + height_signed_thickness / 2)
                    * transform);
                geometry_sink->AddLine(
                    D2D1::Point2F(top_midpoint.x, top_midpoint.y + height_signed_thickness) * transform);
                geometry_sink->AddLine(D2D1::Point2F(top_midpoint.x - width / 2 + thickness / 2,
                                           top_midpoint.y + height + height_signed_thickness / 2)
                    * transform);
                geometry_sink->AddLine(D2D1::Point2F(top_midpoint.x - width / 2, top_midpoint.y + height) * transform);
                geometry_sink->EndFigure(D2D1_FIGURE_END_CLOSED);

                THROW_IF_FAILED(geometry_sink->Close());

                return path_geometry;
            };

            std::vector<wil::com_ptr<ID2D1PathGeometry>> triangle_geometries;

            if (m_can_scroll_vertically) {
                triangle_geometries.push_back(make_chevron_geometry());
                triangle_geometries.push_back(make_chevron_geometry(D2D1::Matrix3x2F::Rotation(180, centre)));
            }

            if (m_can_scroll_horizontally) {
                triangle_geometries.push_back(make_chevron_geometry(D2D1::Matrix3x2F::Rotation(90, centre)));
                triangle_geometries.push_back(make_chevron_geometry(D2D1::Matrix3x2F::Rotation(270, centre)));
            }

            device_context->BeginDraw();
            device_context->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));

            const auto colour = m_is_dark ? D2D1::ColorF(.6f, .6f, .6f, 0.9f) : D2D1::ColorF(.45f, .45f, .45f, 0.9f);
            const auto grey_brush = make_brush(colour);
            device_context->FillEllipse(D2D1::Ellipse(centre, circle_radius, circle_radius), grey_brush.get());

            for (const auto& geometry : triangle_geometries)
                device_context->FillGeometry(geometry.get(), grey_brush.get());

            THROW_IF_FAILED(device_context->EndDraw());
        });

        OverlayImage overlay_image(std::move(rendered_image.bitmap), rendered_image.width, rendered_image.height);
        m_overlays.insert_or_assign(cache_key, overlay_image);
        return overlay_image;
    }
    CATCH_LOG()

    return {};
}

void AutoscrollHelper::set_cursor()
{
    const auto [x_speed, y_speed] = get_scroll_velocity();

    const auto cursor_id = [&] {
        if (x_speed == 0 && y_speed == 0) {
            if (m_can_scroll_horizontally && m_can_scroll_vertically)
                return m_cursor_info.initial_vertical_horizontal;

            if (m_can_scroll_horizontally)
                return m_cursor_info.initial_horizontal;

            return m_cursor_info.initial_vertical;
        }

        if (x_speed == 0)
            return y_speed < 0 ? m_cursor_info.scroll_up : m_cursor_info.scroll_down;

        if (y_speed == 0)
            return x_speed < 0 ? m_cursor_info.scroll_left : m_cursor_info.scroll_right;

        if (y_speed < 0)
            return x_speed < 0 ? m_cursor_info.scroll_up_left : m_cursor_info.scroll_up_right;

        return x_speed < 0 ? m_cursor_info.scroll_down_left : m_cursor_info.scroll_down_right;
    }();

    if (cursor_id == m_current_cursor_id)
        return;

    SetCursor(get_hcursor(cursor_id));
    m_current_cursor_id = cursor_id;
}

void AutoscrollHelper::stop()
{
    if (!m_active)
        return;

    if (GetCapture() == m_wnd)
        ReleaseCapture();

    m_timing_thread.stop();
    m_active = false;

    if (m_overlay_window) {
        m_overlay_window->destroy();
        m_overlay_window.reset();
    }

    if (m_previously_focused_wnd)
        SetFocus(IsWindow(m_previously_focused_wnd) ? m_previously_focused_wnd : GetAncestor(m_wnd, GA_ROOT));
}

} // namespace uih
