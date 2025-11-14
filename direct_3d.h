#pragma once

namespace uih::d3d {

wil::com_ptr<ID3D11Device> create_d3d_device(D3D_DRIVER_TYPE driver_type,
    std::span<const D3D_FEATURE_LEVEL> feature_levels, ID3D11DeviceContext** device_context = nullptr);

}
