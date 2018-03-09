// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_MINIDUMP_H_
#define SOURCE_TIER0_INCLUDE_MINIDUMP_H_

#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/base_types.h"
#include "tier0/include/tier0_api.h"

// Writes out a minidump of the current stack trace with a unique filename.
SOURCE_TIER0_API void WriteMiniDump();

using FnWMain = void (*)(i32, ch *[]);

// Calls the passed in function pointer and catches any exceptions/crashes
// thrown by it, and writes a minidump use from wmain() to protect the whole
// program.
SOURCE_TIER0_API void CatchAndWriteMiniDump(FnWMain pfn, i32 argc, ch *argv[]);

#include "base/include/windows/windows_light.h"

#include <DbgHelp.h>

using FnMiniDump = void (*)(u32 uStructuredExceptionCode, EXCEPTION_POINTERS *pExceptionInfo);

// Replaces the current function pointer with the one passed in.
// Returns the previously-set function.
// The function is called internally by WriteMiniDump() and CatchAndWriteMiniDump() The default is
// the built-in function that uses DbgHlp.dll's MiniDumpWriteDump function.
SOURCE_TIER0_API FnMiniDump SetMiniDumpFunction(FnMiniDump pfn);

// Use this to write a minidump explicitly.
// Some of the tools choose to catch the minidump themselves instead of using
// CatchAndWriteMinidump so they can show their own dialog.
//
// ptchMinidumpFileNameBuffer if not-nullptr should be a writable ch buffer of
// length at least SOURCE_MAX_PATH and on return will contain the name of the minidump
// file written. If ptchMinidumpFileNameBuffer is nullptr the name of the
// minidump file written will not be available after the function returns.
SOURCE_TIER0_API bool WriteMiniDumpUsingExceptionInfo(u32 uStructuredExceptionCode,
                                                      EXCEPTION_POINTERS *pExceptionInfo,
                                                      MINIDUMP_TYPE minidumpType,
                                                      wch *ptchMinidumpFileNameBuffer = nullptr);
#endif

#endif  // SOURCE_TIER0_INCLUDE_MINIDUMP_H_
