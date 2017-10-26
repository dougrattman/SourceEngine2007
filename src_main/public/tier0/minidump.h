// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_MINIDUMP_H_
#define SOURCE_TIER0_MINIDUMP_H_

#include "tier0/platform.h"

// Writes out a minidump of the current stack trace with a unique filename.
PLATFORM_INTERFACE void WriteMiniDump();

#if defined(_WIN32)

using FnWMain = void (*)(int, char *[]);

// Calls the passed in function pointer and catches any exceptions/crashes
// thrown by it, and writes a minidump use from wmain() to protect the whole
// program.
PLATFORM_INTERFACE void CatchAndWriteMiniDump(FnWMain pfn, int argc,
                                              char *argv[]);

#include <DbgHelp.h>

// Replaces the current function pointer with the one passed in.
// Returns the previously-set function.
// The function is called internally by WriteMiniDump() and
// CatchAndWriteMiniDump() The default is the built-in function that uses
// DbgHlp.dll's MiniDumpWriteDump function
using FnMiniDump = void (*)(unsigned int uStructuredExceptionCode,
                            EXCEPTION_POINTERS *pExceptionInfo);
PLATFORM_INTERFACE FnMiniDump SetMiniDumpFunction(FnMiniDump pfn);

// Use this to write a minidump explicitly.
// Some of the tools choose to catch the minidump themselves instead of using
// CatchAndWriteMinidump so they can show their own dialog.
//
// ptchMinidumpFileNameBuffer if not-nullptr should be a writable char buffer of
// length at least _MAX_PATH and on return will contain the name of the minidump
// file written. If ptchMinidumpFileNameBuffer is nullptr the name of the
// minidump file written will not be available after the function returns.
PLATFORM_INTERFACE bool WriteMiniDumpUsingExceptionInfo(
    unsigned int uStructuredExceptionCode, EXCEPTION_POINTERS *pExceptionInfo,
    MINIDUMP_TYPE minidumpType, wchar_t *ptchMinidumpFileNameBuffer = nullptr);
#endif

#endif  // SOURCE_TIER0_MINIDUMP_H_
