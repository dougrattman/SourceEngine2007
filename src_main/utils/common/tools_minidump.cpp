// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tools_minidump.h"

#include <utility>  // for std::exchange

#include "base/include/windows/windows_light.h"
#include "tier0/include/minidump.h"

namespace {
bool g_should_write_full_minidumps{false};
ToolsExceptionHandler g_exception_handler{nullptr};

LONG WINAPI DefaultToolsExceptionFilter(EXCEPTION_POINTERS *info) {
  // Non VMPI workers write a minidump and show a crash dialog like normal.
  const MINIDUMP_TYPE minidump_type =
      g_should_write_full_minidumps
          ? static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs |
                                       MiniDumpWithIndirectlyReferencedMemory)
          : MiniDumpNormal;

  WriteMiniDumpUsingExceptionInfo(info->ExceptionRecord->ExceptionCode, info,
                                  minidump_type);

  return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI ToolsExceptionFilter(EXCEPTION_POINTERS *info) {
  // Run their custom handler.
  g_exception_handler(info->ExceptionRecord->ExceptionCode, info);

  // (never gets here anyway)
  return EXCEPTION_EXECUTE_HANDLER;
}
}  // namespace

void EnableFullMinidumps(bool enable) {
  g_should_write_full_minidumps = enable;
}

void SetupDefaultToolsMinidumpHandler() {
  SetUnhandledExceptionFilter(DefaultToolsExceptionFilter);
}

ToolsExceptionHandler SetupToolsMinidumpHandler(ToolsExceptionHandler handler) {
  const auto old_handler{std::exchange(g_exception_handler, handler)};
  SetUnhandledExceptionFilter(ToolsExceptionFilter);
  return old_handler;
}
