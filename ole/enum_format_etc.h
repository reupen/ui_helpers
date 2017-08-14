/**
 * This code is based on:
 * - The DragImg sample in the Windows SDK for Windows Server 2008 and .NET Framework 3.5
 * - https://msdn.microsoft.com/en-gb/library/ms997502.aspx [http://archive.is/uvdvf]
 * 
 * The licence terms for the relevant Windows SDK are contained in windows-sdk-6.1-licence.html.
 */

/**************************************************************************
   THIS CODE AND INFORMATION IS PROVIDED 'AS IS' WITHOUT WARRANTY OF
   ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
   PARTICULAR PURPOSE.

   Copyright 1999 - 2000 Microsoft Corporation.  All Rights Reserved.
**************************************************************************/

#pragma once

#include <windows.h>
#include <ole2.h>

class CEnumFormatEtc : public IEnumFORMATETC {
public:
    CEnumFormatEtc(LPFORMATETC pFE, int numberItems);
    ~CEnumFormatEtc(void);

    //BOOL FInit(HWND);

    //IUnknown members
    STDMETHOD(QueryInterface)(REFIID, LPVOID*);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    //IEnumFORMATETC members
    STDMETHOD(Next)(ULONG, LPFORMATETC, ULONG*);
    STDMETHOD(Skip)(ULONG);
    STDMETHOD(Reset)(void);
    STDMETHOD(Clone)(IEnumFORMATETC**);

private:
    DWORD m_cRefCount;
    LPFORMATETC m_pStrFE;
    ULONG m_iCur;
    ULONG m_cItems;
};
