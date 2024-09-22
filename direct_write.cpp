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
        const auto x_start = baselineOriginX;
        const auto x_end = x_start
            + underline->width * (underline->readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? -1 : 1);
        const auto y_start = baselineOriginY + underline->offset;
        const auto y_end = y_start + underline->thickness;

        wil::com_ptr_t<ColourEffect> colour_effect;
        if (clientDrawingEffect) {
            colour_effect = wil::try_com_query<ColourEffect>(clientDrawingEffect);
        }

        const COLORREF colour = colour_effect ? colour_effect->GetColour() : m_default_colour;

        const auto dc = m_render_target->GetMemoryDC();
        const auto scaling_factor = m_render_target->GetPixelsPerDip();

        const auto scale_value
            = [scaling_factor](float value) { return gsl::narrow_cast<int>(value * scaling_factor + 0.5); };

        RECT underline_rect = {scale_value(x_start), scale_value(y_start), scale_value(x_end), scale_value(y_end)};
        wil::unique_hbrush brush(CreateSolidBrush(colour));
        FillRect(dc, &underline_rect, brush.get());

        return S_OK;
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

wil::com_ptr_t<IDWriteRenderingParams> RenderingParams::get(HWND wnd) const
{
    const auto root_window = GetAncestor(wnd, GA_ROOT);
    const auto monitor = MonitorFromWindow(root_window, MONITOR_DEFAULTTONEAREST);

    if (m_rendering_params && m_monitor == monitor)
        return m_rendering_params;

    wil::com_ptr_t<IDWriteRenderingParams> default_rendering_params;
    THROW_IF_FAILED(m_factory->CreateMonitorRenderingParams(monitor, &default_rendering_params));

    wil::com_ptr_t<IDWriteRenderingParams> custom_rendering_params;
    THROW_IF_FAILED(m_factory->CreateCustomRenderingParams(default_rendering_params->GetGamma(),
        default_rendering_params->GetEnhancedContrast(), default_rendering_params->GetClearTypeLevel(),
        m_force_greyscale_antialiasing ? DWRITE_PIXEL_GEOMETRY_FLAT : default_rendering_params->GetPixelGeometry(),
        m_rendering_mode, &custom_rendering_params));

    m_monitor = monitor;
    m_rendering_params = custom_rendering_params;

    return custom_rendering_params;
}

float TextLayout::get_max_height() const noexcept
{
    return m_text_layout->GetMaxHeight();
}

float TextLayout::get_max_width() const noexcept
{
    return m_text_layout->GetMaxWidth();
}

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

void TextLayout::set_max_height(float value) const
{
    THROW_IF_FAILED(m_text_layout->SetMaxHeight(value));
}

void TextLayout::set_max_width(float value) const
{
    THROW_IF_FAILED(m_text_layout->SetMaxWidth(value));
}

void TextLayout::set_underline(bool is_underlined, DWRITE_TEXT_RANGE text_range) const
{
    THROW_IF_FAILED(m_text_layout->SetUnderline(is_underlined, text_range));
}

void TextLayout::set_font(const wchar_t* font_family, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style, float size, DWRITE_TEXT_RANGE text_range) const
{
    THROW_IF_FAILED(m_text_layout->SetFontFamilyName(font_family, text_range));
    THROW_IF_FAILED(m_text_layout->SetFontWeight(weight, text_range));
    THROW_IF_FAILED(m_text_layout->SetFontStretch(stretch, text_range));
    THROW_IF_FAILED(m_text_layout->SetFontStyle(style, text_range));
    THROW_IF_FAILED(m_text_layout->SetFontSize(size, text_range));
}

void TextLayout::render_with_transparent_background(
    HWND wnd, HDC dc, RECT output_rect, COLORREF default_colour, float x_origin_offset) const
{
    const auto metrics = get_metrics();

    if (metrics.width <= 0.0f)
        return;

    const auto scaling_factor = get_default_scaling_factor();
    const auto overhang_metrics = get_overhang_metrics();
    const auto layout_width = m_text_layout->GetMaxWidth();
    const auto layout_height = m_text_layout->GetMaxHeight();
    const auto rect_width = wil::rect_width(output_rect);
    const auto rect_height = wil::rect_height(output_rect);

    const auto draw_left_px
        = std::max(0l, gsl::narrow_cast<long>((x_origin_offset - overhang_metrics.left) * scaling_factor) - 1);
    const auto draw_right_px = std::min(rect_width,
        gsl::narrow_cast<long>((x_origin_offset + layout_width + overhang_metrics.right) * scaling_factor + 1.0f) + 1);

    const auto draw_top_px = std::max(0l, gsl::narrow_cast<long>(-overhang_metrics.top * scaling_factor) - 1);
    const auto draw_bottom_px = std::min(
        rect_height, gsl::narrow_cast<long>((layout_height + overhang_metrics.bottom) * scaling_factor + 1.0f) + 1);

    const auto bitmap_width = std::min(rect_width, draw_right_px - draw_left_px);
    const auto bitmap_height = std::min(rect_height, draw_bottom_px - draw_top_px);

    const auto is_shrunk_width = bitmap_width < rect_width;
    const auto is_shrunk_height = bitmap_height < rect_height;

    const auto source_x = output_rect.left + (is_shrunk_width ? draw_left_px : 0l);
    const auto source_y = output_rect.top + (is_shrunk_height ? draw_top_px : 0l);

    const auto draw_left_dip = is_shrunk_width ? gsl::narrow_cast<float>(draw_left_px) / scaling_factor : 0.0f;
    const auto draw_top_dip = is_shrunk_height ? gsl::narrow_cast<float>(draw_top_px) / scaling_factor : 0.0f;

    wil::com_ptr_t<IDWriteBitmapRenderTarget> bitmap_render_target;
    THROW_IF_FAILED(m_gdi_interop->CreateBitmapRenderTarget(dc, bitmap_width, bitmap_height, &bitmap_render_target));
    THROW_IF_FAILED(bitmap_render_target->SetPixelsPerDip(scaling_factor));

    const auto rendering_params = m_rendering_params->get(wnd);
    const auto memory_dc = bitmap_render_target->GetMemoryDC();

    wil::com_ptr_t<IDWriteTextRenderer> renderer
        = new GdiTextRenderer(m_factory, bitmap_render_target.get(), rendering_params.get(), default_colour);

    BitBlt(memory_dc, 0, 0, bitmap_width, bitmap_height, dc, source_x, source_y, SRCCOPY);

    THROW_IF_FAILED(m_text_layout->Draw(NULL, renderer.get(), -draw_left_dip + x_origin_offset, -draw_top_dip));

    BitBlt(dc, source_x, source_y, bitmap_width, bitmap_height, memory_dc, 0, 0, SRCCOPY);
}

void TextLayout::render_with_solid_background(HWND wnd, HDC dc, float x_origin, float y_origin, RECT clip_rect,
    COLORREF background_colour, COLORREF default_text_colour) const
{
    const auto bitmap_width = wil::rect_width(clip_rect);
    const auto bitmap_height = wil::rect_height(clip_rect);
    const auto scaling_factor = get_default_scaling_factor();

    wil::com_ptr_t<IDWriteBitmapRenderTarget> bitmap_render_target;
    THROW_IF_FAILED(m_gdi_interop->CreateBitmapRenderTarget(dc, bitmap_width, bitmap_height, &bitmap_render_target));
    THROW_IF_FAILED(bitmap_render_target->SetPixelsPerDip(scaling_factor));

    const auto memory_dc = bitmap_render_target->GetMemoryDC();
    OffsetWindowOrgEx(memory_dc, clip_rect.left, clip_rect.top, nullptr);

    const wil::unique_hbrush fill_brush(CreateSolidBrush(background_colour));
    FillRect(memory_dc, &clip_rect, fill_brush.get());

    const auto rendering_params = m_rendering_params->get(wnd);

    DWRITE_MATRIX transform{1.0f, 0.0f, 0.0f, 1.0f, -px_to_dip(gsl::narrow_cast<float>(clip_rect.left)),
        -px_to_dip(gsl::narrow_cast<float>(clip_rect.top))};

    THROW_IF_FAILED(bitmap_render_target->SetCurrentTransform(&transform));

    const wil::com_ptr_t<IDWriteTextRenderer> renderer
        = new GdiTextRenderer(m_factory, bitmap_render_target.get(), rendering_params.get(), default_text_colour);

    THROW_IF_FAILED(m_text_layout->Draw(NULL, renderer.get(), x_origin, y_origin));

    BitBlt(dc, clip_rect.left, clip_rect.top, bitmap_width, bitmap_height, memory_dc, clip_rect.left, clip_rect.top,
        SRCCOPY);
}

void TextFormat::set_text_alignment(DWRITE_TEXT_ALIGNMENT value) const
{
    THROW_IF_FAILED(m_text_format->SetTextAlignment(value));
}

void TextFormat::set_paragraph_alignment(DWRITE_PARAGRAPH_ALIGNMENT value) const
{
    THROW_IF_FAILED(m_text_format->SetParagraphAlignment(value));
}

void TextFormat::set_word_wrapping(DWRITE_WORD_WRAPPING value) const
{
    THROW_IF_FAILED(m_text_format->SetWordWrapping(value));
}

void TextFormat::enable_trimming_sign()
{
    if (!m_trimming_sign) {
        THROW_IF_FAILED(m_factory->CreateEllipsisTrimmingSign(m_text_format.get(), &m_trimming_sign));
    }

    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    THROW_IF_FAILED(m_text_format->SetTrimming(&trimming, m_trimming_sign.get()));
}

void TextFormat::disable_trimming_sign() const
{
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0};
    THROW_IF_FAILED(m_text_format->SetTrimming(&trimming, nullptr));
}

int TextFormat::get_minimum_height(std::wstring_view text) const
{
    try {
        constexpr auto max_size = 65536.0f;
        const auto text_layout = create_text_layout(text, max_size, max_size);
        const auto metrics = text_layout.get_metrics();
        return gsl::narrow_cast<int>(metrics.height * get_default_scaling_factor() + 1);
    }
    CATCH_LOG()

    return 1;
}

TextPosition TextFormat::measure_text_position(std::wstring_view text, int height, float max_width) const
{
    try {
        const auto scaling_factor = get_default_scaling_factor();
        const auto text_layout = create_text_layout(text, max_width, static_cast<float>(height) / scaling_factor);
        const auto metrics = text_layout.get_metrics();
        const auto left = gsl::narrow_cast<int>(metrics.left * scaling_factor);
        const auto left_remainder_dip = metrics.left - gsl::narrow_cast<float>(left) / scaling_factor;

        return {left, left_remainder_dip, gsl::narrow_cast<int>((metrics.top) * scaling_factor),
            gsl::narrow_cast<int>(metrics.widthIncludingTrailingWhitespace * scaling_factor + 1),
            gsl::narrow_cast<int>(metrics.height * scaling_factor + 1)};
    }
    CATCH_LOG()

    return {};
}

int TextFormat::measure_text_width(std::wstring_view text) const
{
    try {
        const auto text_layout = create_text_layout(text, 65536.0f, 65536.0f);
        const auto metrics = text_layout.get_metrics();
        return gsl::narrow_cast<int>(metrics.widthIncludingTrailingWhitespace * get_default_scaling_factor() + 1);
    }
    CATCH_LOG()

    return 0;
}

TextLayout TextFormat::create_text_layout(std::wstring_view text, float max_width, float max_height) const
{
    const auto text_length = gsl::narrow<uint32_t>(text.length());

    const auto rendering_mode = m_rendering_params->rendering_mode();

    wil::com_ptr_t<IDWriteTextLayout> text_layout;
    if (rendering_mode == DWRITE_RENDERING_MODE_GDI_CLASSIC || rendering_mode == DWRITE_RENDERING_MODE_GDI_NATURAL)
        THROW_IF_FAILED(m_factory->CreateGdiCompatibleTextLayout(text.data(), text_length, m_text_format.get(),
            max_width, max_height, get_default_scaling_factor(), nullptr,
            rendering_mode == DWRITE_RENDERING_MODE_GDI_NATURAL, &text_layout));
    else
        THROW_IF_FAILED(m_factory->CreateTextLayout(
            text.data(), text_length, m_text_format.get(), max_width, max_height, &text_layout));

    const auto typography = m_context->get_default_typography();
    THROW_IF_FAILED(text_layout->SetTypography(typography.get(), {0, text_length}));

    return {m_factory, m_gdi_interop, text_layout, m_rendering_params};
}

Context::Context()
{
    THROW_IF_FAILED(
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1), m_factory.put_unknown()));
    THROW_IF_FAILED(m_factory->GetGdiInterop(&m_gdi_interop));
}

LOGFONT Context::create_log_font(const wil::com_ptr_t<IDWriteFont>& font) const
{
    LOGFONT log_font{};
    BOOL is_system_font{};
    THROW_IF_FAILED(m_gdi_interop->ConvertFontToLOGFONT(font.get(), &log_font, &is_system_font));
    return log_font;
}

wil::com_ptr_t<IDWriteFont> Context::create_font(const LOGFONT& log_font) const
{
    wil::com_ptr_t<IDWriteFont> font;
    THROW_IF_FAILED(m_gdi_interop->CreateFontFromLOGFONT(&log_font, &font));
    return font;
}

TextFormat Context::create_text_format(const wil::com_ptr_t<IDWriteFont>& font, float font_size)
{
    wil::com_ptr_t<IDWriteFontFamily> font_family;
    THROW_IF_FAILED(font->GetFontFamily(&font_family));

    return create_text_format(font_family, font->GetWeight(), font->GetStretch(), font->GetStyle(), font_size);
}

TextFormat Context::create_text_format(const wil::com_ptr_t<IDWriteFontFamily>& font_family, DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch, DWRITE_FONT_STYLE style, float font_size)
{
    wil::com_ptr_t<IDWriteLocalizedStrings> family_names;
    THROW_IF_FAILED(font_family->GetFamilyNames(&family_names));

    uint32_t length{};
    THROW_IF_FAILED(family_names->GetStringLength(0, &length));

    std::vector<wchar_t> family_name(length + 1, 0);

    THROW_IF_FAILED(family_names->GetString(0, family_name.data(), length + 1));

    return create_text_format(family_name.data(), weight, stretch, style, font_size);
}

TextFormat Context::create_text_format(const wchar_t* family_name, DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch, DWRITE_FONT_STYLE style, float font_size)
{
    wil::com_ptr_t<IDWriteTextFormat> text_format;
    THROW_IF_FAILED(
        m_factory->CreateTextFormat(family_name, NULL, weight, style, stretch, font_size, L"", &text_format));

    return wrap_text_format(text_format);
}

TextFormat Context::create_text_format(const LOGFONT& log_font, float font_size)
{
    const auto font = create_font(log_font);
    return create_text_format(font, font_size > 0 ? font_size : px_to_dip(20.0f));
}

std::optional<TextFormat> Context::create_text_format_with_fallback(
    const LOGFONT& log_font, std::optional<float> font_size) noexcept
{
    const auto resolved_size = font_size.value_or(-px_to_dip(gsl::narrow_cast<float>(log_font.lfHeight)));

    try {
        return create_text_format(log_font, resolved_size);
    }
    CATCH_LOG()

    try {
        return create_text_format(L"", static_cast<DWRITE_FONT_WEIGHT>(std::clamp(log_font.lfWeight, 1l, 999l)),
            DWRITE_FONT_STRETCH_NORMAL, log_font.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            resolved_size);
    }
    CATCH_LOG()

    return {};
}

TextFormat Context::wrap_text_format(wil::com_ptr_t<IDWriteTextFormat> text_format,
    DWRITE_RENDERING_MODE rendering_mode, bool force_greyscale_antialiasing, bool set_defaults)
{
    if (set_defaults) {
        THROW_IF_FAILED(text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
        THROW_IF_FAILED(text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    }

    if (auto text_format_2 = text_format.try_query<IDWriteTextFormat2>(); text_format_2) {
        DWRITE_LINE_SPACING spacing{DWRITE_LINE_SPACING_METHOD_DEFAULT, 0, 0, 0, DWRITE_FONT_LINE_GAP_USAGE_DISABLED};
        THROW_IF_FAILED(text_format_2->SetLineSpacing(&spacing));
    }

    const auto rendering_params
        = std::make_shared<RenderingParams>(m_factory, rendering_mode, force_greyscale_antialiasing);

    return {shared_from_this(), m_factory, m_gdi_interop, std::move(text_format), rendering_params};
}

wil::com_ptr_t<IDWriteTypography> Context::get_default_typography()
{
    if (!m_default_typography) {
        THROW_IF_FAILED(m_factory->CreateTypography(&m_default_typography));

        THROW_IF_FAILED(m_default_typography->AddFontFeature({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1}));
    }

    return m_default_typography;
}

std::optional<std::wstring> Context::get_face_name(
    const wchar_t* family_name, DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STRETCH stretch, DWRITE_FONT_STYLE style) const
{
    wil::com_ptr_t<IDWriteFontCollection> font_collection;
    try {
        THROW_IF_FAILED(m_factory->GetSystemFontCollection(&font_collection));

        BOOL exists{};
        uint32_t index{};
        THROW_IF_FAILED(font_collection->FindFamilyName(family_name, &index, &exists));

        if (!exists)
            return {};

        wil::com_ptr_t<IDWriteFontFamily> font_family;
        THROW_IF_FAILED(font_collection->GetFontFamily(index, &font_family));

        wil::com_ptr_t<IDWriteFont> font;
        THROW_IF_FAILED(font_family->GetFirstMatchingFont(weight, stretch, style, &font));

        wil::com_ptr_t<IDWriteLocalizedStrings> face_names;
        font->GetFaceNames(&face_names);

        return get_localised_string(face_names);
    } catch (...) {
        LOG_CAUGHT_EXCEPTION();
        return {};
    }
}

std::vector<Font> FontFamily::fonts() const
{
    std::vector<Font> fonts;

    auto count = family->GetFontCount();

    for (auto index : std::ranges::views::iota(0u, count)) {
        wil::com_ptr_t<IDWriteFont> font;
        THROW_IF_FAILED(family->GetFont(index, &font));

        wil::com_ptr_t<IDWriteLocalizedStrings> localised_names;
        THROW_IF_FAILED(font->GetFaceNames(&localised_names));
        fonts.emplace_back(std::move(font), get_localised_string(localised_names));
    }

    return fonts;
}

std::vector<FontFamily> Context::get_font_families() const
{
    std::vector<FontFamily> families;

    wil::com_ptr_t<IDWriteFontCollection> font_collection;
    THROW_IF_FAILED(m_factory->GetSystemFontCollection(&font_collection));

    const auto family_count = font_collection->GetFontFamilyCount();

    for (auto index : std::ranges::views::iota(0u, family_count)) {
        wil::com_ptr_t<IDWriteFontFamily> family;
        THROW_IF_FAILED(font_collection->GetFontFamily(index, &family));

        wil::com_ptr_t<IDWriteLocalizedStrings> family_localised_names;
        THROW_IF_FAILED(family->GetFamilyNames(&family_localised_names));

        std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale_name;
        uint32_t locale_index{};
        BOOL exists{};

        if (GetUserDefaultLocaleName(locale_name.data(), LOCALE_NAME_MAX_LENGTH))
            THROW_IF_FAILED(family_localised_names->FindLocaleName(locale_name.data(), &locale_index, &exists));

        if (!exists)
            THROW_IF_FAILED(family_localised_names->FindLocaleName(L"en-us", &locale_index, &exists));

        if (!exists)
            locale_index = 0;

        uint32_t name_length{};
        THROW_IF_FAILED(family_localised_names->GetStringLength(locale_index, &name_length));

        std::wstring localised_name;
        localised_name.resize(name_length);

        THROW_IF_FAILED(family_localised_names->GetString(locale_index, localised_name.data(), name_length + 1));

        wil::com_ptr_t<IDWriteFont> first_font;
        THROW_IF_FAILED(family->GetFont(0, &first_font));

        const auto is_symbol_font = first_font->IsSymbolFont() != 0;

        families.emplace_back(std::move(family), std::move(localised_name), is_symbol_font);
    }

    mmh::in_place_sort(
        families,
        [](auto&& left, auto&& right) {
            return StrCmpLogicalW(left.localised_name.c_str(), right.localised_name.c_str());
        },
        false);

    return families;
}

std::wstring get_localised_string(const wil::com_ptr_t<IDWriteLocalizedStrings>& localised_strings)
{
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale_name;
    uint32_t locale_index{};
    BOOL exists{};

    if (GetUserDefaultLocaleName(locale_name.data(), LOCALE_NAME_MAX_LENGTH))
        THROW_IF_FAILED(localised_strings->FindLocaleName(locale_name.data(), &locale_index, &exists));

    if (!exists)
        THROW_IF_FAILED(localised_strings->FindLocaleName(L"en-us", &locale_index, &exists));

    if (!exists)
        locale_index = 0;

    uint32_t name_length{};
    THROW_IF_FAILED(localised_strings->GetStringLength(locale_index, &name_length));

    std::wstring localised_string;
    localised_string.resize(name_length);

    THROW_IF_FAILED(localised_strings->GetString(locale_index, localised_string.data(), name_length + 1));

    return localised_string;
}

float get_default_scaling_factor()
{
    return gsl::narrow_cast<float>(get_system_dpi_cached().cx) / gsl::narrow_cast<float>(USER_DEFAULT_SCREEN_DPI);
}

float pt_to_dip(float point_size)
{
    return point_size * gsl::narrow_cast<float>(USER_DEFAULT_SCREEN_DPI) / 72.0f;
}

float px_to_dip(float px, float scaling_factor)
{
    return px / scaling_factor;
}

float dip_to_px(float dip, float scaling_factor)
{
    return dip * scaling_factor;
}

} // namespace uih::direct_write
