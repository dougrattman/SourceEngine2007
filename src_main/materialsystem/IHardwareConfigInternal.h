// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATERIALSYSTEM_IHARDWARECONFIGINTERNAL_H_
#define SOURCE_MATERIALSYSTEM_IHARDWARECONFIGINTERNAL_H_

#include "materialsystem/imaterialsystemhardwareconfig.h"

// Material system configuration
the_interface IHardwareConfigInternal : public IMaterialSystemHardwareConfig {
 public:
  // Gets at the HW specific shader DLL name
  virtual const char *GetHWSpecificShaderDLLName() const = 0;
};

#endif  // SOURCE_MATERIALSYSTEM_IHARDWARECONFIGINTERNAL_H_
