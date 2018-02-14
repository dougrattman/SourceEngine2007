// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#ifndef SOURCE_TIER0_INCLUDE_MEM_H_
#define SOURCE_TIER0_INCLUDE_MEM_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/platform.h"

#if !defined(STATIC_TIER0) && !defined(_STATIC_LINKED)
#ifdef TIER0_DLL_EXPORT
#define MEM_INTERFACE DLL_EXPORT
#else
#define MEM_INTERFACE DLL_IMPORT
#endif
#else  // BUILD_AS_DLL
#define MEM_INTERFACE extern
#endif  // BUILD_AS_DLL

// DLL-exported methods for particular kinds of memory
MEM_INTERFACE void *MemAllocScratch(usize nMemSize);
MEM_INTERFACE void MemFreeScratch();

#ifdef OS_POSIX
MEM_INTERFACE void ZeroMemory(void *mem, usize length);
#endif

#endif  // SOURCE_TIER0_INCLUDE_MEM_H_
