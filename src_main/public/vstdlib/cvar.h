// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VSTDLIB_CVAR_H_
#define SOURCE_VSTDLIB_CVAR_H_

#include "icvar.h"
#include "vstdlib/vstdlib.h"

// Returns a CVar dictionary for tool usage
VSTDLIB_INTERFACE CreateInterfaceFn VStdLib_GetICVarFactory();

#endif  // SOURCE_VSTDLIB_CVAR_H_
