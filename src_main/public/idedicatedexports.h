// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IDEDICATEDEXPORTS_H
#define IDEDICATEDEXPORTS_H

#include "appframework/IAppSystem.h"
#include "tier1/interface.h"

the_interface IDedicatedExports : public IAppSystem {
 public:
  virtual void Sys_Printf(char *text) = 0;
  virtual void RunServer() = 0;
};

#define VENGINE_DEDICATEDEXPORTS_API_VERSION \
  "VENGINE_DEDICATEDEXPORTS_API_VERSION003"

#endif  // IDEDICATEDEXPORTS_H
