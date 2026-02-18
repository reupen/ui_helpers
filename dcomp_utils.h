#pragma once

#include "dcomp.h"

namespace uih::dcomp {

class DcompApi {
public:
    bool has_wait_for_composition_clock();
    DWORD wait_for_composition_clock(UINT count, const HANDLE* handles, DWORD timeout_in_ms);

private:
    void initialise();

    std::optional<wil::unique_hmodule> m_dcomp_library;
    std::optional<decltype(GetProcAddressByFunctionDeclaration(
        std::declval<HMODULE>(), DCompositionWaitForCompositorClock))>
        m_dcomposition_wait_for_composition_clock;
};

} // namespace uih::dcomp
