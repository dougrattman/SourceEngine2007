// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <cstdio>

#include "ilaunchabledll.h"
#include "tier1/interface.h"

int main(int argc, char **argv) {
  const char *module_name{"vtex_dll.dll"};

  CSysModule *module = Sys_LoadModule(module_name);
  if (!module) {
    fprintf(stderr, "Can't load %s.", module_name);
    return -1;
  }

  CreateInterfaceFn fn = Sys_GetFactory(module);
  if (!fn) {
    fprintf(stderr, "Can't get factory from %s.", module_name);
    Sys_UnloadModule(module);
    return -2;
  }

  ILaunchableDLL *launchable_dll =
      (ILaunchableDLL *)fn(LAUNCHABLE_DLL_INTERFACE_VERSION, nullptr);
  if (!launchable_dll) {
    fprintf(stderr, "Can't get '%s' interface from %s.",
            LAUNCHABLE_DLL_INTERFACE_VERSION, module_name);
    Sys_UnloadModule(module);
    return -3;
  }

  const int return_code = launchable_dll->main(argc, argv);

  Sys_UnloadModule(module);

  return iRet;
}
