// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_IVGUIMATINFOVAR_H_
#define SOURCE_VGUI_IVGUIMATINFOVAR_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/compiler_specific.h"

// wrapper for IMaterialVar
the_interface IVguiMatInfoVar {
 public:
  virtual int GetIntValue() const = 0;
  virtual void SetIntValue(int val) = 0;
};

#endif  // SOURCE_VGUI_IVGUIMATINFOVAR_H_