// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/platform.h"

#include <cassert>
#include <ctime>

#include "tier0/vcrmode.h"
#include "xbox/xbox_console.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif

extern VCRMode_t g_VCRMode;
static uint64_t g_PerformanceFrequency;
static uint64_t g_MillisecondsPerformanceFrequency;
static uint64_t g_ClockStart;

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

double Plat_FloatTime() {
  DWORD return_code = InitTime();
  LARGE_INTEGER current_counter = {};

  if (return_code == ERROR_SUCCESS) {
    return_code = QueryPerformanceCounter(&current_counter) != FALSE
                      ? ERROR_SUCCESS
                      : GetLastError();
  }

  if (return_code == ERROR_SUCCESS) {
    double raw_seconds =
        max(0.0, (double)(current_counter.QuadPart - g_ClockStart) /
                     (double)(g_PerformanceFrequency));

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
    return static_cast<unsigned long>(
        (current_counter.QuadPart - g_ClockStart) /
        g_MillisecondsPerformanceFrequency);
  }

  return 0;
}

uint64_t Plat_PerformanceFrequency() {
  DWORD return_code = InitTime();

  if (return_code == ERROR_SUCCESS) {
    return g_PerformanceFrequency;
  }

  // Since we like to divide by this.
  return 1;
}

void GetCurrentDate(int *pDay, int *pMonth, int *pYear) {
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
#if defined(_WIN32)
  return IsDebuggerPresent() != FALSE;
#else
  return false;
#endif
}

void Plat_DebugString(const char *psz) {
#if defined(_WIN32)
  ::OutputDebugStringA(psz);
#endif
}

const char *Plat_GetCommandLine() { return Plat_GetCommandLineA(); }

const char *Plat_GetCommandLineA() { return GetCommandLine(); }

// Memory stuff.
//
// DEPRECATED. Still here to support binary back compatability of tier0.dll
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

typedef void (*Plat_AllocErrorFn)(unsigned long size);

void Plat_DefaultAllocErrorFn(unsigned long size) {}

Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;
#endif

CRITICAL_SECTION g_AllocCS;
class CAllocCSInit {
 public:
  CAllocCSInit() { InitializeCriticalSection(&g_AllocCS); }
} g_AllocCSInit;

PLATFORM_INTERFACE void *Plat_Alloc(unsigned long size) {
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

PLATFORM_INTERFACE void *Plat_Realloc(void *ptr, unsigned long size) {
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

PLATFORM_INTERFACE void Plat_Free(void *ptr) {
  EnterCriticalSection(&g_AllocCS);
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
  g_pMemAlloc->Free(ptr);
#else
  free(ptr);
#endif
  LeaveCriticalSection(&g_AllocCS);
}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
PLATFORM_INTERFACE void Plat_SetAllocErrorFn(Plat_AllocErrorFn fn) {
  g_AllocError = fn;
}
#endif
