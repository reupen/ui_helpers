#pragma once

namespace uih {

struct ColouredTextSegment {
    COLORREF colour{};
    COLORREF selected_colour{};
    size_t start_character{};
    size_t character_count{};
};

struct ColourPair {
    COLORREF colour{};
    COLORREF selected_colour{};
};

std::optional<ColourPair> parse_colour_code(std::wstring_view text);
std::tuple<std::wstring, std::vector<ColouredTextSegment>> process_colour_codes(std::wstring_view);
std::string remove_colour_codes(std::string_view text);

} // namespace uih
