// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_filesystem.h"

#include "../filesystem/basefilesystem.h"
#include "filesystem.h"
#include "tier0/include/icommandline.h"
#include "tier1/strtools.h"

extern IFileSystem *g_pFileSystem;
extern IBaseFileSystem *g_pBaseFileSystem;
extern IFileSystem *g_pFileSystemSteam;
extern IBaseFileSystem *g_pBaseFileSystemSteam;

// We have two CBaseFilesystem objects, stdio and steam, so we have to manage
// the BaseFileSystem() accessor ourselves.
#ifdef OS_WIN
CBaseFileSystem *BaseFileSystem_Steam();
CBaseFileSystem *BaseFileSystem_Stdio();

static CBaseFileSystem *s_pBaseFileSystem{nullptr};
CBaseFileSystem *BaseFileSystem() { return s_pBaseFileSystem; }
#endif

// Implement our own special factory that we don't export outside of the DLL, to
// stop people being able to get a pointer to a FILESYSTEM_INTERFACE_VERSION
// stdio interface.
void *FileSystemFactory(const char *interface_name, int *return_code) {
  IFileSystem *file_system{g_pFileSystem};
  IBaseFileSystem *base_file_system{g_pBaseFileSystem};

#ifdef OS_WIN
  if (CommandLine()->FindParm("-steam")) {
    s_pBaseFileSystem = BaseFileSystem_Steam();
    file_system = g_pFileSystemSteam;
    base_file_system = g_pBaseFileSystemSteam;
  } else {
    s_pBaseFileSystem = BaseFileSystem_Stdio();
  }
#endif

  if (!_stricmp(interface_name, FILESYSTEM_INTERFACE_VERSION)) {
    if (return_code) *return_code = IFACE_OK;

    return file_system;
  }

  if (!_stricmp(interface_name, BASEFILESYSTEM_INTERFACE_VERSION)) {
    if (return_code) *return_code = IFACE_OK;

    return base_file_system;
  }

  if (return_code) *return_code = IFACE_FAILED;

  return nullptr;
}
