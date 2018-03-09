// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "ilaunchabledll.h"
#include "tier0/include/icommandline.h"

char *GetLastErrorString() {
  static char error_msg[2048];

  char *system_msg;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&system_msg, 0, nullptr);

  strncpy_s(error_msg, system_msg, SOURCE_ARRAYSIZE(error_msg));
  LocalFree(system_msg);

  return error_msg;
}

int main(int argc, char **argv) {
  CommandLine()->CreateCmdLine(argc, argv);
  const char *dll_name = "vvis_dll.dll";

  CSysModule *module = Sys_LoadModule(dll_name);
  if (!module) {
    fprintf(stderr, "vvis error: Can't load '%s'.\n%s\n", dll_name,
            GetLastErrorString());
    return 1;
  }

  CreateInterfaceFn factory = Sys_GetFactory(module);
  if (!factory) {
    fprintf(stderr, "vvis error: Can't get factory from '%s'.\n%s\n", dll_name,
            GetLastErrorString());
    Sys_UnloadModule(module);
    return 2;
  }

  int code = 0;
  auto *dll = reinterpret_cast<ILaunchableDLL *>(
      factory(LAUNCHABLE_DLL_INTERFACE_VERSION, &code));
  if (!dll) {
    fprintf(stderr, "vvis error: Can't get IVVisDLL interface from '%s'.\n",
            dll_name);
    Sys_UnloadModule(module);
    return 3;
  }

  const int result_code = dll->main(argc, argv);
  Sys_UnloadModule(module);

  return result_code;
}
