#pragma once

namespace uih {

struct MenuItemOptions {
    bool is_default{};
    bool is_disabled{};
    bool is_checked{};
    bool is_radio_checked{};
};

class Menu {
public:
    Menu() { m_menu.reset(CreatePopupMenu()); }

    HMENU get() const { return m_menu.get(); }

    HMENU detach() { return m_menu.release(); }

    uint32_t size() const
    {
        const int size = GetMenuItemCount(m_menu.get());
        LOG_LAST_ERROR_IF(size == -1);

        if (size < 0)
            return 0;

        return gsl::narrow<uint32_t>(size);
    }

    void insert_command(uint32_t index, uint32_t id, wil::zwstring_view text, MenuItemOptions opts = {}) const
    {
        MENUITEMINFO mii{};
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE;
        mii.dwTypeData = const_cast<wchar_t*>(text.c_str());
        mii.cch = gsl::narrow<uint32_t>(text.size());
        mii.fType = MFT_STRING;
        mii.fState = MFS_ENABLED;

        if (opts.is_radio_checked)
            mii.fType |= MFT_RADIOCHECK;

        if (opts.is_checked || opts.is_radio_checked)
            mii.fState |= MFS_CHECKED;

        if (opts.is_default)
            mii.fState |= MFS_DEFAULT;

        if (opts.is_disabled)
            mii.fState |= MFS_DISABLED;

        mii.wID = id;

        LOG_IF_WIN32_BOOL_FALSE(InsertMenuItem(m_menu.get(), index, TRUE, &mii));
    }

    void insert_submenu(uint32_t index, HMENU submenu, wil::zwstring_view text) const
    {
        MENUITEMINFO mii{};
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_FTYPE;
        mii.dwTypeData = const_cast<wchar_t*>(text.c_str());
        mii.cch = gsl::narrow<uint32_t>(text.size());
        mii.fType = MFT_STRING;
        mii.fState = MFS_ENABLED;
        mii.hSubMenu = submenu;
        LOG_IF_WIN32_BOOL_FALSE(InsertMenuItem(m_menu.get(), index, TRUE, &mii));
    }

    void insert_submenu(uint32_t index, Menu&& submenu, wil::zwstring_view text) const
    {
        insert_submenu(index, submenu.detach(), text);
    }

    void insert_separator(uint32_t index) const
    {
        MENUITEMINFO mii{};
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_FTYPE;
        mii.fType = MFT_SEPARATOR;
        LOG_IF_WIN32_BOOL_FALSE(InsertMenuItem(m_menu.get(), index, TRUE, &mii));
    }

    void append_command(uint32_t id, wil::zwstring_view text, MenuItemOptions opts = {}) const
    {
        insert_command(UINT32_MAX, id, text, opts);
    }

    void append_submenu(HMENU submenu, wil::zwstring_view text) const { insert_submenu(UINT32_MAX, submenu, text); }

    void append_submenu(Menu&& submenu, wil::zwstring_view text) const { append_submenu(submenu.detach(), text); }

    void append_separator() const { insert_separator(UINT32_MAX); }

    auto run(HWND wnd, POINT pt) const
    {
        return gsl::narrow_cast<uint32_t>(
            TrackPopupMenu(m_menu.get(), TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, wnd, nullptr));
    }

private:
    wil::unique_hmenu m_menu;
};

class MenuCommandCollector {
public:
    uint32_t add(std::function<void()> command)
    {
        m_commands.emplace_back(std::move(command));
        return gsl::narrow<uint32_t>(m_commands.size());
    }

    void execute(uint32_t index) const
    {
        assert(index <= m_commands.size());

        if (1 <= index && index <= m_commands.size())
            m_commands[index - 1]();
    }

private:
    std::vector<std::function<void()>> m_commands;
};

} // namespace uih
