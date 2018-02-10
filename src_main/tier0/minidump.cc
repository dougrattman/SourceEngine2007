// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/minidump.h"

#ifdef OS_WIN

#include <ctime>

#include "tier0/include/platform.h"
#include "tier0/include/valve_off.h"

// true if we're currently writing a minidump caused by an assert
static bool g_bWritingNonfatalMinidump = false;
// counter used to make sure minidump names are unique
static i32 g_nMinidumpsWritten = 0;
// is in exception mode.
static bool g_bInException = false;

// Purpose: Creates a new file and dumps the exception info into it
bool WriteMiniDumpUsingExceptionInfo(
    u32 uStructuredExceptionCode, _EXCEPTION_POINTERS *pExceptionInfo,
    MINIDUMP_TYPE minidumpType, wch *ptchMinidumpFileNameBuffer /* = nullptr */
) {
  if (ptchMinidumpFileNameBuffer) {
    *ptchMinidumpFileNameBuffer = L'\0';
  }

  // Get the function pointer directly so that we don't have to include the
  // .lib, and that we can easily change it to using our own dll when this code
  // is used on win98/ME/2K machines
  HMODULE dbghelp_module = ::LoadLibraryW(L"DbgHelp.dll");
  if (!dbghelp_module) return false;

  // MiniDumpWriteDumpFunc() function declaration (so we can just get the
  // function directly from windows)
  using MiniDumpWriteDumpFunc =
      BOOL(WINAPI *)(_In_ HANDLE hProcess, _In_ DWORD ProcessId,
                     _In_ HANDLE hFile, _In_ MINIDUMP_TYPE DumpType,
                     _In_opt_ PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                     _In_opt_ PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                     _In_opt_ PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

  bool bReturnValue = false;
  const auto miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFunc>(
      ::GetProcAddress(dbghelp_module, "MiniDumpWriteDump"));

  if (miniDumpWriteDump) {
    // create a unique filename for the minidump based on the current time and
    // module name
    time_t currTime = ::time(nullptr);
    struct tm *pTime = ::localtime(&currTime);
    ++g_nMinidumpsWritten;

    // strip off the rest of the path from the .exe name
    wch module_name[MAX_PATH];
    ::GetModuleFileNameW(nullptr, module_name, ARRAYSIZE(module_name));
    wch *pch = wcsrchr(module_name, L'.');
    if (pch) {
      *pch = L'\0';
    }
    pch = wcsrchr(module_name, L'\\');
    if (pch) {
      // move past the last slash
      pch++;
    }

    // can't use the normal string functions since we're in tier0
    wch file_name[MAX_PATH];
    _snwprintf_s(file_name, ARRAYSIZE(file_name),
                 L"%s_%s_%d%.2d%2d%.2d%.2d%.2d_%d.mdmp", pch ? pch : L"unknown",
                 g_bWritingNonfatalMinidump ? L"assert" : L"crash",
                 pTime->tm_year + 1900, /* Year less 2000 */
                 pTime->tm_mon + 1,     /* month (0 - 11 : 0 = January) */
                 pTime->tm_mday,        /* day of month (1 - 31) */
                 pTime->tm_hour,        /* hour (0 - 23) */
                 pTime->tm_min,         /* minutes (0 - 59) */
                 pTime->tm_sec,         /* seconds (0 - 59) */
                 g_nMinidumpsWritten    // ensures the filename is unique
    );

    BOOL bMinidumpResult = FALSE;
    HANDLE minidump_file =
        ::CreateFileW(file_name, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (minidump_file != INVALID_HANDLE_VALUE) {
      // dump the exception information into the file
      _MINIDUMP_EXCEPTION_INFORMATION ex_info;
      ex_info.ThreadId = ::GetCurrentThreadId();
      ex_info.ExceptionPointers = pExceptionInfo;
      ex_info.ClientPointers = FALSE;

      bMinidumpResult = (*miniDumpWriteDump)(
          ::GetCurrentProcess(), ::GetCurrentProcessId(), minidump_file,
          minidumpType, &ex_info, nullptr, nullptr);
      ::CloseHandle(minidump_file);

      if (bMinidumpResult) {
        bReturnValue = true;

        if (ptchMinidumpFileNameBuffer) {
          // Copy the file name from "pSrc = rgchFileName" into "pTgt =
          // ptchMinidumpFileNameBuffer"
          wch *pTgt = ptchMinidumpFileNameBuffer;
          wch const *pSrc = file_name;
          while ((*(pTgt++) = *(pSrc++)) != ch(0)) continue;
        }
      }

      // fall through to trying again
    }

    // mark any failed minidump writes by renaming them
    if (!bMinidumpResult) {
      wch failed_file_name[_MAX_PATH];
      _snwprintf_s(failed_file_name, ARRAYSIZE(failed_file_name), L"(failed)%s",
                   file_name);
      _wrename(file_name, failed_file_name);
    }
  }

  ::FreeLibrary(dbghelp_module);

  // call the log flush function if one is registered to try to flush any logs
  // CallFlushLogFunc();

  return bReturnValue;
}

void InternalWriteMiniDumpUsingExceptionInfo(
    u32 unstructured_exception_code, _EXCEPTION_POINTERS *exception_infos) {
  // First try to write it with all the indirectly referenced memory (ie: a
  // large file). If that doesn't work, then write a smaller one.
  MINIDUMP_TYPE minidump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);

  if (!WriteMiniDumpUsingExceptionInfo(unstructured_exception_code,
                                       exception_infos, minidump_type)) {
    minidump_type = MiniDumpWithDataSegs;
    WriteMiniDumpUsingExceptionInfo(unstructured_exception_code,
                                    exception_infos, minidump_type);
  }
}

// minidump function to use
static FnMiniDump g_pfnWriteMiniDump = InternalWriteMiniDumpUsingExceptionInfo;

// Purpose: Set a function to call which will write our minidump, overriding
// the default function
FnMiniDump SetMiniDumpFunction(FnMiniDump pfn) {
  FnMiniDump pfnTemp = g_pfnWriteMiniDump;
  g_pfnWriteMiniDump = pfn;
  return pfnTemp;
}

// Purpose: writes out a minidump from the current process
void WriteMiniDump() {
  // throw an exception so we can catch it and get the stack info
  g_bWritingNonfatalMinidump = true;

  __try {
    ::RaiseException(0,                         // dwExceptionCode
                     EXCEPTION_NONCONTINUABLE,  // dwExceptionFlags
                     0,                         // nNumberOfArguments,
                     nullptr);                  // const ULONG_PTR* lpArguments
    // Never get here (non-continuable exception)
  }
  // Write the minidump from inside the filter (GetExceptionInformation() is
  // only valid in the filter)
  __except (g_pfnWriteMiniDump(0, GetExceptionInformation()),
            EXCEPTION_EXECUTE_HANDLER) {
  }

  g_bWritingNonfatalMinidump = false;
}

// Purpose: Catches and writes out any exception throw by the specified function
void CatchAndWriteMiniDump(FnWMain pfn, i32 argc, ch *argv[]) {
  if (Plat_IsInDebugSession()) {
    // don't mask exceptions when running in the debugger
    pfn(argc, argv);
  } else {
    try {
      _set_se_translator(g_pfnWriteMiniDump);
      pfn(argc, argv);
    } catch (...) {
      g_bInException = true;
      DMsg("console", 1, "Fatal exception caught, minidump written\n");
      // handle everything and just quit, we've already written out our minidump
    }
  }
}

#endif
