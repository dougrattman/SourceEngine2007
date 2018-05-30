// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IVTEX_H
#define IVTEX_H

#include "base/include/base_types.h"
#include "base/include/macros.h"
#include "tier1/interface.h"

the_interface IVTex {
 public:
  // For use by command-line tools.
  virtual i32 VTex(i32 argc, ch * *argv) = 0;
  // For use by engine.
  virtual i32 VTex(CreateInterfaceFn filesystem_factory, const ch *game_dir,
                   i32 argc, ch **argv) = 0;
};

#define IVTEX_VERSION_STRING "VTEX_003"

#endif  // IVTEX_H
