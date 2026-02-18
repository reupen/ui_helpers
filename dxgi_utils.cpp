#include "stdafx.h"

#include "dxgi_utils.h"

namespace uih::dxgi {

namespace {

int compute_intersection_area(RECT left, RECT right)
{
    return std::max(0l, std::min(left.right, right.right) - std::max(left.left, right.left))
        * std::max(0l, std::min(left.bottom, right.bottom) - std::max(left.top, right.top));
}

} // namespace

wil::com_ptr<IDXGIFactory2> create_dxgi_factory()
{
    wil::com_ptr<IDXGIFactory2> dxgi_factory;
    THROW_IF_FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), dxgi_factory.put_void()));
    return dxgi_factory;
}

wil::com_ptr<IDXGIOutput> find_output_by_wnd(const wil::com_ptr<IDXGIFactory1>& dxgi_factory, HWND wnd)
{
    wil::com_ptr<IDXGIOutput> best_output;
    int best_intersect_area{-1};

    RECT window_rect{};
    THROW_IF_WIN32_BOOL_FALSE(GetWindowRect(wnd, &window_rect));

    uint32_t adapter_index{};
    wil::com_ptr<IDXGIAdapter1> adapter;

    while (dxgi_factory->EnumAdapters1(adapter_index, &adapter) == S_OK) {
        uint32_t output_index{};
        wil::com_ptr<IDXGIOutput> current_output;

        while (adapter->EnumOutputs(output_index, &current_output) == S_OK) {
            DXGI_OUTPUT_DESC desc{};
            THROW_IF_FAILED(current_output->GetDesc(&desc));

            const RECT desktop_rect = desc.DesktopCoordinates;
            const auto intersection_area = compute_intersection_area(window_rect, desktop_rect);

            if (intersection_area == wil::rect_width(window_rect) * wil::rect_height(window_rect))
                return current_output;

            if (!best_output || intersection_area > best_intersect_area) {
                best_output = current_output;
                best_intersect_area = intersection_area;
            }

            ++output_index;
        }

        ++adapter_index;
    }

    return best_output;
}

wil::com_ptr<IDXGIOutput> get_primary_output(const wil::com_ptr<IDXGIFactory1>& dxgi_factory)
{
    wil::com_ptr<IDXGIAdapter1> adapter;
    THROW_IF_FAILED(dxgi_factory->EnumAdapters1(0, &adapter));

    wil::com_ptr<IDXGIOutput> output;
    THROW_IF_FAILED(adapter->EnumOutputs(0, &output));

    DXGI_OUTPUT_DESC desc{};
    THROW_IF_FAILED(output->GetDesc(&desc));

    return output;
}

} // namespace uih::dxgi
