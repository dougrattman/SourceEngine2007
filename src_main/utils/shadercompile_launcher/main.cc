// Copyright © 1996-2005, Valve Corporation, All rights reserved.

#include <conio.h>
#include <direct.h>
#include <windows.h>
#include <cstdio>

#include "ishadercompiledll.h"
#include "tier0/include/icommandline.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"

char *GetLastErrorString() {
  static char error_msg[2048];

  LPVOID msg_buffer;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&msg_buffer, 0, NULL);

  strncpy_s(error_msg, (char *)msg_buffer, ARRAYSIZE(error_msg));
  LocalFree(msg_buffer);

  error_msg[sizeof(error_msg) - 1] = '\0';
  return error_msg;
}

void MakeFullPath(const char *pIn, char *pOut, int outLen) {
  if (pIn[0] == '/' || pIn[0] == '\\' || pIn[1] == ':') {
    // It's already a full path.
    Q_strncpy(pOut, pIn, outLen);
  } else {
    _getcwd(pOut, outLen);
    Q_strncat(pOut, "\\", outLen, COPY_ALL_CHARACTERS);
    Q_strncat(pOut, pIn, outLen, COPY_ALL_CHARACTERS);
  }
}

int main(int argc, char *argv[]) {
  CommandLine()->CreateCmdLine(argc, argv);

  char full_path[512], redirect_path[512];
  MakeFullPath(argv[0], full_path, ARRAYSIZE(full_path));
  Q_StripFilename(full_path);
  Q_snprintf(redirect_path, ARRAYSIZE(redirect_path), "%s\\%s", full_path,
             "vrad.redirect");

  char redirect_module_name[512];
  // First, look for vrad.redirect and load the dll specified in there if
  // possible.
  CSysModule *redirect_module = nullptr;

  FILE *fp;
  if (!fopen_s(&fp, redirect_path, "rt")) {
    if (fgets(redirect_module_name, ARRAYSIZE(redirect_module_name), fp)) {
      char *pEnd = strstr(redirect_module_name, "\n");
      if (pEnd) *pEnd = '\0';

      redirect_module = Sys_LoadModule(redirect_module_name);

      if (redirect_module)
        fprintf(stdout,
                "Loaded alternate VRAD DLL (%s) specified in vrad.redirect.\n",
                redirect_module_name);
      else
        fprintf(stderr, "Can't find '%s' specified in vrad.redirect.\n",
                redirect_module_name);
    }

    fclose(fp);
  }

  // If it didn't load the module above, then use the
  if (!redirect_module) {
    strcpy_s(redirect_module_name, "shadercompile_dll.dll");
    redirect_module = Sys_LoadModule(redirect_module_name);
  }

  if (!redirect_module) {
    fprintf(stderr, "Can't load '%s'.\n%s", redirect_module_name,
            GetLastErrorString());
    return 1;
  }

  const CreateInterfaceFn create_interface = Sys_GetFactory(redirect_module);
  if (!create_interface) {
    fprintf(stderr, "Can't get factory from %s.\n", redirect_module_name);
    Sys_UnloadModule(redirect_module);
    return 2;
  }

  int return_code = 0;
  auto *shader_compile_dll = reinterpret_cast<IShaderCompileDLL *>(
      (*create_interface)(SHADER_COMPILE_INTERFACE_VERSION, &return_code));
  if (!shader_compile_dll) {
    fprintf(stderr,
            "vrad_launcher error: can't get IShaderCompileDLL interface from "
            "%s.\n",
            redirect_module_name);
    Sys_UnloadModule(redirect_module);
    return 3;
  }

  return_code = shader_compile_dll->main(argc, argv);
  Sys_UnloadModule(redirect_module);

  return return_code;
}
