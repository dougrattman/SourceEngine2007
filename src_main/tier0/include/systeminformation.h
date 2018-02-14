// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_SYSTEMINFORMATION_H_
#define SOURCE_TIER0_INCLUDE_SYSTEMINFORMATION_H_

#include "base/include/base_types.h"

#include "tier0/include/platform.h"

// Returns the size of a memory page in kilobytes.
PLATFORM_INTERFACE u32 Plat_GetMemPageSize();

#endif  // SOURCE_TIER0_INCLUDE_SYSTEMINFORMATION_H_
