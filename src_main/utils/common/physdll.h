// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef PHYSDLL_H
#define PHYSDLL_H

#ifdef __cplusplus
#include "vphysics_interface.h"
class IPhysics;
class IPhysicsCollision;

extern CreateInterfaceFn GetPhysicsFactory(void);

extern "C" {
#endif

// tools need to force the path
void PhysicsDLLPath(const char *pPathname);

#ifdef __cplusplus
}
#endif

#endif  // PHYSDLL_H
