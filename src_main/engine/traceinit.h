// Copyright � 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Allows matching of initialization and shutdown function calls

#if !defined(TRACEINIT_H)
#define TRACEINIT_H

void TraceInit(const char *i, const char *s, int list);
void TraceShutdown(const char *s, int list);

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
