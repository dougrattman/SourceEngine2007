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

#define SOURCE_MAX_PATH PATH_MAX

int main(int argc, char *argv[]) {
  // Must add 'bin' to the path....
  char *path_env = getenv("LD_LIBRARY_PATH");
  char cwd[SOURCE_MAX_PATH];
  if (!getcwd(cwd, sizeof(cwd))) {
    fprintf(stderr, "getcwd failed (%s)\n", strerror(errno));
    return errno;
  }

  char ld_library_path_env[4096];
  snprintf(ld_library_path_env, sizeof(ld_library_path_env) - 1,
           "LD_LIBRARY_PATH=%s/bin:%s", cwd, path_env);

  const int return_code = putenv(ld_library_path_env);
  if (return_code) {
    fprintf(stderr, "putenv for LD_LIBRARY_PATH failed (%s)\n",
            strerror(errno));
    return errno;
  }

  void *tier0_dl = dlopen("tier0.so.1", RTLD_NOW);
  if (!tier0_dl) {
    fprintf(stderr, "dlopen for tier0.so.1 failed (%s)\n", dlerror());
    return EXIT_FAILURE;
  }
  void *vstdlib_dl = dlopen("vstdlib.so.1", RTLD_NOW);
  if (!vstdlib_dl) {
    fprintf(stderr, "dlopen for vstdlib.so.1 failed (%s)\n", dlerror());
    return EXIT_FAILURE;
  }

  const char *dedicated_dl_path = "bin/dedicated.so.1";
  void *dedicated_dl = dlopen(dedicated_dl_path, RTLD_NOW);
  if (!dedicated_dl) {
    fprintf(stderr, "dlopen for %s failed (%s)\n", dedicated_dl_path,
            dlerror());
    return EXIT_FAILURE;
  }

  using DedicatedMain = int (*)(int argc, char *argv[]);
  const DedicatedMain main =
      reinterpret_cast<DedicatedMain>(dlsym(dedicated_dl, "DedicatedMain"));
  if (!main) {
    fprintf(stderr, "dlsym for DedicatedMain in %s failed (%s)\n",
            dedicated_dl_path, dlerror());
    return EXIT_FAILURE;
  }

  return_code = (*main)(argc, argv);

  dlclose(dedicated_dl);
  dlclose(vstdlib_dl);
  dlclose(tier0_dl);

  return return_code;
}
