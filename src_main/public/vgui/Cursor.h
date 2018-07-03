// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Holds the enumerated list of default cursors.

#ifndef SOURCE_VGUI_CURSOR_H_
#define SOURCE_VGUI_CURSOR_H_

#ifdef _WIN32
#pragma once
#endif

#include "vgui/VGUI.h"

namespace vgui {
enum CursorCode {
  dc_user,
  dc_none,
  dc_arrow,
  dc_ibeam,
  dc_hourglass,
  dc_waitarrow,
  dc_crosshair,
  dc_up,
  dc_sizenwse,
  dc_sizenesw,
  dc_sizewe,
  dc_sizens,
  dc_sizeall,
  dc_no,
  dc_hand,
  dc_blank,  // don't show any custom vgui cursor, just let windows do it stuff
             // (for HTML widget)
  dc_last,
};
using HCursor = unsigned long;
}  // namespace vgui

#endif  // SOURCE_VGUI_CURSOR_H_
