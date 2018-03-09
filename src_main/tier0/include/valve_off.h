// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// This turns off all Valve-specific #defines. Because we sometimes call
// external include files from inside .cc files, we need to wrap those includes
// like this:
//
// #include "tier0/include/valve_off.h"
// #include <external.h>
// #include "tier0/include/valve_on.h"

#ifdef STEAM

// Memory-related #defines
#undef malloc
#undef realloc
#undef _expand
#undef free

#endif
