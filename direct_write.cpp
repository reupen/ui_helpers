#include "stdafx.h"

using namespace std::string_view_literals;

namespace uih::direct_write {

namespace {

constexpr COLORREF direct_write_colour_to_colorref(DWRITE_COLOR_F colour)
{
    constexpr auto transform_channel
        = [](auto value) { return gsl::narrow_cast<uint8_t>(std::clamp(std::round(value * 255.0f), 0.0f, 255.0f)); };

    return RGB(transform_channel(colour.r), transform_channel(colour.g), transform_channel(colour.b));
}

class __declspec(uuid("E50EB289-73D3-481C-B726-09B88105539A")) ColourEffect : public IUnknown {
public:
    explicit ColourEffect(COLORREF colour) : m_colour(colour) {}

    COLORREF GetColour() const { return m_colour; }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (__uuidof(ColourEffect) == riid) {
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

private:
    std::atomic<ULONG> m_ref_count{};
    COLORREF m_colour{};
};

class GdiTextRenderer : public IDWriteTextRenderer {
public:
    GdiTextRenderer(wil::com_ptr<IDWriteFactory> factory, IDWriteBitmapRenderTarget* bitmapRenderTarget,
        IDWriteRenderingParams* renderingParams, COLORREF default_colour);

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) noexcept override
    {
        if (__uuidof(IDWriteTextRenderer) == riid) {
            AddRef();
            *ppvObject = this;
        } else if (__uuidof(IDWritePixelSnapping) == riid) {
            AddRef();
            *ppvObject = static_cast<IDWritePixelSnapping*>(this);
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

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled) noexcept override
    {
        *isDisabled = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentTransform(
        void* clientDrawingContext, DWRITE_MATRIX* transform) noexcept override
    {
        return m_render_target->GetCurrentTransform(transform);
    }

    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip) noexcept override
    {
        *pixelsPerDip = m_render_target->GetPixelsPerDip();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode, const DWRITE_GLYPH_RUN* glyphRun,
        const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription, IUnknown* clientDrawingEffect) noexcept override
    {
        wil::com_ptr_t<ColourEffect> colour_effect;
        if (clientDrawingEffect) {
            colour_effect = wil::try_com_query<ColourEffect>(clientDrawingEffect);
        }

        const COLORREF colour = colour_effect ? colour_effect->GetColour() : m_default_colour;

        wil::com_ptr_t<IDWriteColorGlyphRunEnumerator> colour_glyph_run_enumerator;

        try {
            colour_glyph_run_enumerator = GetColorGlyphEnumerator(
                clientDrawingContext, baselineOriginX, baselineOriginY, measuringMode, glyphRun, glyphRunDescription);
        }
        CATCH_RETURN()

        if (!colour_glyph_run_enumerator) {
            return m_render_target->DrawGlyphRun(
                baselineOriginX, baselineOriginY, measuringMode, glyphRun, m_rendering_params.get(), colour);
        }

        while (true) {
            BOOL has_run{};

            RETURN_IF_FAILED(colour_glyph_run_enumerator->MoveNext(&has_run));

            if (!has_run)
                return S_OK;

            const DWRITE_COLOR_GLYPH_RUN* colour_glyph_run{};
            RETURN_IF_FAILED(colour_glyph_run_enumerator->GetCurrentRun(&colour_glyph_run));

            const auto colour_run_colour = colour_glyph_run->paletteIndex == 0xffff
                ? colour
                : direct_write_colour_to_colorref(colour_glyph_run->runColor);

            RETURN_IF_FAILED(
                m_render_target->DrawGlyphRun(colour_glyph_run->baselineOriginX, colour_glyph_run->baselineOriginY,
                    measuringMode, &colour_glyph_run->glyphRun, m_rendering_params.get(), colour_run_colour));
        }
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
        const DWRITE_UNDERLINE* underline, IUnknown* clientDrawingEffect) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void* clientDrawingContext, FLOAT baselineOriginX,
        FLOAT baselineOriginY, const DWRITE_STRIKETHROUGH* strikethrough,
        IUnknown* clientDrawingEffect) noexcept override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY,
        IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
        IUnknown* clientDrawingEffect) noexcept override
    {
        return inlineObject->Draw(
            clientDrawingContext, this, originX, originY, isSideways, isRightToLeft, clientDrawingEffect);
    }

private:
    wil::com_ptr_t<IDWriteColorGlyphRunEnumerator> GetColorGlyphEnumerator(void* clientDrawingContext,
        FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode,
        const DWRITE_GLYPH_RUN* glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription)
    {
        auto factory = m_factory.try_query<IDWriteFactory2>();

        if (!factory)
            return {};

        DWRITE_MATRIX transform{};
        THROW_IF_FAILED(GetCurrentTransform(clientDrawingContext, &transform));

        wil::com_ptr_t<IDWriteColorGlyphRunEnumerator> enumerator;

        const auto hr = factory->TranslateColorGlyphRun(
            baselineOriginX, baselineOriginY, glyphRun, glyphRunDescription, measuringMode, &transform, 0, &enumerator);

        if (hr == DWRITE_E_NOCOLOR)
            return {};

        THROW_IF_FAILED(hr);

        return enumerator;
    }

    std::atomic<ULONG> m_ref_count{};
    wil::com_ptr_t<IDWriteFactory> m_factory;
    wil::com_ptr<IDWriteBitmapRenderTarget> m_render_target;
    wil::com_ptr<IDWriteRenderingParams> m_rendering_params;
    COLORREF m_default_colour{};
};

GdiTextRenderer::GdiTextRenderer(wil::com_ptr<IDWriteFactory> factory, IDWriteBitmapRenderTarget* bitmapRenderTarget,
    IDWriteRenderingParams* renderingParams, COLORREF default_colour)
    : m_factory(std::move(factory))
    , m_render_target(bitmapRenderTarget)
    , m_rendering_params(renderingParams)
    , m_default_colour(default_colour)
{
}

} // namespace

DWRITE_TEXT_METRICS TextLayout::get_metrics() const
{
    DWRITE_TEXT_METRICS metrics{};
    THROW_IF_FAILED(m_text_layout->GetMetrics(&metrics));
    return metrics;
}

DWRITE_OVERHANG_METRICS TextLayout::get_overhang_metrics() const
{
    DWRITE_OVERHANG_METRICS overhang_metrics{};
    THROW_IF_FAILED(m_text_layout->GetOverhangMetrics(&overhang_metrics));
    return overhang_metrics;
}

void TextLayout::set_colour(COLORREF colour, DWRITE_TEXT_RANGE text_range) const
{
    const auto colour_effect = wil::com_ptr<ColourEffect>(new ColourEffect(colour));
    THROW_IF_FAILED(m_text_layout->SetDrawingEffect(colour_effect.get(), text_range));
}

void TextLayout::render(HDC dc, RECT rect, COLORREF default_colour, float x_origin_offset) const
{
    const auto layout_width = m_text_layout->GetMaxWidth();
    const auto layout_height = m_text_layout->GetMaxHeight();
    const auto overhang_metrics = get_overhang_metrics();

    const auto draw_left_px = std::max(0l, gsl::narrow_cast<long>((-overhang_metrics.left) * m_scaling_factor));
    const auto draw_left_dip = gsl::narrow_cast<float>(draw_left_px) / m_scaling_factor;

    const auto draw_right_px = gsl::narrow_cast<long>((layout_width + overhang_metrics.right) * m_scaling_factor + 1);

    const auto draw_top_px = std::max(0l, gsl::narrow_cast<long>((-overhang_metrics.top) * m_scaling_factor));
    const auto draw_top_dip = gsl::narrow_cast<float>(draw_top_px) / m_scaling_factor;

    const auto draw_bottom_px
        = gsl::narrow_cast<long>((layout_height + overhang_metrics.bottom) * m_scaling_factor + 1);

    const auto rect_width = wil::rect_width(rect);
    const auto rect_height = wil::rect_height(rect);

    const auto bitmap_width = std::min(rect_width, draw_right_px - draw_left_px);
    const auto bitmap_height = std::min(rect_height, draw_bottom_px - draw_top_px);
    const auto source_x = rect.left + (bitmap_width < rect_width ? draw_left_px : 0l);
    const auto source_y = rect.top + (bitmap_height < rect_height ? draw_top_px : 0l);

    wil::com_ptr_t<IDWriteBitmapRenderTarget> bitmap_render_target;
    THROW_IF_FAILED(m_gdi_interop->CreateBitmapRenderTarget(dc, bitmap_width, bitmap_height, &bitmap_render_target));
    THROW_IF_FAILED(bitmap_render_target->SetPixelsPerDip(m_scaling_factor));

    wil::com_ptr_t<IDWriteRenderingParams> dw_rendering_params;
    THROW_IF_FAILED(m_factory->CreateRenderingParams(&dw_rendering_params));

    const auto memory_dc = bitmap_render_target->GetMemoryDC();

    wil::com_ptr_t<IDWriteTextRenderer> renderer
        = new GdiTextRenderer(m_factory, bitmap_render_target.get(), dw_rendering_params.get(), default_colour);

    BitBlt(memory_dc, 0, 0, bitmap_width, bitmap_height, dc, source_x, source_y, SRCCOPY);

    THROW_IF_FAILED(m_text_layout->Draw(NULL, renderer.get(), -draw_left_dip + x_origin_offset, -draw_top_dip));

    BitBlt(dc, source_x, source_y, bitmap_width, bitmap_height, memory_dc, 0, 0, SRCCOPY);
}

void TextFormat::set_alignment(DWRITE_TEXT_ALIGNMENT alignment) const
{
    THROW_IF_FAILED(m_text_format->SetTextAlignment(alignment));
}

void TextFormat::enable_trimming_sign() const
{
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    THROW_IF_FAILED(m_text_format->SetTrimming(&trimming, m_trimming_sign.get()));
}

void TextFormat::disable_trimming_sign() const
{
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
    THROW_IF_FAILED(m_text_format->SetTrimming(&trimming, nullptr));
}

int TextFormat::get_minimum_height() const
{
    try {
        auto max_size = 65536.0f;
        const auto text_layout = create_text_layout(L""sv, max_size, max_size);
        const auto metrics = text_layout.get_metrics();
        auto top_half = max_size / 2 - metrics.top;
        auto bottom_half = metrics.top + metrics.height - max_size / 2;

        return gsl::narrow_cast<int>(
            std::max(top_half, bottom_half) * 2.0f * TextLayout::s_default_scaling_factor() + 1);
    }
    CATCH_LOG()

    return 1;
}

TextPosition TextFormat::measure_text_position(std::wstring_view text, int height, float max_width) const
{
    try {
        const auto text_layout
            = create_text_layout(text, max_width, static_cast<float>(height) / TextLayout::s_default_scaling_factor());
        const auto metrics = text_layout.get_metrics();
        const auto scaling_factor = TextLayout::s_default_scaling_factor();
        const auto left = gsl::narrow_cast<int>((metrics.left) * scaling_factor);
        const auto left_remainder_dip = metrics.left - gsl::narrow_cast<float>(left) / scaling_factor;

        return {left, left_remainder_dip, gsl::narrow_cast<int>((metrics.top) * scaling_factor),
            gsl::narrow_cast<int>((metrics.width) * scaling_factor + 1),
            gsl::narrow_cast<int>((metrics.height) * scaling_factor + 1)};
    }
    CATCH_LOG()

    return {};
}

int TextFormat::measure_text_width(std::wstring_view text) const
{
    try {
        const auto text_layout = create_text_layout(text, 65536.0f, 65536.0f);
        const auto metrics = text_layout.get_metrics();
        return gsl::narrow_cast<int>((metrics.width) * TextLayout::s_default_scaling_factor() + 1);
    }
    CATCH_LOG()

    return 0;
}

TextLayout TextFormat::create_text_layout(std::wstring_view text, float max_width, float max_height) const
{
    const auto text_length = gsl::narrow<uint32_t>(text.length());

    wil::com_ptr_t<IDWriteTextLayout> text_layout;
    THROW_IF_FAILED(m_factory->CreateTextLayout(
        text.data(), text_length, m_text_format.get(), max_width, max_height, &text_layout));

    const auto typography = m_context->get_default_typography();
    THROW_IF_FAILED(text_layout->SetTypography(typography.get(), {0, text_length}));

    return {m_factory, m_gdi_interop, text_layout};
}

Context::Context()
{
    THROW_IF_FAILED(
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1), m_factory.put_unknown()));
    THROW_IF_FAILED(m_factory->GetGdiInterop(&m_gdi_interop));
}

TextFormat Context::create_text_format(const LOGFONT& log_font, float font_size)
{
    wil::com_ptr_t<IDWriteFont> dw_font;
    THROW_IF_FAILED(m_gdi_interop->CreateFontFromLOGFONT(&log_font, &dw_font));

    wil::com_ptr_t<IDWriteFontFamily> dw_font_family;
    THROW_IF_FAILED(dw_font->GetFontFamily(&dw_font_family));

    wil::com_ptr_t<IDWriteLocalizedStrings> dw_family_names;
    THROW_IF_FAILED(dw_font_family->GetFamilyNames(&dw_family_names));

    uint32_t length{};
    THROW_IF_FAILED(dw_family_names->GetStringLength(0, &length));

    std::vector<wchar_t> family_name(length + 1, 0);

    THROW_IF_FAILED(dw_family_names->GetString(0, family_name.data(), length + 1));

    const auto fallback_font_size = 20.0f * gsl::narrow_cast<float>(USER_DEFAULT_SCREEN_DPI)
        / gsl::narrow_cast<float>(get_system_dpi_cached().cx);

    wil::com_ptr_t<IDWriteTextFormat> text_format;
    THROW_IF_FAILED(m_factory->CreateTextFormat(family_name.data(), NULL, dw_font->GetWeight(), dw_font->GetStyle(),
        dw_font->GetStretch(), font_size > 0 ? font_size : fallback_font_size, L"", &text_format));

    wil::com_ptr_t<IDWriteInlineObject> trimming_sign;
    THROW_IF_FAILED(m_factory->CreateEllipsisTrimmingSign(text_format.get(), &trimming_sign));

    THROW_IF_FAILED(text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    THROW_IF_FAILED(text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));

    return {shared_from_this(), m_factory, m_gdi_interop, text_format, trimming_sign};
}

std::optional<TextFormat> Context::create_text_format_with_fallback(
    const LOGFONT& log_font, std::optional<float> font_size) noexcept
{
    const auto resolved_size = font_size.value_or(-log_font.lfHeight * gsl::narrow<float>(USER_DEFAULT_SCREEN_DPI)
        / gsl::narrow<float>(get_system_dpi_cached().cx));

    try {
        return create_text_format(log_font, resolved_size);
    }
    CATCH_LOG()

    LOGFONT icon_font{};
    if (SystemParametersInfo(SPI_GETICONTITLELOGFONT, 0, &icon_font, 0)) {
        try {
            return create_text_format(icon_font, resolved_size);
        }
        CATCH_LOG()
    }

    return {};
}

wil::com_ptr_t<IDWriteTypography> Context::get_default_typography()
{
    if (!m_default_typography) {
        THROW_IF_FAILED(m_factory->CreateTypography(&m_default_typography));

        THROW_IF_FAILED(m_default_typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1}));
    }

    return m_default_typography;
}

} // namespace uih::direct_write
