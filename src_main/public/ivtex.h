// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef IVTEX_H
#define IVTEX_H

#include "tier1/interface.h"

class IVTex {
 public:
  // For use by command-line tools
  virtual int VTex(int argc, char **argv) = 0;
  // For use by engine
  virtual int VTex(CreateInterfaceFn filesystemFactory, const char *pGameDir,
                   int argc, char **argv) = 0;
};

#define IVTEX_VERSION_STRING "VTEX_003"

#endif  // IVTEX_H
