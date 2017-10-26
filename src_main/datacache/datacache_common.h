// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef DATACACHE_COMMON_H
#define DATACACHE_COMMON_H

#include "tier3/tier3.h"

class IDataCacheSection;
FORWARD_DECLARE_HANDLE(memhandle_t);
typedef memhandle_t DataCacheHandle_t;

// Console commands
extern ConVar developer;

#endif  // DATACACHE_COMMON_H
