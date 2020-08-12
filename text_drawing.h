#pragma once

namespace uih {

class UniscribeTextRenderer {
public:
    UniscribeTextRenderer() { initialise(); }

    ~UniscribeTextRenderer() { cleanup(); }

    UniscribeTextRenderer(HDC dc, const wchar_t* p_str, size_t p_str_len, int max_cx, bool b_clip)
    {
        initialise();
        analyse(dc, p_str, p_str_len, max_cx, b_clip);
    }

    void analyse(HDC dc, const wchar_t* p_str, size_t p_str_len, int max_cx, bool b_clip)
    {
        if (m_ssa) {
            cleanup();
            initialise();
        }
        m_string_length = p_str_len;
        if (m_string_length)
            ScriptStringAnalyse(dc, p_str, p_str_len, NULL, -1,
                SSA_FALLBACK | SSA_GLYPHS | SSA_LINK | (b_clip ? SSA_CLIP : NULL), max_cx, &m_sc, &m_ss, nullptr,
                nullptr, nullptr, &m_ssa);
    }

    void text_out(int x, int y, UINT flags, const RECT* p_rc)
    {
        if (m_ssa)
            ScriptStringOut(m_ssa, x, y, flags, p_rc, 0, 0, FALSE);
    }

    int get_output_character_count()
    {
        const int* len = m_ssa ? ScriptString_pcOutChars(m_ssa) : nullptr; // b_clip = true only
        return len ? *len : m_string_length;
    }

    void get_output_size(SIZE& p_sz)
    {
        const SIZE* sz = m_ssa ? ScriptString_pSize(m_ssa) : nullptr;
        if (sz)
            p_sz = *sz;
        else {
            p_sz.cx = (p_sz.cy = 0);
        }
    }

    int get_output_width()
    {
        SIZE sz;
        get_output_size(sz);
        return sz.cx;
    }

    long get_font_height(HDC dc)
    {
        long height = 0;
        ScriptCacheGetHeight(dc, &m_scache, &height);
        return height;
    }

    void get_character_logical_widths(int* p_array_out)
    {
        if (m_ssa)
            ScriptStringGetLogicalWidths(m_ssa, p_array_out);
    }

    void get_character_logical_extents(int* p_array_out, int offset = 0) // NB RTL !
    {
        get_character_logical_widths(p_array_out);
        int i;
        int count = get_output_character_count();
        for (i = 1; i < count; i++) {
            p_array_out[i] += p_array_out[i - 1];
        }
        if (offset)
            for (i = 0; i < count; i++)
                p_array_out[i] += offset;
    }

private:
    void initialise()
    {
        m_scache = nullptr;
        m_ssa = nullptr;
        m_string_length = NULL;
        memset(&m_sc, 0, sizeof(m_sc));
        memset(&m_ss, 0, sizeof(m_ss));

        if (!m_sdg_valid) {
            ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &m_sdg);
            m_sdg_valid = true;
        }
        ScriptApplyDigitSubstitution(&m_sdg, &m_sc, &m_ss);
    }

    void cleanup()
    {
        if (m_ssa) {
            ScriptStringFree(&m_ssa);
            m_ssa = nullptr;
        }
        if (m_scache) {
            ScriptFreeCache(&m_scache);
            m_scache = nullptr;
        }
    }

    SCRIPT_STRING_ANALYSIS m_ssa;
    SCRIPT_CONTROL m_sc;
    SCRIPT_STATE m_ss;
    SCRIPT_CACHE m_scache;
    size_t m_string_length;

    static SCRIPT_DIGITSUBSTITUTE m_sdg;
    static bool m_sdg_valid;
};

class CharacterExtentsCalculator {
    pfc::string8_fast_aggressive temp;
    pfc::stringcvt::string_wide_from_utf8_fast text_w;
    pfc::stringcvt::string_utf8_from_wide w_utf8;

public:
    CharacterExtentsCalculator() { temp.prealloc(64); }
    BOOL run(HDC dc, const char* text, int length, int max_width, LPINT max_chars, LPINT width_array, LPSIZE sz,
        unsigned* width_out, bool trunc);
};

enum alignment {
    ALIGN_LEFT,
    ALIGN_CENTRE,
    ALIGN_RIGHT,
};

bool is_rect_null_or_reversed(const RECT* r);
void get_text_size(HDC dc, const char* src, int len, SIZE& sz);
int get_text_width(HDC dc, const char* src, int len);
int get_text_width_colour(HDC dc, const char* src, int len, bool b_ignore_tabs = false);
unsigned get_text_truncate_point(const char* src, unsigned len);
BOOL text_out_colours_ellipsis(HDC dc, const char* src, int len, int x_offset, int pos_y, const RECT* base_clip,
    bool selected, bool show_ellipsis, DWORD default_color, alignment align, unsigned* p_width = nullptr,
    bool b_set_default_colours = true, int* p_position = nullptr);
BOOL text_out_colours_tab(HDC dc, const char* display, int display_len, int left_offset, int border,
    const RECT* base_clip, bool selected, DWORD default_color, bool enable_tab_columns, bool show_ellipsis,
    alignment align, unsigned* p_width = nullptr, bool b_set_default_colours = true,
    bool b_vertical_align_centre = true, int* p_position = nullptr);

void remove_color_marks(const char* src, pfc::string_base& out, t_size len = pfc_infinite);
} // namespace uih
