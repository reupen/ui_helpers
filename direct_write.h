#pragma once

namespace uih::direct_write {

class TextLayout {
public:
    TextLayout(wil::com_ptr_t<IDWriteFactory> factory, wil::com_ptr_t<IDWriteGdiInterop> gdi_interop,
        wil::com_ptr_t<IDWriteTextLayout> text_layout)
        : m_factory(std::move(factory))
        , m_gdi_interop(std::move(gdi_interop))
        , m_text_layout(std::move(text_layout))
    {
    }

    static float s_default_scaling_factor()
    {
        return gsl::narrow_cast<float>(get_system_dpi_cached().cx) / gsl::narrow_cast<float>(USER_DEFAULT_SCREEN_DPI);
    }

    DWRITE_TEXT_METRICS get_metrics() const;
    DWRITE_OVERHANG_METRICS get_overhang_metrics() const;
    void render(HDC dc, RECT rect, COLORREF default_colour, float x_origin_offset = 0.0f) const;
    void set_colour(COLORREF colour, DWRITE_TEXT_RANGE text_range) const;

private:
    const float m_scaling_factor{s_default_scaling_factor()};
    wil::com_ptr_t<IDWriteFactory> m_factory;
    wil::com_ptr_t<IDWriteGdiInterop> m_gdi_interop;
    wil::com_ptr_t<IDWriteTextLayout> m_text_layout;
};

struct TextPosition {
    int left{};
    float left_remainder_dip{};
    int top{};
    int width{};
    int height{};
};

class TextFormat {
public:
    TextFormat(std::shared_ptr<class Context> context, wil::com_ptr_t<IDWriteFactory> factory,
        wil::com_ptr_t<IDWriteGdiInterop> gdi_interop, wil::com_ptr_t<IDWriteTextFormat> text_format,
        wil::com_ptr_t<IDWriteInlineObject> trimming_sign)
        : m_context(std::move(context))
        , m_factory(std::move(factory))
        , m_gdi_interop(std::move(gdi_interop))
        , m_text_format(std::move(text_format))
        , m_trimming_sign(std::move(trimming_sign))
    {
    }

    void set_alignment(DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING) const;
    void enable_trimming_sign() const;
    void disable_trimming_sign() const;

    [[nodiscard]] int get_minimum_height() const;
    [[nodiscard]] TextPosition measure_text_position(
        std::wstring_view text, int height, float max_width = 65536.0f) const;
    [[nodiscard]] int measure_text_width(std::wstring_view text) const;
    [[nodiscard]] TextLayout create_text_layout(std::wstring_view text, float max_width, float max_height) const;

private:
    std::shared_ptr<Context> m_context;
    wil::com_ptr_t<IDWriteFactory> m_factory;
    wil::com_ptr_t<IDWriteGdiInterop> m_gdi_interop;
    wil::com_ptr_t<IDWriteTextFormat> m_text_format;
    wil::com_ptr_t<IDWriteInlineObject> m_trimming_sign;
};

class Context : public std::enable_shared_from_this<Context> {
public:
    using Ptr = std::shared_ptr<Context>;

    static Ptr s_create()
    {
        auto ptr = s_ptr.lock();

        if (!ptr) {
            ptr = std::make_shared<Context>();
            s_ptr = ptr;
        }

        return ptr;
    }

    Context();

    TextFormat create_text_format(const LOGFONT& log_font, float font_size);
    std::optional<TextFormat> create_text_format_with_fallback(
        const LOGFONT& log_font, std::optional<float> font_size) noexcept;
    wil::com_ptr_t<IDWriteTypography> get_default_typography();

private:
    inline static std::weak_ptr<Context> s_ptr;

    wil::com_ptr_t<IDWriteFactory1> m_factory;
    wil::com_ptr_t<IDWriteGdiInterop> m_gdi_interop;
    wil::com_ptr_t<IDWriteTypography> m_default_typography;
};

} // namespace uih::direct_write
