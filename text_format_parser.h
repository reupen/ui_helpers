#pragma once

namespace uih::text_style {

struct InitialPropertyValue {};

enum class TextDecorationType : int8_t {
    None,
    Underline,
};

struct FormatProperties {
    std::optional<std::variant<std::wstring, InitialPropertyValue>> font_family;
    std::optional<std::variant<float, InitialPropertyValue>> font_size;
    std::optional<std::variant<DWRITE_FONT_WEIGHT, InitialPropertyValue>> font_weight;
    std::optional<std::variant<DWRITE_FONT_STRETCH, float, InitialPropertyValue>> font_stretch;
    std::optional<std::variant<DWRITE_FONT_STYLE, InitialPropertyValue>> font_style;
    std::optional<std::variant<TextDecorationType, InitialPropertyValue>> text_decoration;
};

#if UIH_HAS_LEXY
std::optional<FormatProperties> parse_format_properties(std::wstring_view input);
#endif

} // namespace uih::text_style
