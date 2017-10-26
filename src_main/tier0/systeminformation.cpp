// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/systeminformation.h"

// Returns the size of a memory page in bytes.
// Assume page size is 4 Kb
uint32_t Plat_GetMemPageSize() { return 4; }
