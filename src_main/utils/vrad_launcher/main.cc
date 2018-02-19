// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "stdafx.h"

#include <direct.h>
#include "tier0/include/icommandline.h"
#include "tier1/strtools.h"

namespace {
char *GetLastErrorString() {
  static char error_msg[2048];

  char *msg_buffer;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&msg_buffer, 0, NULL);

  strncpy_s(error_msg, msg_buffer, ARRAYSIZE(error_msg));
  LocalFree(msg_buffer);

  return error_msg;
}

void MakeFullPath(const char *in, char *out, size_t out_size) {
  if (in[0] == '/' || in[0] == '\\' || in[1] == ':') {
    // It's already a full path.
    Q_strncpy(out, in, out_size);
  } else {
    _getcwd(out, out_size);
    Q_strncat(out, "\\", out_size, COPY_ALL_CHARACTERS);
    Q_strncat(out, in, out_size, COPY_ALL_CHARACTERS);
  }
}
}  // namespace

int main(int argc, char **argv) {
  CommandLine()->CreateCmdLine(argc, argv);

  // check whether they used the -both switch. If this is specified, vrad will
  // be run twice, once with -hdr and once without
  int both_arg = 0;
  for (int i = 1; i < argc; i++) {
    if (Q_stricmp(argv[i], "-both") == 0) {
      both_arg = i;
    }
  }

  char full_path[512], redirect_file_name[512];
  MakeFullPath(argv[0], full_path, ARRAYSIZE(full_path));
  Q_StripFilename(full_path);
  Q_snprintf(redirect_file_name, ARRAYSIZE(redirect_file_name), "%s\\%s",
             full_path, "vrad.redirect");

  char dll_name[512];
  // First, look for vrad.redirect and load the dll specified in there if
  // possible.
  CSysModule *module = nullptr;

  FILE *redirect_file = fopen(redirect_file_name, "rt");
  if (redirect_file) {
    if (fgets(dll_name, ARRAYSIZE(dll_name), redirect_file)) {
      char *end = strstr(dll_name, "\n");
      if (end) *end = '\0';

      module = Sys_LoadModule(dll_name);

      if (module)
        printf(
            "vrad info: Loaded alternate VRAD dll '%s' specified in "
            "vrad.redirect.\n",
            dll_name);
      else
        fprintf(stderr,
                "vrad error: Can't find '%s' specified in vrad.redirect.\n",
                dll_name);
    }

    fclose(redirect_file);
  }

  int result_code = 0;

  for (int mode = 0; mode < 2; mode++) {
    if (mode && (!both_arg)) continue;

    // If it didn't load the module above, then use the
    if (!module) {
      strcpy(dll_name, "vrad_dll.dll");
      module = Sys_LoadModule(dll_name);
    }

    if (!module) {
      fprintf(stderr, "vrad error: Can't load '%s'.\n%s\n", dll_name,
              GetLastErrorString());
      return 1;
    }

    CreateInterfaceFn factory = Sys_GetFactory(module);
    if (!factory) {
      fprintf(stderr, "vrad error: Can't get factory from '%s'.\n%s\n",
              dll_name, GetLastErrorString());
      Sys_UnloadModule(module);
      return 2;
    }

    int return_code = 0;
    auto *vrad_dll = reinterpret_cast<IVRadDLL *>(
        factory(VRAD_INTERFACE_VERSION, &return_code));
    if (!vrad_dll) {
      fprintf(stderr, "vrad error: Can't get IVRadDLL interface from '%s'.\n",
              dll_name);
      Sys_UnloadModule(module);
      return 3;
    }

    if (both_arg) strcpy(argv[both_arg], mode ? "-hdr" : "-ldr");

    result_code = vrad_dll->main(argc, argv);

    Sys_UnloadModule(module);
    module = nullptr;
  }

  return result_code;
}
