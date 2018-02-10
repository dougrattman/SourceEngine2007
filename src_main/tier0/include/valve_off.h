// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:	This turns off all Valve-specific #defines.  Because we
// sometimes call external include files from inside .cpp files, we need to wrap
// those includes like this:
// #include "tier0/include/include/valve_off.h"
// #include <external.h>
// #include "tier0/include/include/valve_on.h"

#include "public/tier0/valve_off.h"
