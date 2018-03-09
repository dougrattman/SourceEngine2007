// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Make dynamic loading of dx_proxy.dll and methods acquisition easier.

#ifndef DX_PROXY_H
#define DX_PROXY_H

#include "base/include/windows/windows_light.h"

// Uses a lazy-load technique to load the dx_proxy.dll module and acquire the
// function pointers.
// The dx_proxy.dll module is automatically unloaded during desctruction.
class DxProxyModule {
 public:
  DxProxyModule() : m_hModule{nullptr} {
    ZeroMemory(m_arrFuncs, sizeof(m_arrFuncs));
  }
  ~DxProxyModule() { Free(); }

 private:  // Prevent copying via copy constructor or assignment
  DxProxyModule(const DxProxyModule&) = delete;
  DxProxyModule& operator=(const DxProxyModule&) = delete;

 public:
  // Loads the module and acquires function pointers, returns if the module was
  // loaded successfully.
  // If the module was already loaded the call has no effect and returns TRUE.
  BOOL Load() {
    if (m_hModule == nullptr &&
        (m_hModule = ::LoadLibraryW(L"dx_proxy.dll")) != nullptr) {
      // Requested function names array
      LPCSTR const arrFuncNames[fnTotal] = {"Proxy_D3DXCompileShaderFromFile"};

      // Acquire the functions
      for (int k = 0; k < fnTotal; ++k) {
        m_arrFuncs[k] = ::GetProcAddress(m_hModule, arrFuncNames[k]);
      }
    }

    return m_hModule ? TRUE : FALSE;
  }
  // Frees the loaded module.
  void Free() {
    if (m_hModule) {
      ::FreeLibrary(m_hModule);
      m_hModule = nullptr;
      ZeroMemory(m_arrFuncs, sizeof(m_arrFuncs));
    }
  }

 private:
  enum Func { fnD3DXCompileShaderFromFile = 0, fnTotal };
  HMODULE m_hModule;            //!< The handle of the loaded dx_proxy.dll
  FARPROC m_arrFuncs[fnTotal];  //!< The array of loaded function pointers

  // Interface functions calling into DirectX proxy
 public:
  inline HRESULT D3DXCompileShaderFromFile(
      LPCSTR pSrcFile, CONST D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude,
      LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags,
      LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs,
      LPD3DXCONSTANTTABLE* ppConstantTable) {
    if (!Load()) return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 1);
    if (!m_arrFuncs[fnD3DXCompileShaderFromFile])
      return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 2);

    return (*(HRESULT(WINAPI*)(
        LPCSTR, CONST D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR, DWORD,
        LPD3DXBUFFER*, LPD3DXBUFFER*,
        LPD3DXCONSTANTTABLE*))m_arrFuncs[fnD3DXCompileShaderFromFile])(
        pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader,
        ppErrorMsgs, ppConstantTable);
  }
};

#endif  // DX_PROXY_H
