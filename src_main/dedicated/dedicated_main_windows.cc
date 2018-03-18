// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <tuple>

#include "base/include/windows/windows_light.h"

#include <shellapi.h>

#include "appframework/AppFramework.h"
#include "base/include/windows/scoped_se_translator.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/minidump.h"
#include "tier0/include/vcrmode.h"

#include "dedicated_main.h"

namespace {
u32 g_se_code{0};
u32 GetSeCode() { return g_se_code; }
void SetSeCode(u32 new_se_code) { g_se_code = new_se_code; };

// Save structured exception code |se_code| and writes minidump with |se_info|.
void __cdecl SaveSeCodeAndWriteMiniDump(u32 se_code,
                                        EXCEPTION_POINTERS *se_info) {
  SetSeCode(se_code);
  WriteMiniDumpUsingExceptionInfo(se_code, se_info,
                                  MINIDUMP_TYPE::MiniDumpNormal);
}

// Run main server function with |argc| & |argv|.
int RunMain(_In_ int argc, _In_z_count_(argc) char **argv) {
  CommandLine()->CreateCmdLine(VCRHook_GetCommandLine());

  if (!Plat_IsInDebugSession() && !CommandLine()->FindParm("-nominidumps")) {
    const source::windows::ScopedSeTranslator scoped_se_translator{
        SaveSeCodeAndWriteMiniDump};

    // This try block allows the Se translator to work.
    try {
      return main(argc, argv);
    } catch (...) {
      return GetSeCode();
    }
  }

  return main(argc, argv);
}

// Convert wide |wargv| array of size |argc| to utf8.
std::tuple<i32, ch **, u32> ConvertWideCharToUtf8(_In_z_count_(argc)
                                                      wch **wargv,
                                                  i32 argc) {
  // Convert argv to UTF8.
  ch **argv{new ch *[argc + 1]};
  if (!argv) return {0, nullptr, ERROR_OUTOFMEMORY};

  for (i32 i{0}; i < argc; i++) {
    // Compute the size of the required buffer.
    const i32 size{WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0,
                                       nullptr, nullptr)};
    if (size == 0) {
      for (i32 j{0}; j < i; ++j) delete[] argv[j];
      delete[] argv;

      return {0, nullptr, GetLastError()};
    }

    // Do the actual conversion.
    argv[i] = new ch[size];
    if (!argv[i]) {
      for (i32 j{0}; j < i; ++j) delete[] argv[j];
      delete[] argv;

      return {0, nullptr, ERROR_OUTOFMEMORY};
    }

    const i32 result{WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i],
                                         size, nullptr, nullptr)};
    if (result == 0) {
      for (i32 j{0}; j <= i; ++j) delete[] argv[j];
      delete[] argv;

      return {0, nullptr, GetLastError()};
    }
  }

  argv[argc] = nullptr;

  return {argc, argv, NOERROR};
}

// Get unix-like argc, argv from windows wide commdna line |command_line|.
std::tuple<i32, ch **, u32> CommandLineToArgv(_In_z_ wch *command_line) {
  i32 argc{0};
  wch **wargv{CommandLineToArgvW(command_line, &argc)};

  if (wargv != nullptr) {
    auto result = ConvertWideCharToUtf8(wargv, argc);

    LocalFree(wargv);

    return result;
  }

  return std::make_tuple(0, nullptr,
                         wargv != nullptr ? NOERROR : GetLastError());
}
}  // namespace

// Here app go.
SOURCE_API_EXPORT int DedicatedMain(_In_ HINSTANCE instance,
                                    _In_ int /*cmd_show*/) {
  if (instance == nullptr) return ERROR_INVALID_HANDLE;

  SetAppInstance(instance);

  auto [argc, argv, error_code] = CommandLineToArgv(GetCommandLineW());

  return error_code == NOERROR ? RunMain(argc, argv) : error_code;
}
