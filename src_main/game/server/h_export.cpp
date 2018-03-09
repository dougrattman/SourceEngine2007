// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Entity classes exported by Halflife.

#include <cassert>

#include "base/include/windows/windows_light.h"

#include "datamap.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

// Ensure data sizes are stable
static_assert(sizeof(inputfunc_t) == sizeof(int));

HINSTANCE win32DLLHandle;

// Required DLL entry point
BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD call_reason,
                    _In_ LPVOID) {
  switch (call_reason) {
    case DLL_PROCESS_ATTACH:
      win32DLLHandle = instance;
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
