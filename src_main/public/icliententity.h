// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ICLIENTENTITY_H
#define ICLIENTENTITY_H

#include "iclientnetworkable.h"
#include "iclientrenderable.h"
#include "iclientthinkable.h"

struct Ray_t;
class CGameTrace;
typedef CGameTrace trace_t;
class CMouthInfo;
class IClientEntityInternal;
struct SpatializationInfo_t;

//-----------------------------------------------------------------------------
// Purpose: All client entities must implement this interface.
//-----------------------------------------------------------------------------
the_interface IClientEntity : public IClientUnknown,
                               public IClientRenderable,
                               public IClientNetworkable,
                               public IClientThinkable {
 public:
  // Delete yourself.
  virtual void Release(void) = 0;

  // Network origin + angles
  virtual const Vector& GetAbsOrigin(void) const = 0;
  virtual const QAngle& GetAbsAngles(void) const = 0;

  virtual CMouthInfo* GetMouth(void) = 0;

  // Retrieve sound spatialization info for the specified sound on this entity
  // Return false to indicate sound is not audible
  virtual bool GetSoundSpatialization(SpatializationInfo_t & info) = 0;
};

#endif  // ICLIENTENTITY_H
