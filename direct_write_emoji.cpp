#include "stdafx.h"

#include "direct_write.h"
#include "emoji.h"

namespace uih::direct_write {

namespace {

wil::com_ptr<IDWriteFontFamily> find_font_family(
    const wil::com_ptr<IDWriteFontCollection>& font_collection, const wchar_t* family_name)
{
    uint32_t index{};
    BOOL exists{};
    THROW_IF_FAILED(font_collection->FindFamilyName(family_name, &index, &exists));

    if (!exists)
        return {};

    wil::com_ptr<IDWriteFontFamily> font_family;
    THROW_IF_FAILED(font_collection->GetFontFamily(index, &font_family));

    return font_family;
}

class EmojiFontFallback : public IDWriteFontFallback1 {
public:
    explicit EmojiFontFallback(const wil::com_ptr<IDWriteFontCollection>& font_collection,
        wil::com_ptr<IDWriteFontFallback> base_fallback, const wchar_t* emoji_family_name,
        const wchar_t* monochrome_family_name)
        : m_base_fallback(std::move(base_fallback))
    {
        m_text_font_family = find_font_family(font_collection, monochrome_family_name);
        m_emoji_font_family = find_font_family(font_collection, emoji_family_name);

        if (m_text_font_family)
            LOG_IF_FAILED(m_text_font_family->GetFirstMatchingFont(
                DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &m_text_font));

        m_base_fallback_1 = m_base_fallback.try_query<IDWriteFontFallback1>();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (__uuidof(IDWriteFontFallback) == riid) {
            AddRef();
            *ppvObject = this;
        } else if (__uuidof(IDWriteFontFallback1) == riid && m_base_fallback_1) {
            AddRef();
            *ppvObject = this;
        } else if (__uuidof(IUnknown) == riid) {
            AddRef();
            *ppvObject = static_cast<IUnknown*>(this);
        } else {
            *ppvObject = nullptr;
            return E_FAIL;
        }

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++m_ref_count; }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        if (--m_ref_count == 0) {
            delete this;
            return 0;
        }

        return m_ref_count;
    }

    HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource* analysisSource, UINT32 textPosition,
        UINT32 textLength, IDWriteFontCollection* baseFontCollection, const wchar_t* baseFamilyName,
        DWRITE_FONT_WEIGHT baseWeight, DWRITE_FONT_STYLE baseStyle, DWRITE_FONT_STRETCH baseStretch,
        UINT32* mappedLength, IDWriteFont** mappedFont, FLOAT* scale) noexcept override
    {
        const wchar_t* text{};
        uint32_t length{};
        RETURN_IF_FAILED(analysisSource->GetTextAtPosition(textPosition, &text, &length));

        std::wstring_view text_view{text, length};
        const auto map_result = map_char(text_view);

        auto pos = map_result.skip_length;

        while (pos < text_view.size()) {
            auto next_map_result = map_char(text_view.substr(pos), map_result.type == EmojiType::Emoji);

            if (next_map_result.type != map_result.type)
                break;

            pos += next_map_result.skip_length;
        }

        if (map_result.type == EmojiType::NotEmoji)
            return m_base_fallback->MapCharacters(analysisSource, textPosition, gsl::narrow_cast<uint32_t>(pos),
                baseFontCollection, baseFamilyName, baseWeight, baseStyle, baseStretch, mappedLength, mappedFont,
                scale);

        *mappedLength = gsl::narrow_cast<uint32_t>(pos);
        *scale = 1.0f;

        if (const auto& family = map_result.type == EmojiType::Emoji ? m_emoji_font_family : m_text_font_family)
            LOG_IF_FAILED(family->GetFirstMatchingFont(baseWeight, baseStretch, baseStyle, mappedFont));

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource* analysisSource, UINT32 textPosition,
        UINT32 textLength, IDWriteFontCollection* baseFontCollection, const WCHAR* baseFamilyName,
        const DWRITE_FONT_AXIS_VALUE* fontAxisValues, UINT32 fontAxisValueCount, UINT32* mappedLength, FLOAT* scale,
        IDWriteFontFace5** mappedFontFace) noexcept override
    {
        const wchar_t* text{};
        uint32_t length{};
        RETURN_IF_FAILED(analysisSource->GetTextAtPosition(textPosition, &text, &length));

        std::wstring_view text_view{text, length};
        const auto map_result = map_char(text_view);

        auto pos = map_result.skip_length;

        while (pos < text_view.size()) {
            auto next_map_result = map_char(text_view.substr(pos), map_result.type == EmojiType::Emoji);

            if (next_map_result.type != map_result.type)
                break;

            pos += next_map_result.skip_length;
        }

        if (map_result.type == EmojiType::NotEmoji)
            return m_base_fallback_1->MapCharacters(analysisSource, textPosition, gsl::narrow_cast<uint32_t>(pos),
                baseFontCollection, baseFamilyName, fontAxisValues, fontAxisValueCount, mappedLength, scale,
                mappedFontFace);

        *mappedLength = gsl::narrow_cast<uint32_t>(pos);

        const auto& family = map_result.type == EmojiType::Emoji ? m_emoji_font_family : m_text_font_family;
        *scale = 1.0f;

        if (!family)
            return S_OK;

        auto family_2 = family.try_query<IDWriteFontFamily2>();

        if (!family_2)
            return E_NOINTERFACE;

        wil::com_ptr<IDWriteFontSet1> font_set;
        RETURN_IF_FAILED(family_2->GetFontSet(&font_set));

        wil::com_ptr<IDWriteFontSet1> matching_font_set;
        RETURN_IF_FAILED(font_set->GetMatchingFonts(nullptr, fontAxisValues, fontAxisValueCount, &matching_font_set));

        LOG_IF_FAILED(matching_font_set->CreateFontFace(0, mappedFontFace));

        return S_OK;
    }

private:
    enum class EmojiType {
        Emoji,
        Text,
        NotEmoji,
    };

    struct MapResult {
        EmojiType type{};
        size_t skip_length{};
    };

    MapResult map_char(std::wstring_view text, bool in_emoji_sequence = false) const
    {
        uint32_t decoded{};
        auto decoded_length = pfc::utf16_decode_char(text.data(), &decoded, text.size());

        const auto iter = emoji::emojis.find(decoded);

        if (iter == emoji::emojis.end())
            return MapResult{EmojiType::NotEmoji, decoded_length};

        const auto props = iter->second;

        if (!in_emoji_sequence && !props.emoji)
            return MapResult{EmojiType::NotEmoji, decoded_length};

        const auto next_char = decoded_length < text.size() ? std::make_optional(text[decoded_length]) : std::nullopt;
        const auto is_next_char_text_selector = next_char && *next_char == 0xFE0E;
        const auto is_next_char_emoji_selector = next_char && *next_char == 0xFE0F;
        const auto is_next_char_selector = is_next_char_text_selector || is_next_char_emoji_selector;

        const auto is_text_variation_selected = props.has_variation && is_next_char_text_selector;
        const auto is_emoji_variation_selected = props.has_variation && is_next_char_emoji_selector;

        const auto type = [&, this] {
            if (is_emoji_variation_selected)
                return EmojiType::Emoji;

            if (is_text_variation_selected)
                return EmojiType::Text;

            if (props.component && in_emoji_sequence && !props.has_variation)
                return EmojiType::Emoji;

            if (props.component && !props.emoji_presentation)
                return EmojiType::NotEmoji;

            if (!props.emoji_presentation) {
                if (props.fallthrough_by_default)
                    return EmojiType::NotEmoji;

                BOOL exists{};

                if (m_text_font)
                    LOG_IF_FAILED(m_text_font->HasCharacter(decoded, &exists));

                if (exists)
                    return EmojiType::Text;
            }

            return props.emoji ? EmojiType::Emoji : EmojiType::NotEmoji;
        }();

        return MapResult{type, decoded_length + (is_next_char_selector ? 1 : 0)};
    }

    std::atomic<ULONG> m_ref_count{};
    wil::com_ptr<IDWriteFont> m_text_font;
    wil::com_ptr<IDWriteFontFamily> m_text_font_family;
    wil::com_ptr<IDWriteFontFamily> m_emoji_font_family;
    wil::com_ptr<IDWriteFontFallback> m_base_fallback;
    wil::com_ptr<IDWriteFontFallback1> m_base_fallback_1;
};

} // namespace

wil::com_ptr<IDWriteFontFallback> create_emoji_font_fallback(const wil::com_ptr<IDWriteFontCollection>& font_collection,
    wil::com_ptr<IDWriteFontFallback> base_fallback, const wchar_t* emoji_family_name,
    const wchar_t* monochrome_family_name)
{
    return new EmojiFontFallback(font_collection, std::move(base_fallback), emoji_family_name, monochrome_family_name);
}

} // namespace uih::direct_write
