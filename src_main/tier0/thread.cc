// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Thread management routines

#include "tier0/include/threadtools.h"

#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

#include "tier0/include/valve_off.h"

void Plat_SetThreadName(unsigned long dwThreadID, const ch *pszName) {
  ThreadSetDebugName(dwThreadID, pszName);
}

unsigned long Plat_RegisterThread(const ch *pszName) {
  ThreadSetDebugName(pszName);
  return ThreadGetCurrentId();
}

unsigned long Plat_GetCurrentThreadID() { return ThreadGetCurrentId(); }
