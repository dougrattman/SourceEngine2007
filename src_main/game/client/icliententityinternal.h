// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ICLIENTENTITYINTERNAL_H
#define ICLIENTENTITYINTERNAL_H

#include "ClientLeafSystem.h"
#include "IClientEntity.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

class ClientClass;

//-----------------------------------------------------------------------------
// represents a handle used only by the client DLL
//-----------------------------------------------------------------------------

typedef CBaseHandle ClientEntityHandle_t;
typedef unsigned short SpatialPartitionHandle_t;

#endif  // ICLIENTENTITYINTERNAL_H
