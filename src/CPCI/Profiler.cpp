// Profiler.cpp : Implementation of CProfiler

#include "stdafx.h"
#include "Profiler.h"

typedef void(__cdecl *PREMAIN)(void);

STDMETHODIMP CProfiler::Initialize(IUnknown *pICorProfilerInfoUnk) {
    //fprintf(stderr, "Attempting managed code workaround ...\n");

    HINSTANCE auditor_handle = LoadLibrary(_T("LibAO.dll"));
    if (auditor_handle == NULL) {
	fprintf(stderr, "Error: unable to find LibAO.dll");
	return E_FAIL;
    }

    m_spCorInfo = pICorProfilerInfoUnk;
    ATLASSERT(m_spCorInfo);
    m_spCorInfo->SetEventMask(COR_PRF_MONITOR_NONE);

    PREMAIN premain = (PREMAIN)GetProcAddress(auditor_handle, "PreMain");
    if (premain == NULL) {
	fprintf(stderr, "Error: unable to find PreMain");
	return E_FAIL;
    }

    // Bootstrap the real auditor ...
    premain();

    return S_OK;
}

STDMETHODIMP CProfiler::Shutdown() {
    //printf("DETACHING cpci.dll ...\n");
    m_spCorInfo.Release();
    return S_OK;
}
