// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include <cassert>
#include "base/include/windows/windows_light.h"

HINSTANCE g_hTier0Instance = nullptr;

#ifndef STATIC_TIER0
BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD call_reason,
                    _In_ void *) {
  switch (call_reason) {
    case DLL_PROCESS_ATTACH:
      ::DisableThreadLibraryCalls(instance);
      g_hTier0Instance = instance;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
    default:
      assert(false);
  }
  return TRUE;
}
#endif
