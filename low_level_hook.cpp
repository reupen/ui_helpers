#include "stdafx.h"

namespace uih {

    class MainThreadCallback : public main_thread_callback {
    public:
        MainThreadCallback(WPARAM msg, const MSLLHOOKSTRUCT& mllhs)
            : m_msg(msg), m_mllhs(mllhs) {}
    private:
        void callback_run() override
        {
            LowLevelMouseHookManager::s_get_instance().on_event(m_msg, m_mllhs);
        }

        WPARAM m_msg;
        MSLLHOOKSTRUCT m_mllhs;
    };

    class LowLevelMouseHookManager::HookThread : public pfc::thread {
    public:
        HookThread();
        ~HookThread();

    private:
        static constexpr unsigned MSG_QUIT = WM_USER + 3;
        static LRESULT CALLBACK s_on_event(int code, WPARAM wp, LPARAM lp);

        void threadProc() override;
        HHOOK m_hook;
        DWORD m_thread_id;
    };

    LowLevelMouseHookManager::HookThread::HookThread()
        : m_hook(nullptr), m_thread_id(0)
    {
        winStart(GetThreadPriority(GetCurrentThread()), &m_thread_id);
    }

    LowLevelMouseHookManager::HookThread::~HookThread()
    {
        if (!PostThreadMessage(m_thread_id, MSG_QUIT, 0, 0))
            uBugCheck();
        waitTillDone();
    }

    LRESULT LowLevelMouseHookManager::HookThread::s_on_event(int code, WPARAM wp, LPARAM lp)
    {
        if (code >= 0) {
            MainThreadCallback::ptr callback = new service_impl_t<MainThreadCallback>(wp, *reinterpret_cast<LPMSLLHOOKSTRUCT>(lp));
            callback->callback_enqueue();
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }

    void LowLevelMouseHookManager::HookThread::threadProc()
    {
        TRACK_CALL_TEXT("LowLevelMouseHookManager::HookThread::threadProc");
        MSG msg;
        BOOL res;

        m_hook = SetWindowsHookEx(WH_MOUSE_LL, &s_on_event, core_api::get_my_instance(), NULL);

        while ((res = GetMessage(&msg, nullptr, 0, 0)) != 0) {
            if (res == -1) {
                PFC_ASSERT(!"GetMessage failure");
                uBugCheck();
            } else if (msg.hwnd == nullptr && msg.message == MSG_QUIT) {
                PostQuitMessage(0);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }

    LowLevelMouseHookManager& LowLevelMouseHookManager::s_get_instance()
    {
        static LowLevelMouseHookManager instance;
        return instance;
    }

    void LowLevelMouseHookManager::register_hook(HookCallback* callback)
    {
        m_callbacks.push_back(callback);
        if (m_callbacks.size() == 1) {
            m_hook_thread = std::make_unique<HookThread>();
        }
    }

    void LowLevelMouseHookManager::deregister_hook(HookCallback* callback)
    {
        m_callbacks.erase(std::remove(m_callbacks.begin(), m_callbacks.end(), callback), m_callbacks.end());
        if (m_callbacks.size() == 0) {
            m_hook_thread.reset();
        }
    }

    LowLevelMouseHookManager::LowLevelMouseHookManager()
        : m_hook(nullptr) {}

    LowLevelMouseHookManager::~LowLevelMouseHookManager()
    {
        PFC_ASSERT(m_callbacks.size() == 0);
    }

    void LowLevelMouseHookManager::on_event(WPARAM msg, const MSLLHOOKSTRUCT& mllhs)
    {
        for (auto& callback : m_callbacks)
            callback->on_hooked_message(msg, mllhs);
    }

}
