// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef TOOLS_MINIDUMP_H
#define TOOLS_MINIDUMP_H

// Defaults to false. If true, it'll write larger minidump files with the
// contents of global variables and following pointers from where the crash
// occurred.
void EnableFullMinidumps(bool enable);

// This handler catches any crash, writes a minidump, and runs the default
// system crash handler (which usually shows a dialog).
void SetupDefaultToolsMinidumpHandler();

// (Used by VMPI) - you specify your own crash handler.
// Arguments passed to ToolsExceptionHandler
//  exceptionCode - exception code
//  pvExceptionInfo - on Win32 platform points to "struct_EXCEPTION_POINTERS"
//  otherwise NULL
using ToolsExceptionHandler = void (*)(unsigned long exception_code,
                                       void *info);
ToolsExceptionHandler SetupToolsMinidumpHandler(ToolsExceptionHandler handler);

#endif  // MINIDUMP_H
