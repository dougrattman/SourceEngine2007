// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/rangecheckedvar.h"

 
#include "tier0/include/memdbgon.h"

bool g_bDoRangeChecks = true;

static int g_nDisables = 0;

CDisableRangeChecks::CDisableRangeChecks() {
  if (!ThreadInMainThread()) return;
  g_nDisables++;
  g_bDoRangeChecks = false;
}

CDisableRangeChecks::~CDisableRangeChecks() {
  if (!ThreadInMainThread()) return;
  Assert(g_nDisables > 0);
  --g_nDisables;
  if (g_nDisables == 0) {
    g_bDoRangeChecks = true;
  }
}
