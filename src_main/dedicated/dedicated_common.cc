// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_common.h"

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include "tier0/include/icommandline.h"
#include "tier0/include/platform.h"
#include "tier1/strtools.h"

bool g_bVGui{false};

bool GetExeName(ch *exe_name, usize size) {
#ifdef _WIN32
  return GetModuleFileNameA(GetModuleHandleW(nullptr), exe_name, size);
#else
  extern ch g_szEXEName[256];
  strcpy(exe_name, g_szEXEName);
  return true;
#endif
}

// Return the directory where this .exe is running from.
const ch *GetExeDirectory() {
  static ch exe_dir[SOURCE_MAX_PATH];
  exe_dir[0] = '\0';

  if (!GetExeName(exe_dir, SOURCE_ARRAYSIZE(exe_dir))) return nullptr;

  ch *slash{strrchr(exe_dir, '\\')};
  ch *reverse_slash{strrchr(exe_dir, '/')};

  if (reverse_slash > slash) slash = reverse_slash;
  if (slash) *slash = '\0';

  // Return the bin directory as the executable dir if it's not in there
  // because that's really where we're running from...
  const usize exe_dir_size{strlen(exe_dir)};
  if (exe_dir[exe_dir_size - 4] != CORRECT_PATH_SEPARATOR ||
      exe_dir[exe_dir_size - 3] != 'b' || exe_dir[exe_dir_size - 2] != 'i' ||
      exe_dir[exe_dir_size - 1] != 'n') {
    Q_strncat(exe_dir, "\\bin", SOURCE_ARRAYSIZE(exe_dir), COPY_ALL_CHARACTERS);
    Q_FixSlashes(exe_dir);
  }

  return exe_dir;
}

// Return the directory where this .exe is running from.
const ch *GetBaseDirectory() {
  static ch base_dir[SOURCE_MAX_PATH];

  const ch *new_base_dir{CommandLine()->CheckParm("-basedir")};
  if (new_base_dir) return new_base_dir;

  base_dir[0] = '\0';

  const ch *exe_dir{GetExeDirectory()};

  if (exe_dir) {
    strcpy_s(base_dir, exe_dir);
    const usize exe_dir_size{strlen(base_dir)};

    if (base_dir[exe_dir_size - 3] == 'b' &&
        base_dir[exe_dir_size - 2] == 'i' &&
        base_dir[exe_dir_size - 1] == 'n') {
      base_dir[exe_dir_size - 4] = '\0';
    }
  }

  return base_dir;
}
