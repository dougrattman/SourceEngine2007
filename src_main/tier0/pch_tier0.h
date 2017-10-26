// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "winlite.h"
#endif

// First include standard libraries.
#include <ctype.h>
#include <malloc.h>
#include <memory.h>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <map>
#include "tier0/valve_off.h"

// tier0
#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier0/dbgflag.h"
#ifdef STEAM
#include "tier0/memhook.h"
#endif
#include "tier0/validator.h"

#ifdef _LINUX
#define _MAX_PATH PATH_MAX
#endif

#include "tier0/valve_on.h"
