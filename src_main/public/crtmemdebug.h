// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_CRTMEMDEBUG_H_
#define SOURCE_CRTMEMDEBUG_H_

#ifdef USECRTMEMDEBUG
#include <crtdbg.h>
#define MEMCHECK CheckHeap()
void CheckHeap();
#else
#define MEMCHECK
#endif

void InitCRTMemDebug();

#endif  // SOURCE_CRTMEMDEBUG_H_
