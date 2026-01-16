#pragma once

#include "direct_write.h"
#include "text_format_parser.h"

namespace uih::text_style {

struct ColourPair {
    COLORREF colour{};
    COLORREF selected_colour{};
};

struct ColourSegment {
    COLORREF colour{};
    COLORREF selected_colour{};
    size_t start_character{};
    size_t character_count{};
};

struct FontSegment {
    FormatProperties font;
    size_t start_character{};
    size_t character_count{};
};

std::optional<ColourPair> parse_colour_code(std::wstring_view text);

std::tuple<std::wstring, std::vector<ColourSegment>, std::vector<FontSegment>> process_colour_and_font_codes(
    std::wstring_view text, const FormatProperties& initial_format_properties = {},
    const std::function<void(std::wstring)>& print_legacy_feedback = {},
    const direct_write::Context::Ptr& direct_write_context = {});

std::string remove_colour_and_font_codes(std::string_view text);

} // namespace uih::text_style
