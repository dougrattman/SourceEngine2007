// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_MEM_HELPERS_H_
#define SOURCE_TIER0_MEM_HELPERS_H_

#include "base/include/base_types.h"

// Normally, the runtime libraries like to mess with the memory returned by
// malloc(), which can create problems trying to repro bugs in debug builds or
// in the debugger.
//
// If the debugger is present, it initializes data to 0xbaadf00d, which makes
// floating point numbers come out to about 0.1.
//
// If the debugger is not present, and it's a debug build, then you get
// 0xcdcdcdcd, which is about 25 million.
//
// Otherwise, you get uninitialized memory.
//
// In here, we make sure the memory is either random garbage, or it's set to
// 0xffeeffee, which casts to a NAN.
void ApplyMemoryInitializations(void *memory, usize size);
usize CalcHeapUsed();

#endif  // SOURCE_TIER0_MEM_HELPERS_H_
