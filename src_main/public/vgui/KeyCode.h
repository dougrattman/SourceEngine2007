// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: this is a map for virtual key codes
// virtual key codes may exist outside this range for other languages
// NOTE: Button codes also contain mouse codes, but we won't worry about that.

#ifndef SOURCE_VGUI_KEYCODE_H_
#define SOURCE_VGUI_KEYCODE_H_

#include "inputsystem/ButtonCode.h"

namespace vgui {
using KeyCode = ButtonCode_t;
}

#endif  // SOURCE_VGUI_KEYCODE_H_
