// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: names mouse button inputs
// NOTE: Button codes also contain key codes, but we won't worry about that

#ifndef SOURCE_VGUI_MOUSECODE_H_
#define SOURCE_VGUI_MOUSECODE_H_

#ifdef _WIN32
#pragma once
#endif

#include "inputsystem/ButtonCode.h"

namespace vgui {
typedef ButtonCode_t MouseCode;
}

#endif  // SOURCE_VGUI_MOUSECODE_H_
