// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_steam_application.h"

#include "dedicated_filesystem.h"
#include "filesystem.h"
#include "vstdlib/cvar.h"

bool DedicatedSteamApplication::Create() {
  AppModule_t cvar_module = LoadModule(VStdLib_GetICVarFactory());
  if (cvar_module == APP_MODULE_INVALID) return false;

  AddSystem(cvar_module, CVAR_INTERFACE_VERSION);

  AppModule_t file_system_module = LoadModule(FileSystemFactory);
  if (file_system_module == APP_MODULE_INVALID) return false;

  m_pFileSystem =
      AddSystem<IFileSystem>(file_system_module, FILESYSTEM_INTERFACE_VERSION);

  if (m_pFileSystem) return true;

  Warning("Unable to load the File system interface %s.\n",
          FILESYSTEM_INTERFACE_VERSION);
  return false;
}
