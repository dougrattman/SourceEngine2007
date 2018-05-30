// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <cassert>

#include "base/include/windows/windows_light.h"

BOOL APIENTRY DllMain(_In_ HINSTANCE instance, _In_ DWORD call_reason,
                      _In_ void *) {
  switch (call_reason) {
    // Do not call DisableThreadLibraryCalls function from a DLL that is linked
    // to the static C run-time library (CRT).  The static CRT requires
    // DLL_THREAD_ATTACH and DLL_THREAD_DETATCH notifications to function
    // properly.  See
    // https://msdn.microsoft.com/en-us/library/ms682579(v=vs.85).aspx
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
    default:
      assert(false);
      return FALSE;
  }
  return TRUE;
}