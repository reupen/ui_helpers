#pragma once

#include <icu.h>

namespace uih::icu {

class ICUHandle {
public:
    using Ptr = std::shared_ptr<ICUHandle>;

    static std::shared_ptr<ICUHandle> s_create()
    {
        if (s_has_library && !*s_has_library)
            return {};

        auto ptr = s_ptr.lock();

        if (!ptr) {
            wil::unique_hmodule icu_module{LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)};
            s_has_library = icu_module.is_valid();

            if (!icu_module)
                return {};

            const auto u_hasBinaryProperty_ptr
                = GetProcAddressByFunctionDeclaration(icu_module.get(), u_hasBinaryProperty);

            if (!u_hasBinaryProperty_ptr) {
                s_has_library = false;
                return {};
            }

            ptr = std::make_shared<ICUHandle>(std::move(icu_module), u_hasBinaryProperty_ptr);
            s_ptr = ptr;
        }

        return ptr;
    }

    ICUHandle(wil::unique_hmodule icu_module, decltype(u_hasBinaryProperty)* u_hasBinaryProperty_ptr)
        : m_icu_module(std::move(icu_module))
        , m_u_hasBinaryProperty_ptr(u_hasBinaryProperty_ptr)
    {
    }

    UBool u_has_binary_property(UChar32 c, UProperty which) const { return m_u_hasBinaryProperty_ptr(c, which); }

private:
    inline static std::optional<bool> s_has_library{};
    inline static std::weak_ptr<ICUHandle> s_ptr;

    wil::unique_hmodule m_icu_module;
    decltype(u_hasBinaryProperty)* m_u_hasBinaryProperty_ptr{};
};

} // namespace uih::icu
