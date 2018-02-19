// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "physdll.h"

#include "filesystem_tools.h"

static CSysModule *pPhysicsModule = nullptr;

CreateInterfaceFn GetPhysicsFactory(void) {
  if (!pPhysicsModule) {
    pPhysicsModule = g_pFullFileSystem->LoadModule("vphysics.dll");
    if (!pPhysicsModule) return nullptr;
  }

  return Sys_GetFactory(pPhysicsModule);
}

void PhysicsDLLPath(const char *pPathname) {
  if (!pPhysicsModule) {
    pPhysicsModule = g_pFullFileSystem->LoadModule(pPathname);
  }
}
