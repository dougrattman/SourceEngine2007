// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Thread management routines

#include "pch_tier0.h"

#include "tier0/threadtools.h"

#ifdef _WIN32
#include "winlite.h"
#endif

#include "tier0/dbg.h"
#include "tier0/platform.h"

#include "tier0/valve_off.h"

void Plat_SetThreadName(unsigned long dwThreadID, const char *pszName) {
  ThreadSetDebugName(dwThreadID, pszName);
}

unsigned long Plat_RegisterThread(const char *pszName) {
  ThreadSetDebugName(pszName);
  return ThreadGetCurrentId();
}

unsigned long Plat_GetCurrentThreadID() { return ThreadGetCurrentId(); }
