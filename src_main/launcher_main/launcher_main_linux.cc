// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: This is just a little redirection tool to get all the dls in bin.

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "base/include/unique_module_ptr.h"

namespace {
// Print shared library |dl_path| load error to stderr and return error_code.
int no_dl_load_error(const str &dl_path) {
  fprintf(stderr, "dlopen for '%s' failed (%s).\n", dl_path.c_str(), dlerror());
  return -EXIT_FAILURE;
}

// Print no |address_name| error for shared library |dl_path| to stderr
// and return error_code.
int no_dl_address_error(const str &dl_path, const ch *address_name) {
  fprintf(stderr, "dlsym for '%s' in '%s' failed (%s).\n", address_name,
          dl_path.c_str(), dlerror());
  return -EXIT_FAILURE;
}
}  // namespace

int main(int argc, char *argv[]) {
  ch cwd[PATH_MAX];
  if (SOURCE_UNLIKELY(!getcwd(cwd, sizeof(cwd)))) {
    fprintf(stderr, "getcwd failed (%s).\n", strerror(errno));
    return -errno;
  }

  // All our binaries live in ./bin/.
  str bin_dir_path{cwd};
  bin_dir_path += "/bin/";

  const str launcher_module_path{bin_dir_path + LIBLAUNCHER_SO};
  const auto launcher_module{source::unique_module_ptr::from_load_library(
      launcher_module_path, RTLD_LAZY)};

  if (SOURCE_LIKELY(!!launcher_module)) {
    using LauncherMain = int (*)(int, char *[]);
    const char *kLauncherMainName{"LauncherMain"};
    const auto main =
        launcher_module.get_address_as<LauncherMain>(kLauncherMainName);

    return SOURCE_LIKELY(main != nullptr)
               ? (*main)(argc, argv)
               : no_dl_address_error(launcher_module_path, kLauncherMainName);
  }

  return no_dl_load_error(launcher_module_path);
}
