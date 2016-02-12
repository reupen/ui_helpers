#pragma once

#define DDWM_UPDATEWINDOW (WM_USER+3)

namespace mmh {
	namespace ole {

		CLIPFORMAT ClipboardFormatDropDescription();
		HRESULT SetBlob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob);
		HRESULT SetDropDescription(IDataObject *pdtobj, DROPIMAGETYPE dit, const char * msg, const char * insert);
		HRESULT SetUsingDefaultDragImage(IDataObject *pdtobj, BOOL value = TRUE);
		HRESULT SetIsShowingText(IDataObject *pdtobj, BOOL value = TRUE);
		HRESULT SetDisableDragText(IDataObject *pdtobj, BOOL value);
		HRESULT SetIsComputingImage(IDataObject *pdtobj, BOOL value = TRUE);
		HRESULT DoDragDrop(HWND wnd, WPARAM initialKeyState, IDataObject *pDataObject, DWORD permittedEffects, DWORD preferredEffect, DWORD *pdwEffect);

		class IDropSource_Generic : public IDropSource {
		public:
			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
			ULONG STDMETHODCALLTYPE AddRef();
			ULONG STDMETHODCALLTYPE Release();
			HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState);
			HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect);
			IDropSource_Generic(HWND wnd, IDataObject * pDataObj, DWORD initial_key_state, bool b_allowdropdescriptiontext = true); //careful, some fb2k versions have broken IDataObject
		private:
			long refcount;
			DWORD m_initial_key_state;
			bool m_prev_is_showing_layered;
			mmh::comptr_t<IDragSourceHelper> m_DragSourceHelper;
			mmh::comptr_t<IDataObject> m_DataObject;
		};

	}
};
