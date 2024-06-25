#include "stdafx.h"

using namespace std::string_view_literals;

namespace uih {

std::string remove_colour_codes(std::string_view text)
{
    std::string stripped_text;
    size_t offset{};

    while (true) {
        const size_t index = text.find("\3"sv, offset);

        const auto fragment_length = index == std::string_view::npos ? std::string_view::npos : index - offset;
        const auto fragment = text.substr(offset, fragment_length);

        if (!fragment.empty())
            stripped_text.append(fragment);

        if (index == std::string_view::npos)
            break;

        offset = text.find("\3"sv, index + 1);

        if (offset == std::string_view::npos)
            break;

        ++offset;
    }

    return stripped_text;
}

} // namespace uih
