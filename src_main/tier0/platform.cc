// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/platform.h"

#include <algorithm>
#include <cassert>
#include <ctime>

#include "base/include/stdio_file_stream.h"
#include "base/include/windows/windows_light.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/threadtools.h"
#include "tier0/include/vcrmode.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/include/memalloc.h"

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

  fprintf(stderr, "%s", psz);
}

const ch *Plat_GetCommandLine() { return GetCommandLine(); }

// For debugging startup times, etc.
bool Plat_TimestampedLog(ch const *fmt, ...) {
  static f64 last_stamp{0.0};
  static bool should_log{false};
  static bool is_checked{false};
  static bool is_first_time_write{true};

  if (!is_checked) {
    should_log = CommandLine()->CheckParm("-profile");
    is_checked = true;
  }
  if (!should_log) return false;

  ch log_buffer[1024];
  va_list arg_list;
  va_start(arg_list, fmt);
  _vsnprintf_s(log_buffer, std::size(log_buffer), fmt, arg_list);
  va_end(arg_list);

  const f64 current_stamp = Plat_FloatTime();
  const bool should_show_legend{is_first_time_write};

  if (is_first_time_write) {
    _unlink("profile_timestamps.log");
    is_first_time_write = false;
  }

  auto [log, errno_info] =
      source::stdio_file_stream_factory::open("profile_timestamps.log", "at+");

  if (errno_info.is_success()) {
    if (should_show_legend) {
      log.print("[From (s) | Diff (s)] Profile log message.\n");
    }

    size_t bytes_written;
    std::tie(bytes_written, errno_info) =
        log.print("[%8.4f | %8.4f] %s\n", current_stamp,
                  current_stamp - last_stamp, log_buffer);
    last_stamp = current_stamp;

    return errno_info.is_success() && bytes_written > 0;
  }

  last_stamp = current_stamp;

  return false;
}

void Plat_SetThreadName(unsigned long dwThreadID, const ch *pszName) {
  ThreadSetDebugName(dwThreadID, pszName);
}

unsigned long Plat_RegisterThread(const ch *pszName) {
  ThreadSetDebugName(pszName);
  return ThreadGetCurrentId();
}

unsigned long Plat_GetCurrentThreadID() { return ThreadGetCurrentId(); }
