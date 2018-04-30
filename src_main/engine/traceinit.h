// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Allows matching of initialization and shutdown function calls

#ifndef TRACEINIT_H
#define TRACEINIT_H

#include "base/include/base_types.h"

bool TraceInit(const ch *i, const ch *s, usize list);
bool TraceShutdown(const ch *s, usize list);

#define TRACEINITNUM(initfunc, shutdownfunc, num) \
  TraceInit(#initfunc, #shutdownfunc, num);       \
  initfunc;

#define TRACESHUTDOWNNUM(shutdownfunc, num) \
  TraceShutdown(#shutdownfunc, num);        \
  shutdownfunc;

#define TRACEINIT(initfunc, shutdownfunc) \
  TRACEINITNUM(initfunc, shutdownfunc, 0)
#define TRACESHUTDOWN(shutdownfunc) TRACESHUTDOWNNUM(shutdownfunc, 0)

#endif  // TRACEINIT_H
