// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/platform.h"

#include <algorithm>
#include <cassert>
#include <ctime>

#include "tier0/include/vcrmode.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/include/memalloc.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"
#endif

extern VCRMode_t g_VCRMode;
static u64 g_PerformanceFrequency;
static u64 g_MillisecondsPerformanceFrequency;
static u64 g_ClockStart;

static DWORD InitTime() {
  if (!g_PerformanceFrequency) {
    LARGE_INTEGER performance_frequency, clock_start;

    if (QueryPerformanceFrequency(&performance_frequency) == FALSE) {
      return GetLastError();
    }

    g_PerformanceFrequency = performance_frequency.QuadPart;
    g_MillisecondsPerformanceFrequency = g_PerformanceFrequency / 1000;

    if (QueryPerformanceCounter(&clock_start) == FALSE) {
      return GetLastError();
    }

    g_ClockStart = clock_start.QuadPart;
  }

  return ERROR_SUCCESS;
}

f64 Plat_FloatTime() {
  DWORD return_code = InitTime();
  LARGE_INTEGER current_counter = {};

  if (return_code == ERROR_SUCCESS) {
    return_code = QueryPerformanceCounter(&current_counter) != FALSE
                      ? ERROR_SUCCESS
                      : GetLastError();
  }

  if (return_code == ERROR_SUCCESS) {
    f64 raw_seconds =
        std::max(0.0, (f64)(current_counter.QuadPart - g_ClockStart) /
                          (f64)(g_PerformanceFrequency));

#if !defined(STEAM)
    if (g_VCRMode == VCR_Disabled) return raw_seconds;

    return VCRHook_Sys_FloatTime(raw_seconds);
#else
    return raw_seconds;
#endif
  }

  return 0.0;
}

unsigned long Plat_MSTime() {
  DWORD return_code = InitTime();
  LARGE_INTEGER current_counter = {};

  if (return_code == ERROR_SUCCESS) {
    return_code = QueryPerformanceCounter(&current_counter) != FALSE
                      ? ERROR_SUCCESS
                      : GetLastError();
  }

  if (return_code == ERROR_SUCCESS) {
    return static_cast<u32>((current_counter.QuadPart - g_ClockStart) /
                            g_MillisecondsPerformanceFrequency);
  }

  return 0;
}

u64 Plat_PerformanceFrequency() {
  DWORD return_code = InitTime();

  if (return_code == ERROR_SUCCESS) {
    return g_PerformanceFrequency;
  }

  // Since we like to divide by this.
  return 1;
}

void GetCurrentDate(i32 *pDay, i32 *pMonth, i32 *pYear) {
  struct tm *pNewTime;
  time_t long_time;

  time(&long_time);                 /* Get time as long integer. */
  pNewTime = localtime(&long_time); /* Convert to local time. */

  *pDay = pNewTime->tm_mday;
  *pMonth = pNewTime->tm_mon + 1;
  *pYear = pNewTime->tm_year + 1900;
}

bool vtune(bool resume) {
  static bool bInitialized = false;
  static void(__cdecl * VTResume)(void) = nullptr;
  static void(__cdecl * VTPause)(void) = nullptr;

  // Grab the Pause and Resume function pointers from the VTune DLL the first
  // time through:
  if (!bInitialized) {
    bInitialized = true;

    const HMODULE vtune_module = LoadLibraryW(L"vtuneapi.dll");

    if (vtune_module) {
      VTResume = reinterpret_cast<decltype(VTResume)>(
          GetProcAddress(vtune_module, "VTResume"));
      VTPause = reinterpret_cast<decltype(VTPause)>(
          GetProcAddress(vtune_module, "VTPause"));
    }
  }

  // Call the appropriate function, as indicated by the argument:
  if (resume && VTResume) {
    VTResume();
    return true;
  }

  if (!resume && VTPause) {
    VTPause();
    return true;
  }

  return false;
}

bool Plat_IsInDebugSession() {
#ifdef OS_WIN
  return IsDebuggerPresent() != FALSE;
#else
  return false;
#endif
}

void Plat_DebugString(const ch *psz) {
#ifdef OS_WIN
  ::OutputDebugStringA(psz);
#endif
}

const ch *Plat_GetCommandLine() { return GetCommandLine(); }

// Memory stuff.
//
// DEPRECATED. Still here to support binary back compatability of tier0.dll
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

typedef void (*Plat_AllocErrorFn)(u32 size);

void Plat_DefaultAllocErrorFn(u32 size) {}

Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;
#endif

CRITICAL_SECTION g_AllocCS;
class CAllocCSInit {
 public:
  CAllocCSInit() { InitializeCriticalSection(&g_AllocCS); }
} g_AllocCSInit;

SOURCE_TIER0_API void *Plat_Alloc(u32 size) {
  EnterCriticalSection(&g_AllocCS);
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
  void *pRet = g_pMemAlloc->Alloc(size);
#else
  void *pRet = malloc(size);
#endif
  LeaveCriticalSection(&g_AllocCS);
  if (pRet) {
    return pRet;
  } else {
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
    g_AllocError(size);
#endif
    return 0;
  }
}

SOURCE_TIER0_API void *Plat_Realloc(void *ptr, u32 size) {
  EnterCriticalSection(&g_AllocCS);
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
  void *pRet = g_pMemAlloc->Realloc(ptr, size);
#else
  void *pRet = realloc(ptr, size);
#endif
  LeaveCriticalSection(&g_AllocCS);
  if (pRet) {
    return pRet;
  } else {
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
    g_AllocError(size);
#endif
    return 0;
  }
}

SOURCE_TIER0_API void Plat_Free(void *ptr) {
  EnterCriticalSection(&g_AllocCS);
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
  g_pMemAlloc->Free(ptr);
#else
  free(ptr);
#endif
  LeaveCriticalSection(&g_AllocCS);
}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
SOURCE_TIER0_API void Plat_SetAllocErrorFn(Plat_AllocErrorFn fn) {
  g_AllocError = fn;
}
#endif
