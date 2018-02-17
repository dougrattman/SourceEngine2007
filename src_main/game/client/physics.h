// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: insulates client DLL from dependencies on vphysics

#ifndef PHYSICS_H
#define PHYSICS_H

#include "physics_shared.h"
#include "tier1/interface.h"

struct objectparams_t;
struct solid_t;

// HACKHACK: Make this part of IClientSystem somehow???
extern bool PhysicsDLLInit(CreateInterfaceFn physicsFactory);
extern void PhysicsReset();
extern void PhysicsSimulate();
extern float PhysGetSyncCreateTime();

#endif  // PHYSICS_H
