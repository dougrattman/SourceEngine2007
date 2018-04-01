// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_MINIDUMP_H_
#define SOURCE_TIER0_INCLUDE_MINIDUMP_H_

#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/base_types.h"
#include "base/include/windows/windows_errno_info.h"
#include "tier0/include/tier0_api.h"

// Writes out a minidump of the current stack trace with a unique filename.
SOURCE_TIER0_API void WriteMiniDump();

using FnMain = i32 (*)(i32, ch *[]);

// Calls the passed in function pointer and catches any exceptions/crashes
// thrown by it, and writes a minidump use from wmain() to protect the whole
// program.
SOURCE_TIER0_API i32 CatchAndWriteMiniDump(FnMain pfn, i32 argc, ch *argv[]);

#include "base/include/windows/windows_light.h"

#include <DbgHelp.h>

using FnMiniDump = void (*)(u32 se_code, EXCEPTION_POINTERS *se_info);

// Replaces the current function pointer with the one passed in.  Returns the previously-set
// function. The function is called internally by WriteMiniDump() and CatchAndWriteMiniDump().  The
// default is the built-in function that uses DbgHlp.dll's MiniDumpWriteDump function.
SOURCE_TIER0_API FnMiniDump SetMiniDumpFunction(FnMiniDump minidump_fn);

// Use this to write a minidump explicitly.  Some of the tools choose to catch the minidump
// themselves instead of using CatchAndWriteMinidump so they can show their own dialog.
//
// dump_file_name if not-nullptr should be a writable ch buffer of length at least SOURCE_MAX_PATH
// and on return will contain the name of the minidump file written.  If dump_file_name is nullptr
// the name of the minidump file written will not be available after the function returns.
SOURCE_TIER0_API source::windows::windows_errno_code WriteMiniDumpUsingExceptionInfo(
    u32 se_code, EXCEPTION_POINTERS *se_info, MINIDUMP_TYPE minidump_type,
    wch *dump_file_name = nullptr, usize dump_file_name_size = 0);
#endif

#endif  // SOURCE_TIER0_INCLUDE_MINIDUMP_H_
