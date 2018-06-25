// Copyright © 2005, Valve Corporation, All rights reserved.

#ifndef ENGINETHREADS_H
#define ENGINETHREADS_H

#include "const.h"
#include "tier0/include/threadtools.h"

extern bool g_bThreadedEngine;
extern int g_nMaterialSystemThread;
extern int g_nServerThread;

inline bool IsEngineThreaded() { return g_bThreadedEngine; }

#endif  // ENGINETHREADS_H
