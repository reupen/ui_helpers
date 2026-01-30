#include "stdafx.h"

#include "text_style.h"

#include "direct_write.h"

using namespace std::string_view_literals;

namespace uih::text_style {

namespace {

std::optional<float> safe_stof(const std::wstring& value)
{
    try {
        return std::stof(value);
    } catch (const std::exception&) {
        return {};
    }
}

bool are_strings_equal(std::wstring_view left, std::wstring_view right)
{
    return CompareStringEx(LOCALE_NAME_INVARIANT, NORM_IGNORECASE, left.data(), gsl::narrow<int>(left.length()),
               right.data(), gsl::narrow<int>(right.length()), nullptr, nullptr, 0)
        == CSTR_EQUAL;
}

constexpr auto format_properties_members
    = std::tuple{&FormatProperties::text_decoration, &FormatProperties::font_family, &FormatProperties::font_size,
        &FormatProperties::font_stretch, &FormatProperties::font_style, &FormatProperties::font_weight};

template <typename Callback>
void for_each_property(FormatProperties& properties, Callback&& callback)
{
    std::apply([&](auto... members) { (callback(properties.*members), ...); }, format_properties_members);
}

template <typename Member>
void merge_property(FormatProperties& target, FormatProperties& source, Member member)
{
    if (!(target.*member))
        target.*member = source.*member;
}

void merge_properties(FormatProperties& target, FormatProperties& source)
{
    std::apply([&](auto... members) { (merge_property(target, source, members), ...); }, format_properties_members);
}

std::optional<FormatProperties> parse_legacy_font_code(const std::vector<std::wstring_view>& parts,
    const std::function<void(std::wstring)>& print_legacy_feedback,
    const direct_write::Context::Ptr& direct_write_context)
{
    if (parts.size() < 4)
        return {};

    const auto& family_part = parts[1];
    const auto& size_part = parts[2];

    LOGFONT lf{};
    wcsncpy_s(lf.lfFaceName, std::wstring(family_part).c_str(), _TRUNCATE);

    std::wstring size_str(size_part.data(), size_part.size());
    const auto size_points = safe_stof(size_str);

    if (!size_points)
        return {};

    TextDecorationType text_decoration{TextDecorationType::None};
    const auto& style_part = parts[3];
    auto style_elements = std::views::split(style_part, L';');

    for (auto style_element : style_elements) {
        const std::wstring_view style_element_view(style_element.data(), style_element.size());

        const auto equals_index = style_element_view.find(L'=');
        const auto style_name = style_element_view.substr(0, equals_index);
        const auto style_enabled = equals_index == std::wstring_view::npos
            || are_strings_equal(style_element_view.substr(equals_index + 1), L"true");

        if (!style_enabled)
            continue;

        if (are_strings_equal(style_name, L"bold")) {
            lf.lfWeight = true;
        } else if (are_strings_equal(style_name, L"italic")) {
            lf.lfItalic = true;
        } else if (are_strings_equal(style_name, L"underline")) {
            text_decoration = TextDecorationType::Underline;
        }
    }

    try {
        const auto direct_write_font = direct_write_context->create_font(lf);

        wil::com_ptr<IDWriteFontFamily> font_family;
        THROW_IF_FAILED(direct_write_font->GetFontFamily(&font_family));

        wil::com_ptr<IDWriteLocalizedStrings> localised_strings;
        THROW_IF_FAILED(font_family->GetFamilyNames(&localised_strings));

        auto family_name = direct_write::get_localised_string(localised_strings);

        const auto weight = direct_write_font->GetWeight();
        const auto stretch = direct_write_font->GetStretch();
        const auto style = direct_write_font->GetStyle();

        return FormatProperties{std::move(family_name), *size_points, weight, stretch, style, text_decoration};
    } catch (...) {
        if (wil::ResultFromCaughtException() == DWRITE_E_NOFONT) {
            if (print_legacy_feedback)
                print_legacy_feedback(std::format(L"font family \"{}\" not found\n", lf.lfFaceName).c_str());
            return {};
        }

        LOG_CAUGHT_EXCEPTION();
        return {};
    }
}

std::optional<FormatProperties> parse_font_code(std::wstring_view text,
    const std::function<void(std::wstring)>& print_legacy_feedback,
    const direct_write::Context::Ptr& direct_write_context)
{
    const auto parts_split_views = std::views::split(text, L'\uF8FF') | ranges::to<std::vector>;
    const auto parts = parts_split_views
        | ranges::views::transform([](auto&& part) { return std::wstring_view(part.data(), part.size()); })
        | ranges::to<std::vector>;

    if (parts.size() < 2)
        return {};

    const auto& version = parts[0];

    if (version == L"\x1"sv && direct_write_context)
        return parse_legacy_font_code(parts, print_legacy_feedback, direct_write_context);

    if (version == L"\x2"sv)
        return parse_format_properties(parts[1]);

    return {};
}

} // namespace

std::optional<ColourPair> parse_colour_code(std::wstring_view text)
{
    const auto bar_index = text.find(L'|');
    const auto non_selected_colour_hex = text.substr(0, bar_index);

    if (non_selected_colour_hex.empty())
        return {};

    COLORREF non_selected_colour
        = mmh::strtoul_n(non_selected_colour_hex.data(), std::min(size_t{6}, non_selected_colour_hex.length()), 0x10);

    const COLORREF selected_colour = [&] {
        if (bar_index == std::wstring_view::npos)
            return 0xffffff - non_selected_colour;

        const auto selected_colour_hex = text.substr(bar_index + 1);
        return mmh::strtoul_n<wchar_t, COLORREF>(
            selected_colour_hex.data(), std::min(size_t{6}, selected_colour_hex.length()), 0x10);
    }();

    return ColourPair{non_selected_colour, selected_colour};
}

std::tuple<std::wstring, std::vector<ColourSegment>, std::vector<FontSegment>> process_colour_and_font_codes(
    std::wstring_view text, const FormatProperties& initial_format_properties,
    const std::function<void(std::wstring)>& print_legacy_feedback,
    const direct_write::Context::Ptr& direct_write_context)
{
    auto result = std::tuple<std::wstring, std::vector<ColourSegment>, std::vector<FontSegment>>{};
    auto& [stripped_text, coloured_segments, font_segments] = result;

    size_t offset{};
    size_t colour_segment_start{};
    size_t font_segment_start{};
    std::optional<ColourPair> cr_current;
    std::optional current_font{initial_format_properties};

    while (true) {
        const size_t index = text.find_first_of(L"\3\7"sv, offset);

        const auto fragment_length = index == std::wstring_view::npos ? std::wstring_view::npos : index - offset;
        const auto fragment = text.substr(offset, fragment_length);
        const auto is_eos = index == std::wstring_view::npos;
        const auto is_colour_code = !is_eos && text[index] == L'\3';
        const auto is_font_code = !is_eos && text[index] == L'\7';

        if (!fragment.empty())
            stripped_text.append(fragment);

        if (cr_current && (is_eos || is_colour_code) && stripped_text.length() > colour_segment_start)
            coloured_segments.emplace_back(cr_current->colour, cr_current->selected_colour, colour_segment_start,
                stripped_text.length() - colour_segment_start);

        if (current_font && (is_eos || is_font_code) && stripped_text.length() > font_segment_start) {
            FormatProperties cleaned_font{*current_font};

            for_each_property(cleaned_font, [](auto&& member) {
                if (member && std::holds_alternative<InitialPropertyValue>(*member)) {
                    member.reset();
                }
            });

            if (cleaned_font)
                font_segments.emplace_back(
                    cleaned_font, font_segment_start, stripped_text.length() - font_segment_start);
        }

        if (is_eos)
            break;

        offset = text.find(is_colour_code ? L'\3' : L'\7', index + 1);

        if (offset == std::wstring_view::npos)
            break;

        const auto code_text = text.substr(index + 1, offset - index - 1);

        if (is_colour_code) {
            cr_current = parse_colour_code(code_text);
            colour_segment_start = stripped_text.size();
        } else {
            auto new_properties = parse_font_code(code_text, print_legacy_feedback, direct_write_context);

            const auto last_font_segment_end = font_segments.empty()
                ? std::nullopt
                : std::make_optional(font_segments.rbegin()->start_character + font_segments.rbegin()->character_count);
            const auto continues_from_last_segment
                = current_font && (!last_font_segment_end || *last_font_segment_end == stripped_text.size());

            if (!new_properties) {
                current_font.reset();
            } else if (current_font && continues_from_last_segment) {
                merge_properties(*new_properties, *current_font);
                current_font = new_properties;
            } else {
                current_font = new_properties;
            }

            font_segment_start = stripped_text.size();
        }

        ++offset;
    }

    return result;
}

std::optional<std::wstring> remove_newlines(std::wstring_view text)
{
    std::optional<std::wstring> stripped_text;
    size_t offset{};

    while (true) {
        const size_t index = text.find_first_of(L"\r\n"sv, offset);

        if (index == std::string_view::npos && !stripped_text)
            return {};

        const auto fragment_length = index == std::string_view::npos ? std::string_view::npos : index - offset;
        const auto fragment = text.substr(offset, fragment_length);

        if (!stripped_text) {
            stripped_text.emplace();
            stripped_text->reserve(text.size());
        }

        if (!fragment.empty())
            stripped_text->append(fragment);

        if (index == std::string_view::npos)
            break;

        offset = text.find_first_not_of(L"\r\n"sv, index + 1);

        if (offset == std::string_view::npos)
            break;
    }

    return stripped_text;
}

std::string remove_colour_and_font_codes(std::string_view text)
{
    std::string stripped_text;
    size_t offset{};

    while (true) {
        const size_t index = text.find_first_of("\3\7"sv, offset);

        const auto fragment_length = index == std::string_view::npos ? std::string_view::npos : index - offset;
        const auto fragment = text.substr(offset, fragment_length);

        if (!fragment.empty())
            stripped_text.append(fragment);

        if (index == std::string_view::npos)
            break;

        offset = text.find_first_of(text[index], index + 1);

        if (offset == std::string_view::npos)
            break;

        ++offset;
    }

    return stripped_text;
}

} // namespace uih::text_style
