// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#ifndef SOURCE_TIER0_INCLUDE_MEM_H_
#define SOURCE_TIER0_INCLUDE_MEM_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"
#include "tier0/include/tier0_api.h"

// DLL-exported methods for particular kinds of memory
SOURCE_TIER0_API void *MemAllocScratch(usize size);
SOURCE_TIER0_API void MemFreeScratch();

#ifdef OS_POSIX
SOURCE_TIER0_API void ZeroMemory(void *mem, usize length);
#endif

#endif  // SOURCE_TIER0_INCLUDE_MEM_H_
