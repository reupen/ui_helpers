#pragma once

template <typename t_handle, class t_release>
class handle_container_t {
    using t_self = handle_container_t<t_handle, t_release>;

public:
    void release()
    {
        if (is_valid()) {
            t_release::release(m_handle);
            t_release::set_invalid(m_handle);
        }
    }
    t_handle set(t_handle value)
    {
        release();
        return (m_handle = value);
    }
    t_handle get() const { return m_handle; }
    operator t_handle() const { return get(); }
    t_handle operator=(t_handle value) { return set(value); }
    t_self& operator=(const t_self& value) = delete;
    t_self& operator=(t_self&& value)
    {
        set(value.detach());
        return *this;
    }
    t_handle detach()
    {
        t_handle ret = m_handle;
        t_release::set_invalid(m_handle);
        return ret;
    }
    bool is_valid() const { return t_release::is_valid(m_handle); }

    handle_container_t() { t_release::set_invalid(m_handle); }
    handle_container_t(t_handle value) : m_handle(value) {};
    handle_container_t(t_self&& value) { set(value.detach()); }
    handle_container_t(const t_self& value) = delete;

    ~handle_container_t() { release(); }

private:
    t_handle m_handle;
};

class icon_release_t {
public:
    static void release(HICON handle) { DestroyIcon(handle); }
    static bool is_valid(HICON handle) { return handle != nullptr; }
    static void set_invalid(HICON& handle) { handle = nullptr; }
};
using icon_ptr = handle_container_t<HICON, icon_release_t>;

namespace win32 {
class __handle_release_t {
public:
    static void release(HANDLE handle) { CloseHandle(handle); }
    static bool is_valid(HANDLE handle) { return handle != INVALID_HANDLE_VALUE; }
    static void set_invalid(HANDLE& handle) { handle = INVALID_HANDLE_VALUE; }
};

using handle_ptr_t = handle_container_t<HANDLE, __handle_release_t>;

} // namespace win32
