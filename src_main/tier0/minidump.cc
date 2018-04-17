// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/minidump.h"

#ifdef OS_WIN
#include <ctime>

#include "base/include/macros.h"
#include "base/include/unique_module_ptr.h"
#include "base/include/windows/scoped_se_translator.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"
#include "tier0/include/valve_off.h"

// true if we're currently writing a minidump caused by an assert
static bool g_bWritingNonfatalMinidump = false;
// counter used to make sure minidump names are unique
static i32 g_nMinidumpsWritten = 0;
// is in exception mode.
static bool g_bInException = false;

// Creates a new file and dumps the exception info into it.
source::windows::windows_errno_code WriteMiniDumpUsingExceptionInfo(
    u32 se_code, EXCEPTION_POINTERS *se_info, MINIDUMP_TYPE minidump_type,
    wch *dump_file_name, usize dump_file_name_size) {
  if (dump_file_name && dump_file_name_size) *dump_file_name = L'\0';

  auto [dbghelp_module, errno_info] =
      source::unique_module_ptr::from_load_library(
          L"DbgHelp.dll", LOAD_LIBRARY_SEARCH_SYSTEM32);

  using MiniDumpWriteDumpFn =
      BOOL(WINAPI *)(_In_ HANDLE hProcess, _In_ DWORD ProcessId,
                     _In_ HANDLE hFile, _In_ MINIDUMP_TYPE DumpType,
                     _In_opt_ PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                     _In_opt_ PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                     _In_opt_ PMINIDUMP_CALLBACK_INFORMATION CallbackParam);
  MiniDumpWriteDumpFn mini_dump_write_dump{nullptr};

  if (errno_info.is_success() && dbghelp_module) {
    std::tie(mini_dump_write_dump, errno_info) =
        dbghelp_module.get_address_as<MiniDumpWriteDumpFn>("MiniDumpWriteDump");
  }

  // Create a unique filename for the minidump based on the current time and
  // module name.
  time_t time_now{time(nullptr)};
  tm local_time_now{0};

  // TODO(d.rattman): Convert POSIX errno => windows errno code.
  source::windows::windows_errno_code error_code{
      errno_info.is_success() ? localtime_s(&local_time_now, &time_now) : S_OK};

  // Strip off the rest of the path from the .exe name.
  wch module_name[SOURCE_MAX_PATH];

  if (error_code == S_OK) {
    SetLastError(NOERROR);
    GetModuleFileNameW(nullptr, module_name, SOURCE_ARRAYSIZE(module_name));

    error_code = source::windows::windows_errno_code_last_error();
  }

  wch file_name[SOURCE_MAX_PATH];
  HANDLE minidump_file{nullptr};

  if (error_code == S_OK) {
    wch *stripped_module_name = wcsrchr(module_name, L'.');
    if (stripped_module_name) *stripped_module_name = L'\0';

    stripped_module_name = wcsrchr(module_name, L'\\');
    // Move past the last slash.
    if (stripped_module_name) ++stripped_module_name;

    // Can't use the normal string functions since we're in tier0.
    _snwprintf_s(file_name, SOURCE_ARRAYSIZE(file_name),
                 L"%s_%s_%d%.2d%2d%.2d%.2d%.2d_%d.mdmp",
                 stripped_module_name ? stripped_module_name : L"unknown",
                 g_bWritingNonfatalMinidump ? L"assert" : L"crash",
                 local_time_now.tm_year + 1900, /* Year less 2000 */
                 local_time_now.tm_mon + 1, /* month (0 - 11 : 0 = January) */
                 local_time_now.tm_mday,    /* day of month (1 - 31) */
                 local_time_now.tm_hour,    /* hour (0 - 23) */
                 local_time_now.tm_min,     /* minutes (0 - 59) */
                 local_time_now.tm_sec,     /* seconds (0 - 59) */
                 ++g_nMinidumpsWritten);    // ensures the filename is unique

    minidump_file =
        CreateFileW(file_name, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    error_code = minidump_file != INVALID_HANDLE_VALUE
                     ? S_OK
                     : source::windows::windows_errno_code_last_error();
  }

  if (error_code == S_OK) {
    // Dump the exception information into the file.
    MINIDUMP_EXCEPTION_INFORMATION ex_info;
    ex_info.ThreadId = GetCurrentThreadId();
    ex_info.ExceptionPointers = se_info;
    ex_info.ClientPointers = FALSE;

    const BOOL was_written_minidump{(*mini_dump_write_dump)(
        GetCurrentProcess(), GetCurrentProcessId(), minidump_file,
        minidump_type, &ex_info, nullptr, nullptr)};

    // If the function succeeds, the return value is TRUE; otherwise, the
    // return value is FALSE.  To retrieve extended error information, call
    // GetLastError.  Note that the last error will be an HRESULT value.  If
    // the operation is canceled, the last error code is
    // HRESULT_FROM_WIN32(ERROR_CANCELLED).
    // See
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms680360(v=vs.85).aspx
    error_code = was_written_minidump
                     ? S_OK
                     : implicit_cast<source::windows::windows_errno_code>(
                           GetLastError());

    CloseHandle(minidump_file);
  }

  if (error_code == S_OK) {
    if (dump_file_name) {
      wcscpy_s(dump_file_name, dump_file_name_size, file_name);
    }
  } else {
    // Mark any failed minidump writes by renaming them.
    if (minidump_file != INVALID_HANDLE_VALUE) {
      wch failed_file_name[SOURCE_MAX_PATH];
      _snwprintf_s(failed_file_name, SOURCE_ARRAYSIZE(failed_file_name),
                   L"(failed)%s", file_name);

      _wrename(file_name, failed_file_name);
    }
  }

  return error_code;
}

static void Tier0WriteMiniDump(u32 unstructured_exception_code,
                               EXCEPTION_POINTERS *exception_infos) {
  // First try to write it with all the indirectly referenced memory (ie: a
  // large file). If that doesn't work, then write a smaller one.
  auto minidump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);

  if (!source::windows::succeeded(WriteMiniDumpUsingExceptionInfo(
          unstructured_exception_code, exception_infos, minidump_type))) {
    minidump_type = MiniDumpWithDataSegs;
    WriteMiniDumpUsingExceptionInfo(unstructured_exception_code,
                                    exception_infos, minidump_type);
  }
}

// Minidump function to use.
static FnMiniDump g_pfnWriteMiniDump{Tier0WriteMiniDump};

// Set a function to call which will write our minidump, overriding the default
// function.
FnMiniDump SetMiniDumpFunction(FnMiniDump pfn) {
  return std::exchange(g_pfnWriteMiniDump, pfn);
}

// Writes out a minidump from the current process.
void WriteMiniDump() {
  // throw an exception so we can catch it and get the stack info
  g_bWritingNonfatalMinidump = true;

  __try {
    RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    // Never get here (non-continuable exception).
  } __except (g_pfnWriteMiniDump(0, GetExceptionInformation()),
              EXCEPTION_EXECUTE_HANDLER) {
    // Write the minidump from inside the filter (GetExceptionInformation() is
    // only valid in the filter).
  }

  g_bWritingNonfatalMinidump = false;
}

// Catches and writes out any exception throw by the specified function.
i32 CatchAndWriteMiniDump(FnMain main, i32 argc, ch *argv[]) {
  if (!main) {
    AssertMsg(0, "No main function");
    return -2;
  }

  // Don't mask exceptions when running in the debugger
  if (Plat_IsInDebugSession()) return (*main)(argc, argv);

  try {
    source::windows::ScopedSeTranslator scoped_se_translator{
        g_pfnWriteMiniDump};

    return (*main)(argc, argv);
  } catch (...) {
    g_bInException = true;
    DMsg("console", 1, "Fatal exception caught, minidump written.\n");
    // Handle everything and just quit, we've already written out our
    // minidump.
  }

  return -1;
}
#endif
