// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_main.h"

#include <direct.h>
#include <cstdio>

#include "build/include/build_config.h"
#include "dedicated_common.h"
#include "dedicated_steam_app.h"
#include "dedicated_steam_application.h"
#include "mathlib/mathlib.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/platform.h"
#include "tier0/include/vcrmode.h"
#include "tier1/strtools.h"

#ifndef OS_WIN
#define _chdir chdir
#endif

#ifdef OS_POSIX
extern char g_szEXEName[256];
#endif

class CVCRHelpers : public IVCRHelpers {
 public:
  virtual void ErrorMessage(const char *pMsg) {
    fprintf(stderr, "ERROR: %s\n", pMsg);
  }

  virtual void *GetMainWindow() { return 0; }
};

// Return the directory where this .exe is running from.
void GetBaseDirectory(char *base_dir, usize base_dir_size) {
  base_dir[0] = '\0';

  if (GetExeName(base_dir, base_dir_size)) {
    char *separator = strrchr(base_dir, CORRECT_PATH_SEPARATOR);
    if (separator && *separator) {
      *(separator + 1) = '\0';
    }

    const usize len = strlen(base_dir);
    if (len > 0 && (base_dir[len - 1] == '\\') || (base_dir[len - 1] == '/')) {
      base_dir[len - 1] = '\0';
    }
  }

  char const *new_base_dir = CommandLine()->CheckParm(
      source::tier0::command_line_switches::baseDirectory);
  if (new_base_dir) {
    strcpy_s(base_dir, base_dir_size, new_base_dir);
  }

  Q_strlower(base_dir, base_dir_size);
  Q_FixSlashes(base_dir);
}

// Main entry point for dedicated server, shared between win32 and linux.
int main(int argc, char **argv) {
#ifdef OS_POSIX
  strcpy(g_szEXEName, *argv);
  // Store off command line for argument searching
  BuildCmdLine(argc, argv);
#endif

  MathLib_Init(2.2f, 2.2f, 0.0f, 1);

  // Store off command line for argument searching
  CommandLine()->CreateCmdLine(VCRHook_GetCommandLine());
#ifndef OS_WIN
  Plat_SetCommandLine(CommandLine()->GetCmdLine());
#endif

  CVCRHelpers vcr_helpers;

  // Start VCR mode?
  const char *filename;
  if (CommandLine()->CheckParm("-vcrrecord", &filename)) {
    if (!VCRStart(filename, true, &vcr_helpers)) {
      Error("-vcrrecord: can't open '%s' for writing.\n", filename);
      return -1;
    }
  } else if (CommandLine()->CheckParm("-vcrplayback", &filename)) {
    if (!VCRStart(filename, false, &vcr_helpers)) {
      Error("-vcrplayback: can't open '%s' for reading.\n", filename);
      return -1;
    }
  }

  // Figure out the directory the executable is running from and make that be
  // the current working directory
  char base_dir[SOURCE_MAX_PATH];
  GetBaseDirectory(base_dir, std::size(base_dir));
  _chdir(base_dir);

  // Rehook the command line through VCR mode.
  CommandLine()->CreateCmdLine(VCRHook_GetCommandLine());

  if (CommandLine()->CheckParm("-usegh")) {
    Warning("No ghost injection allowed.");
  }

  DedicatedSteamApp dedicatedSystems;
  DedicatedSteamApplication steamApplication(&dedicatedSystems);

  return steamApplication.Run();
}
