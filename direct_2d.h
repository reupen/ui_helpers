#pragma once

namespace uih::d2d {

wil::com_ptr<ID2D1Factory1> create_factory(D2D1_FACTORY_TYPE factory_type);

using MainThreadD2D1Factory = std::shared_ptr<wil::com_ptr<ID2D1Factory1>>;
MainThreadD2D1Factory create_main_thread_factory();

D2D1_COLOR_F colorref_to_d2d_color(COLORREF colour);

constexpr bool is_device_reset_error(const HRESULT hr)
{
    return hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

} // namespace uih::d2d
