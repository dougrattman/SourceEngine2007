// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Entity classes exported by Halflife.

#ifdef _WIN32

#include "winlite.h"

#include "datamap.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Ensure data sizes are stable
COMPILE_TIME_ASSERT(sizeof(inputfunc_t) == sizeof(int));

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

#endif
