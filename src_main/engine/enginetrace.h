// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ENGINETRACE_H
#define ENGINETRACE_H

#include "engine/IEngineTrace.h"

extern IEngineTrace *g_pEngineTraceServer;
extern IEngineTrace *g_pEngineTraceClient;

// Debugging code to render all ray casts since the last time this call was made
void EngineTraceRenderRayCasts();

#endif  // ENGINETRACE_H
