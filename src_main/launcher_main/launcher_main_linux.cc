// Copyright © 1996-2005, Valve Corporation, All rights reserved.
//
// Purpose: This is just a little redirection tool to get all the dls in bin.

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[]) {
  // Must add 'bin' to the path....
  char *path_env = getenv("LD_LIBRARY_PATH");
  char cwd[MAX_PATH];
  if (!getcwd(cwd, sizeof(cwd))) {
    fprintf(stderr, "getcwd failed (%s)\n", strerror(errno));
    return errno;
  }

  char ld_library_path_env[4096];
  snprintf(ld_library_path_env, sizeof(ld_library_path_env) - 1,
           "LD_LIBRARY_PATH=%s/bin:%s", cwd, path_env);

  if (putenv(ld_library_path_env)) {
    fprintf(stderr, "putenv for LD_LIBRARY_PATH failed (%s)\n",
            strerror(errno));
    return errno;
  }

  void *tier0_dl = dlopen("tier0_i486.so", RTLD_NOW);
  if (!tier0_dl) {
    fprintf(stderr, "dlopen for tier0_i486.so failed (%s)\n", dlerror());
    return EXIT_FAILURE;
  }
  void *vstdlib_dl = dlopen("vstdlib_i486.so", RTLD_NOW);
  if (!vstdlib_dl) {
    fprintf(stderr, "dlopen for vstdlib_i486.so failed (%s)\n", dlerror());
    return EXIT_FAILURE;
  }

  const char *launcher_dl_path = "bin/launcher_i486.so";
  void *launcher_dl = dlopen(launcher_dl_path, RTLD_NOW);
  if (!launcher_dl) {
    fprintf(stderr, "dlopen for %s failed (%s)\n", launcher_dl_path, dlerror());
    return EXIT_FAILURE;
  }

  using LauncherMain = int (*)(int argc, char *argv[]);
  const LauncherMain main =
      reinterpret_cast<LauncherMain>(dlsym(launcher_dl, "LauncherMain"));
  if (!main) {
    fprintf(stderr, "dlsym for LauncherMain in %s failed (%s)\n",
            launcher_dl_path, dlerror());
    return EXIT_FAILURE;
  }

  const int return_code = (*main)(argc, argv);

  dlclose(launcher_dl);
  dlclose(vstdlib_dl);
  dlclose(tier0_dl);

  return return_code;
}
