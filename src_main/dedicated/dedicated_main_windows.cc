// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "base/include/windows/windows_light.h"

#include <eh.h>
#include <shellapi.h>

#include "appframework/AppFramework.h"
#include "isys.h"
#include "tier0/include/icommandline.h"
#include "tier1/strtools.h"

#include "base/windows/include/scoped_winsock_initializer.h"

static source::windows::ScopedWinsockInitializer *scoped_winsock_initializer =
    nullptr;

bool NET_Init() {
  scoped_winsock_initializer = new source::windows::ScopedWinsockInitializer{
      source::windows::WinsockVersion::Version2_2};
  const DWORD error_code = scoped_winsock_initializer->error_code();

  if (error_code != ERROR_SUCCESS) {
    char error_message[256];
    Q_snprintf(error_message, SOURCE_ARRAYSIZE(error_message),
               "Winsock 2.2 unavailable, error code 0x%x...", error_code);
    sys->Printf("%s", error_message);
    Plat_DebugString(error_message);
    return false;
  }

  return true;
}

void NET_Shutdown() {
  delete scoped_winsock_initializer;
  scoped_winsock_initializer = nullptr;
}

void MiniDumpFunction(unsigned int nExceptionCode,
                      EXCEPTION_POINTERS *pException) {
#ifndef NO_STEAM
  SteamAPI_WriteMiniDump(nExceptionCode, pException, 0);
#endif
}

int main(int argc, char **argv);

// Main dedicated server entry point.
SOURCE_API_EXPORT int DedicatedMain(HINSTANCE instance, int show_cmd) {
  SetAppInstance(instance);

  CommandLine()->CreateCmdLine(VCRHook_GetCommandLine());

  int return_code, argc = -1;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!Plat_IsInDebugSession() && !CommandLine()->FindParm("-nominidumps")) {
    _set_se_translator(MiniDumpFunction);

    try  // this try block allows the SE translator to work
    {
      return_code = main(argc, (char **)argv);
    } catch (...) {
      return -1;
    }
  } else {
    return_code = main(argc, (char **)argv);
  }

  LocalFree(argv);
  return return_code;
}
