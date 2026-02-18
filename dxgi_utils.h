#pragma once

namespace uih::dxgi {

wil::com_ptr<IDXGIFactory2> create_dxgi_factory();
wil::com_ptr<IDXGIOutput> find_output_by_wnd(const wil::com_ptr<IDXGIFactory1>& dxgi_factory, HWND wnd);
wil::com_ptr<IDXGIOutput> get_primary_output(const wil::com_ptr<IDXGIFactory1>& dxgi_factory);

} // namespace uih::dxgi
