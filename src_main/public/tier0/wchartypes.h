// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:	All of our code is completely Unicode. Instead of char, you
// should use wchar as explained below.

#ifndef SOURCE_TIER0_WCHARTYPES_H_
#define SOURCE_TIER0_WCHARTYPES_H_

// Temporarily turn off Valve defines
#include "tier0/valve_off.h"

#ifndef _WCHAR_T_DEFINED
using wchar_t = unsigned short;
#define _WCHAR_T_DEFINED
#endif

// wchar is a single character of text (currently 16 bits, as all of our text is
// Unicode). Use this whenever you mean a piece of text (for example, in a
// string).
using wchar = wchar_t;

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
#include "tier0/valve_on.h"

#endif  // SOURCE_TIER0_WCHARTYPES_H_
