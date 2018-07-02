// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#include "filesystem_init.h"

#include "base/include/check.h"
#include "tier0/include/platform.h"
#include "tier2/tier2.h"

static CSysModule *g_pFullFileSystemModule{nullptr};

void *DefaultCreateInterfaceFn(const char *pName, int *pReturnCode) {
  if (pReturnCode) *pReturnCode = 0;

  return nullptr;
}

void InitDefaultFileSystem() {
  AssertMsg(!g_pFullFileSystem, "Already set up the file system");

  CHECK(
      Sys_LoadInterface("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION,
                        &g_pFullFileSystemModule, (void **)&g_pFullFileSystem),
      -1);
  CHECK(g_pFullFileSystem->Connect(DefaultCreateInterfaceFn), -1);
  CHECK(g_pFullFileSystem->Init() == INIT_OK, -1);

  g_pFullFileSystem->RemoveAllSearchPaths();
  g_pFullFileSystem->AddSearchPath("", "LOCAL", PATH_ADD_TO_HEAD);
}

void ShutdownDefaultFileSystem() {
  AssertMsg(g_pFullFileSystem, "File system not set up");

  g_pFullFileSystem->Shutdown();
  g_pFullFileSystem->Disconnect();

  Sys_UnloadModule(g_pFullFileSystemModule);
  g_pFullFileSystemModule = nullptr;
}
