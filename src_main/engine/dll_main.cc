// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include <cassert>
#include "crtmemdebug.h"
#include "winlite.h"

#ifdef _WIN32
BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD call_reason,
                    _In_ LPVOID) {
  switch (call_reason) {
    case DLL_PROCESS_ATTACH:
      InitCRTMemDebug();
      ::DisableThreadLibraryCalls(instance);
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
