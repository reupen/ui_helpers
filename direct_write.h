#pragma once

namespace uih::direct_write {

class RenderingParams {
public:
    using Ptr = std::shared_ptr<RenderingParams>;

    RenderingParams(
        wil::com_ptr_t<IDWriteFactory> factory, DWRITE_RENDERING_MODE rendering_mode, bool force_greyscale_antialiasing)
        : m_factory(std::move(factory))
        , m_rendering_mode(rendering_mode)
        , m_force_greyscale_antialiasing(force_greyscale_antialiasing)
    {
    }

    wil::com_ptr_t<IDWriteRenderingParams> get(HWND wnd) const;
    DWRITE_RENDERING_MODE rendering_mode() const { return m_rendering_mode; }

private:
    mutable wil::com_ptr_t<IDWriteRenderingParams> m_rendering_params;
    mutable HMONITOR m_monitor{};

    wil::com_ptr_t<IDWriteFactory> m_factory;
    DWRITE_RENDERING_MODE m_rendering_mode{};
    bool m_force_greyscale_antialiasing{};
};

class TextLayout {
public:
    TextLayout(wil::com_ptr_t<IDWriteFactory> factory, wil::com_ptr_t<IDWriteGdiInterop> gdi_interop,
        wil::com_ptr_t<IDWriteTextLayout> text_layout, RenderingParams::Ptr rendering_params)
        : m_factory(std::move(factory))
        , m_gdi_interop(std::move(gdi_interop))
        , m_text_layout(std::move(text_layout))
        , m_rendering_params(std::move(rendering_params))
    {
    }

    float get_max_height() const noexcept;
    float get_max_width() const noexcept;
    DWRITE_TEXT_METRICS get_metrics() const;
    DWRITE_OVERHANG_METRICS get_overhang_metrics() const;
    void render_with_transparent_background(
        HWND wnd, HDC dc, RECT output_rect, COLORREF default_colour, float x_origin_offset = 0.0f) const;
    void render_with_solid_background(HWND wnd, HDC dc, float x_origin, float y_origin, RECT clip_rect,
        COLORREF background_colour, COLORREF default_text_colour) const;
    void set_colour(COLORREF colour, DWRITE_TEXT_RANGE text_range) const;
    void set_max_height(float value) const;
    void set_max_width(float value) const;
    void set_underline(bool is_underlined, DWRITE_TEXT_RANGE text_range) const;
    void set_font(const wchar_t* font_family, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch,
        DWRITE_FONT_STYLE style, float size, DWRITE_TEXT_RANGE text_range) const;

private:
    wil::com_ptr_t<IDWriteFactory> m_factory;
    wil::com_ptr_t<IDWriteGdiInterop> m_gdi_interop;
    wil::com_ptr_t<IDWriteTextLayout> m_text_layout;
    RenderingParams::Ptr m_rendering_params;
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
        RenderingParams::Ptr rendering_params)
        : m_context(std::move(context))
        , m_factory(std::move(factory))
        , m_gdi_interop(std::move(gdi_interop))
        , m_text_format(std::move(text_format))
        , m_rendering_params(std::move(rendering_params))
    {
    }

    void set_text_alignment(DWRITE_TEXT_ALIGNMENT value = DWRITE_TEXT_ALIGNMENT_LEADING) const;
    void set_paragraph_alignment(DWRITE_PARAGRAPH_ALIGNMENT value) const;
    void set_word_wrapping(DWRITE_WORD_WRAPPING value) const;
    void enable_trimming_sign();
    void disable_trimming_sign() const;

    [[nodiscard]] int get_minimum_height(std::wstring_view text = std::wstring_view(L"", 0)) const;
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
    RenderingParams::Ptr m_rendering_params;
};

struct Font {
    wil::com_ptr_t<IDWriteFont> font;
    std::wstring localised_name;
};

struct FontFamily {
    wil::com_ptr_t<IDWriteFontFamily> family;
    std::wstring localised_name;
    bool is_symbol_font{};

    std::vector<Font> fonts() const;
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

    const wil::com_ptr_t<IDWriteFactory1>& factory() { return m_factory; }

    LOGFONT create_log_font(const wil::com_ptr_t<IDWriteFont>& font) const;
    wil::com_ptr_t<IDWriteFont> create_font(const LOGFONT& log_font) const;
    TextFormat create_text_format(const wil::com_ptr_t<IDWriteFont>& font, float font_size);

    TextFormat create_text_format(const wil::com_ptr_t<IDWriteFontFamily>& font_family, DWRITE_FONT_WEIGHT weight,
        DWRITE_FONT_STRETCH stretch, DWRITE_FONT_STYLE style, float font_size);

    TextFormat create_text_format(const wchar_t* family_name, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch,
        DWRITE_FONT_STYLE style, float font_size);

    TextFormat create_text_format(const LOGFONT& log_font, float font_size);

    std::optional<TextFormat> create_text_format_with_fallback(
        const LOGFONT& log_font, std::optional<float> font_size = {}) noexcept;

    TextFormat wrap_text_format(wil::com_ptr_t<IDWriteTextFormat> text_format,
        DWRITE_RENDERING_MODE rendering_mode = DWRITE_RENDERING_MODE_DEFAULT, bool force_greyscale_antialiasing = false,
        bool set_defaults = true);

    wil::com_ptr_t<IDWriteTypography> get_default_typography();

    std::optional<std::wstring> get_face_name(const wchar_t* family_name, DWRITE_FONT_WEIGHT weight,
        DWRITE_FONT_STRETCH stretch, DWRITE_FONT_STYLE style) const;
    std::vector<FontFamily> get_font_families() const;

private:
    inline static std::weak_ptr<Context> s_ptr;

    wil::com_ptr_t<IDWriteFactory1> m_factory;
    wil::com_ptr_t<IDWriteGdiInterop> m_gdi_interop;
    wil::com_ptr_t<IDWriteTypography> m_default_typography;
};

std::wstring get_localised_string(const wil::com_ptr_t<IDWriteLocalizedStrings>& localised_strings);
float get_default_scaling_factor();

float dip_to_px(float dip, float scaling_factor = get_default_scaling_factor());
float px_to_dip(float px, float scaling_factor = get_default_scaling_factor());
float pt_to_dip(float point_size);

} // namespace uih::direct_write
