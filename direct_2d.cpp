#include "stdafx.h"

#include "direct_2d.h"

namespace uih::d2d {

namespace {

float scale_channel(uint8_t value)
{
    return gsl::narrow_cast<float>(value) / 255.0f;
}

std::weak_ptr<wil::com_ptr<ID2D1Factory1>> weak_main_thread_factory;

} // namespace

wil::com_ptr<ID2D1Factory1> create_factory(D2D1_FACTORY_TYPE factory_type)
{
    wil::com_ptr<ID2D1Factory1> factory;
    D2D1_FACTORY_OPTIONS options{};

#if UIH_ENABLE_D3D_D2D_DEBUG_LAYER == 1
    options.debugLevel = IsDebuggerPresent() ? D2D1_DEBUG_LEVEL_INFORMATION : D2D1_DEBUG_LEVEL_NONE;
#endif

    THROW_IF_FAILED(D2D1CreateFactory(factory_type, options, &factory));

    return factory;
}

MainThreadD2D1Factory create_main_thread_factory()
{
    if (const auto factory = weak_main_thread_factory.lock())
        return factory;

    const auto factory
        = std::make_shared<wil::com_ptr<ID2D1Factory1>>(create_factory(D2D1_FACTORY_TYPE_SINGLE_THREADED));
    weak_main_thread_factory = factory;
    return factory;
}

D2D1_COLOR_F colorref_to_d2d_color(COLORREF colour)
{
    return D2D1::ColorF(
        scale_channel(GetRValue(colour)), scale_channel(GetGValue(colour)), scale_channel(GetBValue(colour)));
}

} // namespace uih::d2d
