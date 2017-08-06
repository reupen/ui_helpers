#pragma once

#define DDWM_UPDATEWINDOW (WM_USER+3)

namespace uih::ole {
    CLIPFORMAT get_clipboard_format_drop_description();
    HRESULT set_blob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob);
    HRESULT set_drop_description(IDataObject *pdtobj, DROPIMAGETYPE dit, const char * msg, const char * insert);
    HRESULT set_using_default_drag_image(IDataObject *pdtobj, BOOL value = TRUE);
    HRESULT set_is_showing_text(IDataObject *pdtobj, BOOL value = TRUE);
    HRESULT set_disable_drag_text(IDataObject *pdtobj, BOOL value);
    HRESULT set_is_computing_image(IDataObject *pdtobj, BOOL value = TRUE);
    HRESULT do_drag_drop(HWND wnd, WPARAM initialKeyState, IDataObject *pDataObject, DWORD permittedEffects, DWORD preferredEffect, DWORD *pdwEffect, SHDRAGIMAGE * lpsdi = nullptr);

    class IDropSourceGeneric : public IDropSource {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;
        HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
        HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect) override;
        IDropSourceGeneric(HWND wnd, IDataObject * pDataObj, DWORD initial_key_state, bool b_allowdropdescriptiontext = true, SHDRAGIMAGE * lpsdi = nullptr); //careful, some fb2k versions have broken IDataObject
    private:
        long m_refcount;
        DWORD m_initial_key_state;
        bool m_prev_is_showing_layered;
        mmh::ComPtr<IDragSourceHelper> m_drag_source_helper;
        mmh::ComPtr<IDataObject> m_data_object;
    };

}
