#include "stdafx.h"

namespace mmh {
	namespace ole {

		CLIPFORMAT ClipboardFormatDropDescription()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_DROPDESCRIPTION);
			return cfRet;
		}

		CLIPFORMAT DragWindowFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"DragWindow");
			return cfRet;
		}

		CLIPFORMAT IsShowingLayeredFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"IsShowingLayered");
			return cfRet;
		}

		CLIPFORMAT IsShowingTextFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"IsShowingText");
			return cfRet;
		}

		CLIPFORMAT DisableDragTextFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"DisableDragText");
			return cfRet;
		}

		CLIPFORMAT IsComputingImageFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"IsComutingImage");
			return cfRet;
		}

		CLIPFORMAT UsingDefaultDragImageFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(L"UsingDefaultDragImage");
			return cfRet;
		}

		CLIPFORMAT PreferredDropEffectFormat()
		{
			static const CLIPFORMAT cfRet = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
			return cfRet;
		}

		template<typename T>
		HRESULT GetDataObjectDataSimple(IDataObject *pDataObj, CLIPFORMAT cf, T & p_out)
		{
			HRESULT hr;

			FORMATETC fe = { 0 };
			fe.cfFormat = cf;
			fe.dwAspect = DVASPECT_CONTENT;
			fe.lindex = -1;
			fe.tymed = TYMED_HGLOBAL;

			STGMEDIUM stgm = { 0 };
			if (SUCCEEDED(hr = pDataObj->GetData(&fe, &stgm)))
			{
				void * pData = GlobalLock(stgm.hGlobal);
				if (pData)
				{
					p_out = *static_cast<T*>(pData);
					GlobalUnlock(pData);
				}
				ReleaseStgMedium(&stgm);
			}
			return hr;
		}

		HRESULT GetDragWindow(IDataObject *pDataObj, HWND & p_wnd)
		{
			HRESULT hr;
			DWORD dw;
			if (SUCCEEDED(hr = GetDataObjectDataSimple(pDataObj, DragWindowFormat(), dw)))
				p_wnd = (HWND)ULongToHandle(dw);

			return hr;
		}

		HRESULT GetIsShowingLayered(IDataObject *pDataObj, BOOL & p_out)
		{
			return GetDataObjectDataSimple(pDataObj, IsShowingLayeredFormat(), p_out);
		}

		HRESULT SetBlob(IDataObject *pdtobj, CLIPFORMAT cf, const void *pvBlob, UINT cbBlob)
		{
			HRESULT hr = E_OUTOFMEMORY;
			void *pv = GlobalAlloc(GPTR, cbBlob);
			if (pv)
			{
				CopyMemory(pv, pvBlob, cbBlob);

				FORMATETC fmte = { cf, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

				// The STGMEDIUM structure is used to define how to handle a global memory transfer. 
				// This structure includes a flag, tymed, which indicates the medium 
				// to be used, and a union comprising pointers and a handle for getting whichever 
				// medium is specified in tymed.
				STGMEDIUM medium = { 0 };
				medium.tymed = TYMED_HGLOBAL;
				medium.hGlobal = pv;

				hr = pdtobj->SetData(&fmte, &medium, TRUE);
				if (FAILED(hr))
				{
					GlobalFree(pv);
				}
			}
			return hr;
		}

		HRESULT SetDropDescription(IDataObject *pdtobj, DROPIMAGETYPE dit, const char * msg, const char * insert)
		{
			if (osversion::is_windows_vista_or_newer())
			{
				DROPDESCRIPTION dd_prev;
				memset(&dd_prev, 0, sizeof(dd_prev));

				bool dd_prev_valid = (SUCCEEDED(GetDataObjectDataSimple(pdtobj, ClipboardFormatDropDescription(), dd_prev)));

				pfc::stringcvt::string_os_from_utf8 wmsg(msg);
				pfc::stringcvt::string_os_from_utf8 winsert(insert);

				// Only set the drop description if it has actually changed (otherwise things get a bit crazy near the edge of the screen).
				if (!dd_prev_valid || dd_prev.type != dit || wcscmp(dd_prev.szInsert, winsert) || wcscmp(dd_prev.szMessage, wmsg))
				{
					DROPDESCRIPTION dd;
					dd.type = dit;
					wcscpy_s(dd.szMessage, wmsg.get_ptr());
					wcscpy_s(dd.szInsert, winsert.get_ptr());
					return SetBlob(pdtobj, ClipboardFormatDropDescription(), &dd, sizeof(dd));
				}
				else
					return S_OK;
			}
			return E_NOTIMPL;
		}

		HRESULT SetUsingDefaultDragImage(IDataObject *pdtobj, BOOL value)
		{
			return SetBlob(pdtobj, UsingDefaultDragImageFormat(), &value, sizeof(value));
		}

		HRESULT SetIsShowingText(IDataObject *pdtobj, BOOL value)
		{
			return SetBlob(pdtobj, IsShowingTextFormat(), &value, sizeof(value));
		}

		HRESULT SetDisableDragText(IDataObject *pdtobj, BOOL value)
		{
			return SetBlob(pdtobj, DisableDragTextFormat(), &value, sizeof(value));
		}

		HRESULT SetIsComputingImage(IDataObject *pdtobj, BOOL value)
		{
			return SetBlob(pdtobj, IsComputingImageFormat(), &value, sizeof(value));
		}

		HRESULT SetPreferredDropEffect(IDataObject *pdtobj, DWORD effect)
		{
			return SetBlob(pdtobj, PreferredDropEffectFormat(), &effect, sizeof(effect));
		}

		HRESULT DoDragDrop(HWND wnd, WPARAM initialKeyState, IDataObject *pDataObject, DWORD dwEffect, DWORD preferredEffect, DWORD *pdwEffect, SHDRAGIMAGE * lpsdi)
		{
			if (preferredEffect)
				SetPreferredDropEffect(pDataObject, preferredEffect);

			mmh::comptr_t<IDropSource> pDropSource;
			//if (!IsVistaOrNewer())
			pDropSource = new mmh::ole::IDropSource_Generic(wnd, pDataObject, initialKeyState, true, lpsdi);
			return SHDoDragDrop(wnd, pDataObject, pDropSource, dwEffect, pdwEffect);
		}

		HRESULT STDMETHODCALLTYPE IDropSource_Generic::QueryInterface(REFIID iid, void ** ppvObject)
		{
			if (ppvObject == NULL) return E_INVALIDARG;
			*ppvObject = NULL;
			if (iid == IID_IUnknown) { AddRef(); *ppvObject = (IUnknown*)this; return S_OK; }
			else if (iid == IID_IDropSource) { AddRef(); *ppvObject = (IDropSource*)this; return S_OK; }
			else return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE IDropSource_Generic::AddRef() { return InterlockedIncrement(&refcount); }
		ULONG STDMETHODCALLTYPE IDropSource_Generic::Release()
		{
			LONG rv = InterlockedDecrement(&refcount);
			if (!rv)
			{
				delete this;
			}
			return rv;
		}

		HRESULT STDMETHODCALLTYPE IDropSource_Generic::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
		{
			if (fEscapePressed || ((m_initial_key_state & MK_LBUTTON) && (grfKeyState & MK_RBUTTON)))
			{
				return DRAGDROP_S_CANCEL;
			}
			else if (
				((m_initial_key_state & MK_LBUTTON) && !(grfKeyState & MK_LBUTTON))
				|| ((m_initial_key_state & MK_RBUTTON) && !(grfKeyState & MK_RBUTTON))
				)
			{
				return DRAGDROP_S_DROP;
			}
			else return S_OK;
		}

		HRESULT STDMETHODCALLTYPE IDropSource_Generic::GiveFeedback(DWORD dwEffect)
		{
			HWND wnd_drag = NULL;
			BOOL isShowingLayered = FALSE;
			GetIsShowingLayered(m_DataObject, isShowingLayered);

			if (SUCCEEDED(GetDragWindow(m_DataObject, wnd_drag)) && wnd_drag)
				PostMessage(wnd_drag, DDWM_UPDATEWINDOW, NULL, NULL);

			if (isShowingLayered)
			{
				if (!m_prev_is_showing_layered)
				{
					auto cursor = LoadCursor(NULL, IDC_ARROW);
					SetCursor(cursor);
				}
				if (wnd_drag)
				{
					WPARAM wp = 1;
					if (dwEffect & DROPEFFECT_COPY)
						wp = 3;
					else if (dwEffect & DROPEFFECT_MOVE)
						wp = 2;
					else if (dwEffect & DROPEFFECT_LINK)
						wp = 4;

					PostMessage(wnd_drag, WM_USER + 2, wp, NULL);
				}
			}

			m_prev_is_showing_layered = isShowingLayered != 0;

			return isShowingLayered ? S_OK : DRAGDROP_S_USEDEFAULTCURSORS;
		}

		IDropSource_Generic::IDropSource_Generic(HWND wnd, IDataObject * pDataObj, DWORD initial_key_state, bool b_allowdropdescriptiontext, SHDRAGIMAGE * lpsdi)
			: refcount(0), m_initial_key_state(initial_key_state), m_DataObject(pDataObj), m_prev_is_showing_layered(false)
		{
			HRESULT hr;
			if (b_allowdropdescriptiontext)
			{
				if (SUCCEEDED(m_DragSourceHelper.instantiate(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER)))
				{
					mmh::comptr_t<IDragSourceHelper2> pDragSourceHelper2 = m_DragSourceHelper;
					if (pDragSourceHelper2.is_valid())
					{
						hr = pDragSourceHelper2->SetFlags(DSH_ALLOWDROPDESCRIPTIONTEXT);
					}
					if (lpsdi)
					{
						hr = m_DragSourceHelper->InitializeFromBitmap(lpsdi, pDataObj);
						if (FAILED(hr) && lpsdi->hbmpDragImage)
						{
							DeleteObject(lpsdi->hbmpDragImage);
							lpsdi->hbmpDragImage = NULL;
						}
					}
					else
						hr = m_DragSourceHelper->InitializeFromWindow(wnd, NULL, pDataObj);
					if (SUCCEEDED(hr))
					{
						SetUsingDefaultDragImage(pDataObj);
					}
				}
			}
		};

	}
}
