// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_DX_PROXY_DXINCLUDEIMPL_H_
#define SOURCE_DX_PROXY_DXINCLUDEIMPL_H_

#include "filememcache.h"

FileCache s_incFileCache;

struct D3DXIncludeImpl : public ID3DXInclude {
  STDMETHOD(Open)
  (D3DXINCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID *ppData, UINT *pBytes) {
    CachedFileData *pFileData = s_incFileCache.Get(pFileName);
    if (!pFileData || !pFileData->IsValid()) return E_FAIL;

    *ppData = pFileData->GetDataPtr();
    *pBytes = pFileData->GetDataLen();

    pFileData->AddRef();

    return S_OK;
  }

  STDMETHOD(Open)
  (D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData,
   LPCVOID *ppData, UINT *pBytes,
   /* OUT */ LPSTR pFullPath, DWORD cbFullPath) {
    if (pFullPath && cbFullPath) strcpy_s(pFullPath, cbFullPath, pFileName);
    return Open(IncludeType, pFileName, pParentData, ppData, pBytes);
  }

  STDMETHOD(Close)(LPCVOID pData) {
    if (CachedFileData *pFileData = CachedFileData::GetByDataPtr(pData))
      pFileData->Release();

    return S_OK;
  }
};

D3DXIncludeImpl s_incDxImpl;

#endif  // SOURCE_DX_PROXY_DXINCLUDEIMPL_H_
