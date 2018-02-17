// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_IVGUIMATINFOVAR_H_
#define SOURCE_VGUI_IVGUIMATINFOVAR_H_

// wrapper for IMaterialVar
class IVguiMatInfoVar {
 public:
  virtual int GetIntValue(void) const = 0;
  virtual void SetIntValue(int val) = 0;

  // TODO: if you need to add more IMaterialVar functions add them here
};

#endif  // SOURCE_VGUI_IVGUIMATINFOVAR_H_