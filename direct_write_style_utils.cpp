#include "stdafx.h"

#include "direct_write.h"
#include "direct_write_style_utils.h"

#include "text_style.h"

namespace uih::direct_write {

namespace {

template <class Value, typename Member>
void process_simple_style_property(const std::span<const text_style::FontSegment>& segments, Member member,
    std::function<void(const Value&, DWRITE_TEXT_RANGE text_range)> apply_to_layout)
{
    struct CurrentValue {
        Value value;
        size_t start{};
        size_t count{};
    };

    std::optional<CurrentValue> current_value;

    const auto apply_current_value = [&] {
        if (!current_value)
            return;

        const auto text_range = DWRITE_TEXT_RANGE{
            gsl::narrow<uint32_t>(current_value->start), gsl::narrow<uint32_t>(current_value->count)};

        try {
            apply_to_layout(current_value->value, text_range);
        }
        CATCH_LOG();

        current_value.reset();
    };

    for (auto& [properties, start_character, character_count] : segments) {
        const auto& value = properties.*member;

        if (!value) {
            apply_current_value();
            continue;
        }

        const auto& segment_value = std::get<Value>(*value);

        if (current_value
            && (current_value->value != segment_value
                || (current_value->start + current_value->count < start_character))) {
            apply_current_value();
        }

        if (current_value) {
            current_value->count += character_count;
        } else {
            current_value = {segment_value, start_character, character_count};
        }
    }

    apply_current_value();
}

void process_wss_style_property(const std::span<const text_style::FontSegment>& segments, const TextLayout& text_layout)
{
    struct CurrentValue {
        DWRITE_FONT_WEIGHT weight{};
        std::variant<DWRITE_FONT_STRETCH, float> stretch{};
        DWRITE_FONT_STYLE style{};
        size_t start{};
        size_t count{};
    };

    const auto initial_weight = text_layout.get_weight();
    const auto initial_stretch = text_layout.get_stretch();
    const auto initial_style = text_layout.get_style();

    std::optional<CurrentValue> current_value;

    const auto apply_current_value = [&] {
        if (!current_value)
            return;

        const auto text_range = DWRITE_TEXT_RANGE{
            gsl::narrow<uint32_t>(current_value->start), gsl::narrow<uint32_t>(current_value->count)};

        const auto weight = current_value->weight;
        const auto stretch = current_value->stretch;
        const auto style = current_value->style;

        if (weight != initial_weight || stretch != initial_stretch || style != initial_style) {
            try {
                text_layout.set_wss(weight, stretch, style, text_range);
            }
            CATCH_LOG();
        }

        current_value.reset();
    };

    for (auto& [properties, start_character, character_count] : segments) {
        std::optional<DWRITE_FONT_WEIGHT> weight;
        std::optional<std::variant<DWRITE_FONT_STRETCH, float>> stretch;
        std::optional<DWRITE_FONT_STYLE> style;

        if (properties.font_weight)
            weight = std::get<DWRITE_FONT_WEIGHT>(*properties.font_weight);

        if (properties.font_stretch) {
            if (std::holds_alternative<float>(*properties.font_stretch))
                stretch = std::get<float>(*properties.font_stretch);
            else
                stretch = std::get<DWRITE_FONT_STRETCH>(*properties.font_stretch);
        }

        if (properties.font_style)
            style = std::get<DWRITE_FONT_STYLE>(*properties.font_style);

        if (!(weight || stretch || style)) {
            apply_current_value();
            continue;
        }

        if (current_value) {
            if (current_value->start + current_value->count < start_character)
                apply_current_value();
            else if (current_value->weight != weight.value_or(initial_weight)
                || current_value->stretch != stretch.value_or(initial_stretch)
                || current_value->style != style.value_or(initial_style))
                apply_current_value();
        }

        if (current_value) {
            current_value->count += character_count;
        } else {
            current_value = {weight.value_or(initial_weight), stretch.value_or(initial_stretch),
                style.value_or(initial_style), start_character, character_count};
        }
    }

    apply_current_value();
}

} // namespace

void set_layout_font_segments(TextLayout& layout, std::span<const text_style::FontSegment> font_segments)
{
    process_simple_style_property<std::wstring>(font_segments, &text_style::FormatProperties::font_family,
        [&](auto&& value, auto&& text_range) { layout.set_family(value.c_str(), text_range); });

    process_simple_style_property<float>(font_segments, &text_style::FormatProperties::font_size,
        [&](auto&& value, auto&& text_range) { layout.set_size(pt_to_dip(value), text_range); });

    process_simple_style_property<text_style::TextDecorationType>(
        font_segments, &text_style::FormatProperties::text_decoration, [&](auto&& value, auto&& text_range) {
            if (value == text_style::TextDecorationType::Underline)
                layout.set_underline(true, text_range);
        });

    process_wss_style_property(font_segments, layout);
}

} // namespace uih::direct_write
