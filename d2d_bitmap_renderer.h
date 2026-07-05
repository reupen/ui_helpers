#pragma once

#include "direct_2d.h"

namespace uih {

struct SizedHbitmap {
    wil::unique_hbitmap bitmap;
    long width{};
    long height{};
};

class D2DBitmapRenderer {
public:
    D2DBitmapRenderer(float dpi, float width_dip, float height_dip, bool use_straight_alpha = false)
        : m_dpi{dpi}
        , m_width_dip(width_dip)
        , m_height_dip(height_dip)
        , m_use_straight_alpha(use_straight_alpha)
    {
    }

    SizedHbitmap render(const std::function<void(const wil::com_ptr<ID2D1DeviceContext>& device_context,
            const wil::com_ptr<ID2D1Factory1>& factory)>& render_callback);

private:
    SizedHbitmap try_create_hbitmap() const;

    void create_device();
    void create_target() const;
    void clear_resources();

    int32_t width_px() const { return std::lround(m_width_dip * m_dpi / 96.f); }
    int32_t height_px() const { return std::lround(m_height_dip * m_dpi / 96.f); }
    uint32_t width_upx() const { return gsl::narrow<uint32_t>(width_px()); }
    uint32_t height_upx() const { return gsl::narrow<uint32_t>(height_px()); }

    wil::com_ptr<ID3D11Device> m_d3d_device;
    d2d::MainThreadD2D1Factory m_d2d_factory;
    wil::com_ptr<ID2D1Device> m_d2d_device;
    wil::com_ptr<ID2D1DeviceContext> m_d2d_device_context;
    float m_dpi{};
    float m_width_dip{};
    float m_height_dip{};
    bool m_use_straight_alpha;
};

} // namespace uih
