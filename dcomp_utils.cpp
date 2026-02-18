#include "stdafx.h"

#include "dcomp_utils.h"

namespace uih::dcomp {

bool DcompApi::has_wait_for_composition_clock()
{
    initialise();
    return *m_dcomposition_wait_for_composition_clock;
}

DWORD DcompApi::wait_for_composition_clock(UINT count, const HANDLE* handles, DWORD timeout_in_ms)
{
    if (!has_wait_for_composition_clock())
        return E_NOTIMPL;

    return (*m_dcomposition_wait_for_composition_clock)(count, handles, timeout_in_ms);
}

void DcompApi::initialise()
{
    if (!m_dcomp_library)
        m_dcomp_library.emplace(LoadLibraryExW(L"dcomp.dll", nullptr, LOAD_LIBRARY_SAFE_CURRENT_DIRS));

    if (!*m_dcomp_library)
        return;

    if (!m_dcomposition_wait_for_composition_clock)
        m_dcomposition_wait_for_composition_clock
            = GetProcAddressByFunctionDeclaration(m_dcomp_library->get(), DCompositionWaitForCompositorClock);
}

} // namespace uih::dcomp
