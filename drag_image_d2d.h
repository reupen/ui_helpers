#pragma once

#include "direct_write.h"

namespace uih {

class D2DDragImageCreator {
public:
    D2DDragImageCreator(float dpi);
    ~D2DDragImageCreator();

    bool create_drag_image(
        HWND wnd, bool is_dark, direct_write::Context& context, std::wstring_view text, LPSHDRAGIMAGE lpsdi) const;

private:
    class D2DDragImageCreatorImpl;

    std::unique_ptr<D2DDragImageCreatorImpl> m_impl;
};

} // namespace uih
