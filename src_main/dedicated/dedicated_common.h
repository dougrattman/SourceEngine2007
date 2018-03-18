// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order.

#ifndef SOURCE_DEDICATED_DEDICATED_COMMON_H_
#define SOURCE_DEDICATED_DEDICATED_COMMON_H_

#include "Base/include/base_types.h"
#include "tier0/include/dbg.h"

const ch *GetExeDirectory();
const ch *GetBaseDirectory();

// Returns exe name in |exe_name| of size |exe_name_size|.
bool GetExeName(ch *exe_name, usize exe_name_size);

extern bool g_bVGui;

#endif  // !SOURCE_DEDICATED_DEDICATED_COMMON_H_
