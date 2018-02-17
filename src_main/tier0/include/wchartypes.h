// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: All of our code is completely Unicode. Instead of ch, you
// should use wchar as explained below.

#ifndef SOURCE_TIER0_INCLUDE_WCHARTYPES_H_
#define SOURCE_TIER0_INCLUDE_WCHARTYPES_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

// Temporarily turn off Valve defines
#include "tier0/include/valve_off.h"

#ifndef _WCHAR_T_DEFINED
using wchar_t = u16;
#define _WCHAR_T_DEFINED
#endif

// __WFILE__
// This is a Unicode version of __FILE__
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)

#ifdef STEAM
#ifndef _UNICODE
#define FORCED_UNICODE
#endif
#define _UNICODE
#endif

#ifdef FORCED_UNICODE
#undef _UNICODE
#endif

// Turn valve defines back on
#include "tier0/include/valve_on.h"

#endif  // SOURCE_TIER0_INCLUDE_WCHARTYPES_H_
