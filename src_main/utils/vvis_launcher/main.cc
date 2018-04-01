// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "base/include/unique_module_ptr.h"
#include "build/include/build_config.h"
#include "ilaunchabledll.h"
#include "tier0/include/icommandline.h"

int main(int argc, char **argv) {
  CommandLine()->CreateCmdLine(argc, argv);

#ifdef OS_WIN
  const ch *library_name{"vvis_dll.dll"};

  auto [library, errno_info] = source::unique_module_ptr::from_load_library(
      L"vvis_dll.dll", LOAD_WITH_ALTERED_SEARCH_PATH);
#elif OS_POSIX
  const ch *library_name{"vvis.so.0"};

  auto [library, errno_info] =
      source::unique_module_ptr::from_load_library(library_name, RTLD_LAZY);
#else
#error Please, add support of your platform to vvis/main.cc
#endif

  if (!errno_info.is_success()) {
    fprintf_s(stderr, "vvis error: Can't load '%s'.\n%s\n", library_name,
              errno_info.description);
    return errno_info.code;
  }

  CreateInterfaceFn factory;
  std::tie(factory, errno_info) =
      library.get_address_as<CreateInterfaceFn>(CREATEINTERFACE_PROCNAME);
  if (!errno_info.is_success()) {
    fprintf_s(stderr, "vvis error: Can't get factory from '%s'.\n%s\n",
              library_name, errno_info.description);
    return errno_info.code;
  }

  int code = 0;
  auto *dll = reinterpret_cast<ILaunchableDLL *>(
      factory(LAUNCHABLE_DLL_INTERFACE_VERSION, &code));
  if (!dll) {
    fprintf_s(stderr,
              "vvis error: Can't get ILaunchableDLL interface from '%s'.\n",
              library_name);
    return code;
  }

  return dll->main(argc, argv);
}
