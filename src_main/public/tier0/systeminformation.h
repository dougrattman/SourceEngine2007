// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_SYSTEMINFORMATION_H_
#define SOURCE_TIER0_SYSTEMINFORMATION_H_

#include <cstdint>
#include "tier0/platform.h"

// Returns the size of a memory page in kilobytes.
PLATFORM_INTERFACE uint32_t Plat_GetMemPageSize();

#endif  // SOURCE_TIER0_SYSTEMINFORMATION_H_
