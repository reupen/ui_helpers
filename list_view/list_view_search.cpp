#include "stdafx.h"

#include "list_view.h"

using namespace wil::literals;
using namespace uih::literals::spx;

namespace uih::lv {

namespace {

struct ToolbarButtonConfig {
    SearchBarIconId icon_id{};
    int command_id{};
    wil::zwstring_view label;
};

struct CreateToolbarResult {
    wil::unique_hwnd wnd;
    int width{};
    int height{};
};

constexpr std::array left_commands{
    ToolbarButtonConfig{SearchBarIconId::Up, ID_SEARCH_BAR_UP, L"Previous result"_zv},
    ToolbarButtonConfig{SearchBarIconId::Down, ID_SEARCH_BAR_DOWN, L"Next result"_zv},
};

constexpr std::array right_commands{
    ToolbarButtonConfig{SearchBarIconId::Close, ID_SEARCH_BAR_CLOSE, L"Close search bar"_zv},
};

template <size_t button_count>
wil::unique_himagelist create_toolbar_imagelist(const SearchBarHost::Ptr& host,
    const std::array<ToolbarButtonConfig, button_count>& buttons, int width, int height, bool is_dark)
{
    wil::unique_himagelist imagelist(ImageList_Create(width, height, ILC_COLOR32, 0, 0));
    ImageList_SetImageCount(imagelist.get(), gsl::narrow<int>(buttons.size()));

    for (auto&& [index, button] : ranges::views::enumerate(buttons)) {
        const auto icon = host->create_icon(button.icon_id, width, height, is_dark);

        if (std::holds_alternative<wil::unique_hbitmap>(icon))
            ImageList_Replace(
                imagelist.get(), gsl::narrow<int>(index), std::get<wil::unique_hbitmap>(icon).get(), nullptr);
        else
            ImageList_ReplaceIcon(imagelist.get(), gsl::narrow<int>(index), std::get<wil::unique_hicon>(icon).get());
    }

    return imagelist;
}

bool is_imagelist_same_size(HIMAGELIST imagelist, int width, int height)
{
    int current_width{};
    int current_height{};
    ImageList_GetIconSize(imagelist, &current_width, &current_height);
    return current_width == width && current_height == height;
}

template <size_t button_count>
CreateToolbarResult create_toolbar(HWND parent_wnd, uint32_t ctrl_id, HIMAGELIST imagelist,
    const std::array<ToolbarButtonConfig, button_count>& buttons, bool is_dark)
{
    wil::unique_hwnd wnd(CreateWindowEx(WS_EX_TOOLWINDOW, TOOLBARCLASSNAME, nullptr,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TRANSPARENT
            | TBSTYLE_TOOLTIPS | CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER,
        0, 0, 0, 0, parent_wnd, reinterpret_cast<HMENU>(static_cast<size_t>(ctrl_id)), wil::GetModuleInstanceHandle(),
        nullptr));

    SetWindowTheme(wnd.get(), is_dark ? L"DarkMode" : nullptr, nullptr);

    const unsigned icon_width = GetSystemMetrics(SM_CXSMICON);
    const unsigned icon_height = GetSystemMetrics(SM_CYSMICON);

    std::array<TBBUTTON, button_count> tbbuttons{};

    for (auto&& [index, command, tbbutton] : ranges::views::zip(ranges::views::iota(int{}), buttons, tbbuttons)) {
        tbbutton.iBitmap = index;
        tbbutton.idCommand = command.command_id;
        tbbutton.fsState = TBSTATE_ENABLED;
        tbbutton.fsStyle = BTNS_AUTOSIZE | BTNS_BUTTON;
        tbbutton.iString = reinterpret_cast<INT_PTR>(command.label.c_str());
    }

    SendMessage(wnd.get(), TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
    SendMessage(wnd.get(), TB_SETBITMAPSIZE, 0, MAKELONG(icon_width, icon_height));

    SendMessage(wnd.get(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(imagelist));

    SendMessage(wnd.get(), TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(wnd.get(), TB_ADDBUTTONS, tbbuttons.size(), reinterpret_cast<LPARAM>(tbbuttons.data()));

    SendMessage(wnd.get(), TB_AUTOSIZE, 0, 0);

    RECT last_button_rect{};
    SendMessage(wnd.get(), TB_GETITEMRECT, button_count - 1, reinterpret_cast<LPARAM>(&last_button_rect));

    return {std::move(wnd), last_button_rect.right, last_button_rect.bottom};
}

void create_toolbar_and_imagelist(SearchBarToolbarState& toolbar, auto&& commands, HWND parent_wnd,
    const SearchBarHost::Ptr& host, uint32_t control_id, int icon_width, int icon_height, bool is_dark)
{
    if (toolbar.wnd)
        return;

    if (!toolbar.imagelist || !is_imagelist_same_size(toolbar.imagelist.get(), icon_width, icon_height))
        toolbar.imagelist = create_toolbar_imagelist(
            host, std::forward<decltype(commands)>(commands), icon_width, icon_height, is_dark);

    auto result = create_toolbar(
        parent_wnd, control_id, toolbar.imagelist.get(), std::forward<decltype(commands)>(commands), is_dark);

    toolbar.wnd = std::move(result.wnd);
    toolbar.width = result.width;
    toolbar.height = result.height;
}

} // namespace

void SearchBar::create(HWND parent_wnd, const char* label, HFONT font, int item_height, bool is_dark)
{
    if (m_edit_control)
        return;

    m_parent_wnd = parent_wnd;
    m_is_dark = is_dark;

    m_edit_control.reset(CreateWindowEx(0, WC_EDIT, L"",
        WS_CHILD | WS_CLIPSIBLINGS | ES_LEFT | WS_CLIPCHILDREN | ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 0, 0, 0, 0,
        parent_wnd, reinterpret_cast<HMENU>(IDC_SEARCH_BAR_EDIT), wil::GetModuleInstanceHandle(), nullptr));

    if (!m_edit_control)
        return;

    enhance_edit_control(m_edit_control.get());

    subclass_window(
        m_edit_control.get(), [&](auto wnd_proc, auto wnd, auto msg, auto wp, auto lp) -> std::optional<LRESULT> {
            switch (msg) {
            case WM_KILLFOCUS:
                m_on_kill_focus_func(reinterpret_cast<HWND>(wp));
                break;
            case WM_SETFOCUS:
                m_on_set_focus_func(reinterpret_cast<HWND>(wp));
                break;
            case WM_KEYDOWN:
                m_ignore_next_wm_char_message = false;

                switch (wp) {
                case VK_TAB:
                    uih::handle_tab_down(wnd);
                    return 0;
                case VK_ESCAPE:
                    destroy();
                    return 0;
                case VK_UP:
                    m_search_bar_host->on_previous();
                    return 0;
                case VK_DOWN:
                    m_search_bar_host->on_next();
                    return 0;
                case VK_F3:
                    if (GetKeyState(VK_SHIFT) & 0x8000)
                        m_search_bar_host->on_previous();
                    else
                        m_search_bar_host->on_next();
                    return 0;
                case VK_RETURN:
                    m_ignore_next_wm_char_message = true;
                    m_search_bar_host->on_return();
                    return 0;
                default:
                    if (m_search_bar_host->on_keydown(wp)) {
                        m_ignore_next_wm_char_message = true;
                        return 0;
                    }
                    break;
                }
                break;
            case WM_SYSKEYDOWN:
                if ((m_ignore_next_wm_syschar_message = m_search_bar_host->on_keydown(wp)))
                    return 0;
                break;
            case WM_CHAR:
                if (m_ignore_next_wm_char_message) {
                    m_ignore_next_wm_char_message = false;
                    return 0;
                }
                return {};
            case WM_SYSCHAR:
                if (m_ignore_next_wm_syschar_message) {
                    m_ignore_next_wm_syschar_message = false;
                    return 0;
                }
                return {};
            }
            return {};
        });

    set_font(font);
    SetWindowTheme(m_edit_control.get(), is_dark ? L"DarkMode_Explorer" : nullptr, nullptr);

    Edit_SetCueBannerTextFocused(m_edit_control.get(), mmh::to_utf16(label).c_str(), true);

    focus();

    const unsigned icon_width = GetSystemMetrics(SM_CXSMICON);
    const unsigned icon_height = GetSystemMetrics(SM_CYSMICON);

    create_toolbar_and_imagelist(m_right_toolbar, right_commands, parent_wnd, m_search_bar_host,
        IDC_SEARCH_BAR_RIGHT_TOOLBAR, icon_width, icon_height, is_dark);

    create_toolbar_and_imagelist(m_left_toolbar, left_commands, parent_wnd, m_search_bar_host,
        IDC_SEARCH_BAR_LEFT_TOOLBAR, icon_width, icon_height, is_dark);

    invalidate();

    if (!m_last_string.empty()) {
        SetWindowText(m_edit_control.get(), m_last_string.c_str());
        select_all();
    }

    ShowWindow(m_edit_control.get(), SW_SHOWNORMAL);

    if (m_left_toolbar.wnd)
        ShowWindow(m_left_toolbar.wnd.get(), SW_SHOWNORMAL);

    if (m_right_toolbar.wnd)
        ShowWindow(m_right_toolbar.wnd.get(), SW_SHOWNORMAL);
}

void SearchBar::destroy()
{
    if (!m_edit_control)
        return;

    invalidate();

    m_edit_control.reset();
    m_left_toolbar.wnd.reset();
    m_right_toolbar.wnd.reset();
    m_search_bar_host->on_close();

    m_on_destroy_func();
}

void SearchBar::shut_down()
{
    m_edit_control.reset();
    m_left_toolbar.wnd.reset();
    m_right_toolbar.wnd.reset();
    m_left_toolbar.imagelist.reset();
    m_right_toolbar.imagelist.reset();
    m_on_destroy_func = {};
}

SearchBar::Metrics SearchBar::get_metrics() const
{
    if (!m_edit_control)
        return {};

    RECT rect{};
    GetWindowRect(m_edit_control.get(), &rect);

    const auto edit_height = static_cast<int>(wil::rect_height(rect));
    const auto edit_width = static_cast<int>(wil::rect_width(rect));

    const auto total_height = std::max(edit_height, m_left_toolbar.height) + get_vertical_padding() * 2;

    return {edit_width, edit_height, total_height};
}

int SearchBar::get_total_height() const
{
    return get_metrics().total_height;
}

void SearchBar::reposition() const
{
    RECT client_rect{};
    GetClientRect(m_parent_wnd, &client_rect);
    reposition(wil::rect_width(client_rect), wil::rect_height(client_rect));
}

void SearchBar::reposition(int client_width, int client_height) const
{
    const auto vertical_padding = get_vertical_padding();
    const auto horizontal_padding = get_horizontal_padding();
    const auto max_edit_width = 300_spx;
    const auto edit_width = std::clamp(
        client_width - 2 * horizontal_padding - m_left_toolbar.width - m_right_toolbar.width, 0, max_edit_width);
    const auto total_height = std::max(m_edit_height, m_right_toolbar.height) + vertical_padding * 2;
    bool should_invalidate{};

    if (m_edit_control) {
        RECT old_edit_rect{};
        GetWindowRect(m_edit_control.get(), &old_edit_rect);

        SetWindowPos(m_edit_control.get(), nullptr, 0, client_height - (total_height + m_edit_height) / 2, edit_width,
            m_edit_height, SWP_NOZORDER);

        should_invalidate = wil::rect_width(old_edit_rect) != edit_width;
    }

    auto reposition_toolbar = [&](const SearchBarToolbarState& toolbar, int x) {
        if (toolbar.wnd)
            SetWindowPos(toolbar.wnd.get(), nullptr, x, client_height - (total_height + toolbar.height) / 2,
                toolbar.width, toolbar.height, SWP_NOZORDER);
    };

    reposition_toolbar(m_left_toolbar, horizontal_padding + edit_width);
    reposition_toolbar(m_right_toolbar, client_width - m_right_toolbar.width);

    if (should_invalidate)
        invalidate();
}

void SearchBar::on_string_change()
{
    const auto new_string = get_window_text(m_edit_control.get());

    if (new_string.size() == m_last_string.size() + 1
        && wcsncmp(m_last_string.c_str(), new_string.c_str(), m_last_string.size()) == 0)
        m_search_bar_host->on_char(gsl::narrow<wchar_t>(new_string[new_string.size() - 1]));
    else
        m_search_bar_host->on_string_replaced(new_string.c_str());

    m_last_string = new_string;
}

void SearchBar::set_use_dark_mode(bool is_dark)
{
    if (m_is_dark == is_dark)
        return;

    m_is_dark = is_dark;

    if (m_edit_control)
        SetWindowTheme(m_edit_control.get(), is_dark ? L"DarkMode_Explorer" : nullptr, nullptr);

    m_left_toolbar.imagelist.reset();
    m_right_toolbar.imagelist.reset();

    const unsigned cx = GetSystemMetrics(SM_CXSMICON);
    const unsigned cy = GetSystemMetrics(SM_CYSMICON);

    auto set_toolbar_dark_mode = [&](SearchBarToolbarState& toolbar, auto&& commands) {
        toolbar.imagelist
            = create_toolbar_imagelist(m_search_bar_host, std::forward<decltype(commands)>(commands), cx, cy, is_dark);
        SendMessage(toolbar.wnd.get(), TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(toolbar.imagelist.get()));
        SetWindowTheme(toolbar.wnd.get(), is_dark ? L"DarkMode" : nullptr, nullptr);

        if (const auto tooltip_wnd = reinterpret_cast<HWND>(SendMessage(toolbar.wnd.get(), TB_GETTOOLTIPS, 0, 0)))
            SetWindowTheme(tooltip_wnd, is_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    };

    if (m_left_toolbar.wnd)
        set_toolbar_dark_mode(m_left_toolbar, left_commands);

    if (m_right_toolbar.wnd)
        set_toolbar_dark_mode(m_right_toolbar, right_commands);
}

void SearchBar::set_font(HFONT font)
{
    if (!m_edit_control.get())
        return;

    m_edit_height = get_font_height(font) + 2 * 3_spx;
    SetWindowFont(m_edit_control.get(), font, FALSE);
}

void SearchBar::set_results_text(std::wstring_view new_text)
{
    m_results_text = new_text;
    invalidate();
}

void SearchBar::invalidate() const
{
    RECT invalidate_rect{};
    GetClientRect(m_parent_wnd, &invalidate_rect);
    invalidate_rect.top = invalidate_rect.bottom - get_total_height();

    RedrawWindow(m_parent_wnd, &invalidate_rect, nullptr, RDW_INVALIDATE);
}

RECT SearchBar::render(HDC dc, int items_width, int items_bottom, const ColourData& colours,
    const std::optional<direct_write::TextFormat>& text_format)
{
    const auto metrics = get_metrics();

    RECT search_area_rect = RECT{0, items_bottom, items_width, items_bottom + metrics.total_height};

    if (!RectVisible(dc, &search_area_rect))
        return search_area_rect;

    SetDCBrushColor(dc, m_is_dark ? colours.m_background : GetSysColor(COLOR_BTNFACE));
    PatBlt(dc, search_area_rect.left, search_area_rect.top, wil::rect_width(search_area_rect), metrics.total_height,
        PATCOPY);

    if (!text_format)
        return search_area_rect;

    const auto horizontal_padding = get_horizontal_padding();

    const auto text_available_width
        = items_width - metrics.edit_width - m_left_toolbar.width - m_right_toolbar.width - horizontal_padding * 3;

    if (text_available_width <= 0)
        return search_area_rect;

    const auto text_layout = text_format->create_text_layout(m_results_text,
        direct_write::px_to_dip(static_cast<float>(text_available_width)),
        direct_write::px_to_dip(static_cast<float>(metrics.total_height)), true);

    const auto text_colour = m_is_dark ? colours.m_text : GetSysColor(COLOR_BTNTEXT);

    const auto left = metrics.edit_width + m_left_toolbar.width + horizontal_padding * 2;
    RECT text_rect{left, items_bottom, left + text_available_width, items_bottom + metrics.total_height};

    text_layout.render_with_transparent_background(m_parent_wnd, dc, text_rect, text_colour);

    return search_area_rect;
}

int SearchBar::get_horizontal_padding()
{
    return 4_spx;
}

int SearchBar::get_vertical_padding()
{
    return 2_spx;
}

} // namespace uih::lv
