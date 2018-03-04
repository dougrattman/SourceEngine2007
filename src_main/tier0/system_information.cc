// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/systeminformation.h"

// Returns the size of a memory page in bytes.
// Assume page size is 4 Kb
u32 Plat_GetMemPageSize() { return 4; }
