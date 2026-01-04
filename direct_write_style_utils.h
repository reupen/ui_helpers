#pragma once

#include "text_style.h"

namespace uih::direct_write {

void set_layout_font_segments(TextLayout& layout, std::span<const text_style::FontSegment> font_segments);

}
