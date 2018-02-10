// Copyright © 1996-2017, Valve Corporation, All rights reserved.

// First include standard libraries.
#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>

// Base.
#include "base/include/base_types.h"
#include "build/include/build_config.h"

// Windows.
#ifdef OS_WIN
#include "base/include/windows/windows_light.h"
#endif

#include "tier0/include/valve_off.h"

// tier0
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier0/include/dbgflag.h"
#ifdef STEAM
#include "tier0/include/memhook.h"
#endif
#include "tier0/include/validator.h"

#ifdef OS_POSIX
#define _MAX_PATH PATH_MAX
#endif

#include "tier0/include/valve_on.h"
