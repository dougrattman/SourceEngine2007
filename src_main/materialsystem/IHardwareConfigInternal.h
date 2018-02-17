// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IHARDWARECONFIGINTERNAL_H
#define IHARDWARECONFIGINTERNAL_H

#include "materialsystem/imaterialsystemhardwareconfig.h"

//-----------------------------------------------------------------------------
// Material system configuration
//-----------------------------------------------------------------------------
class IHardwareConfigInternal : public IMaterialSystemHardwareConfig {
 public:
  // Gets at the HW specific shader DLL name
  virtual const char *GetHWSpecificShaderDLLName() const = 0;
};

#endif  // IHARDWARECONFIGINTERNAL_H
