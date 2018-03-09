// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Basic header for using vgui.

#ifndef SOURCE_VGUI_VGUI_H_
#define SOURCE_VGUI_VGUI_H_

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = unsigned long;

// Do this in GOLDSRC only!!!
//#define Assert assert

namespace vgui {
// handle to an internal vgui panel
// this is the only handle to a panel that is valid across dll boundaries
using VPANEL = unsigned int;

// handles to vgui objects
// NULL values signify an invalid value
using HScheme = unsigned long;
using HTexture = unsigned long;
using HCursor = unsigned long;

using HPanel = unsigned long;
const HPanel INVALID_PANEL = 0xffffffff;

using HFont = unsigned long;
const HFont INVALID_FONT = 0;  // the value of an invalid font handle
}  // namespace vgui

#include "tier1/strtools.h"

#endif  // SOURCE_VGUI_VGUI_H_
