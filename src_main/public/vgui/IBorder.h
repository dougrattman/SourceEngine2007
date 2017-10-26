// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_IBORDER_H_
#define SOURCE_VGUI_IBORDER_H_

#include "vgui/VGUI.h"

class KeyValues;

namespace vgui {
class IScheme;

// Purpose: Interface to panel borders
// Borders have a close relationship with panels
// They are the edges of the panel.
class IBorder {
 public:
  virtual void Paint(VPANEL panel) = 0;
  virtual void Paint(int x0, int y0, int x1, int y1) = 0;
  virtual void Paint(int x0, int y0, int x1, int y1, int breakSide,
                     int breakStart, int breakStop) = 0;
  virtual void SetInset(int left, int top, int right, int bottom) = 0;
  virtual void GetInset(int &left, int &top, int &right, int &bottom) = 0;
  virtual void ApplySchemeSettings(IScheme *pScheme,
                                   KeyValues *inResourceData) = 0;
  virtual const char *GetName() = 0;
  virtual void SetName(const char *name) = 0;

  enum backgroundtype_e {
    BACKGROUND_FILLED,
    BACKGROUND_TEXTURED,
    BACKGROUND_ROUNDEDCORNERS,
  };
  virtual backgroundtype_e GetBackgroundType() = 0;

  enum sides_e { SIDE_LEFT = 0, SIDE_TOP = 1, SIDE_RIGHT = 2, SIDE_BOTTOM = 3 };
};
}  // namespace vgui

#endif  // SOURCE_VGUI_IBORDER_H_
