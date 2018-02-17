// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:	This turns off all Valve-specific #defines.  Because we
// sometimes call external include files from inside .cpp files, we need to wrap
// those includes like this:
// #include "tier0/include/include/valve_off.h"
// #include <external.h>
// #include "tier0/include/include/valve_on.h"

#ifdef STEAM

// Unicode-related #defines (see wchartypes.h)
#undef ch

// Memory-related #defines
#undef malloc
#undef realloc
#undef _expand
#undef free

#endif
