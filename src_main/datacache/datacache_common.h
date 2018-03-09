// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DATACACHE_COMMON_H
#define DATACACHE_COMMON_H

#include "base/include/macros.h"
#include "tier3/tier3.h"

class IDataCacheSection;
SOURCE_FORWARD_DECLARE_HANDLE(memhandle_t);
typedef memhandle_t DataCacheHandle_t;

// Console commands
extern ConVar developer;

#endif  // DATACACHE_COMMON_H
