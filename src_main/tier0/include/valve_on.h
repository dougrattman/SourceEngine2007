// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// This turns on all Valve-specific #defines. Because we sometimes call external
// include files from inside .cc files, we need to wrap those includes like
// this:
//
// #include "tier0/include/valve_off.h"
// #include <external.h>
// #include "tier0/include/valve_on.h"

#ifdef STEAM

// Memory-related #defines.
#define malloc(cub) HEY_DONT_USE_MALLOC_USE_PVALLOC
#define realloc(pvOld, cub) HEY_DONT_USE_REALLOC_USE_PVREALLOC
#define _expand(pvOld, cub) HEY_DONT_USE_EXPAND_USE_PVEXPAND
#define free(pv) HEY_DONT_USE_FREE_USE_FREEPV

#endif
