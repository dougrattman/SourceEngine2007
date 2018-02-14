// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:	This turns on all Valve-specific #defines.  Because we sometimes
// call external include files from inside .cpp files, we need to
// wrap those includes like this:
// #include "tier0/include/include/valve_off.h"
// #include <external.h>
// #include "tier0/include/include/valve_on.h"

#ifdef STEAM
// Unicode-related #defines (see wchartypes.h)
#ifdef ENFORCE_WCHAR
#define ch DontUseChar_SeeWcharOn.h
#endif

// Memory-related #defines
#define malloc(cub) HEY_DONT_USE_MALLOC_USE_PVALLOC
#define realloc(pvOld, cub) HEY_DONT_USE_REALLOC_USE_PVREALLOC
#define _expand(pvOld, cub) HEY_DONT_USE_EXPAND_USE_PVEXPAND
#define free(pv) HEY_DONT_USE_FREE_USE_FREEPV

#endif
