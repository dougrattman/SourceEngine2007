// Copyright © 2005, Valve Corporation, All rights reserved.

#ifndef ENGINETHREADS_H
#define ENGINETHREADS_H

#include "const.h"
#include "tier0/include/threadtools.h"

#ifdef SOURCE_MT

extern bool g_bThreadedEngine;
extern int g_nMaterialSystemThread;
extern int g_nServerThread;

#define IsEngineThreaded() (g_bThreadedEngine)

#else

#define IsEngineThreaded() (false)

#endif

#endif  // ENGINETHREADS_H
