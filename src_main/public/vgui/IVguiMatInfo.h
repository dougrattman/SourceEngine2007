// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_IVGUIMATINFO_H_
#define SOURCE_VGUI_IVGUIMATINFO_H_

#ifdef _WIN32
#pragma once
#endif

#include "IVguiMatInfoVar.h"
#include "base/include/compiler_specific.h"

// wrapper for IMaterial
the_interface IVguiMatInfo {
 public:
  // make sure to delete the returned object after use!
  virtual IVguiMatInfoVar *FindVarFactory(const char *varName, bool *found) = 0;
  virtual int GetNumAnimationFrames() = 0;

  // TODO: if you need to add more IMaterial functions add them here
};

#endif  // SOURCE_VGUI_IVGUIMATINFO_H_