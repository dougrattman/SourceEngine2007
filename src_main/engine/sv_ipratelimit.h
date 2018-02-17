// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: rate limits OOB server queries

#ifndef SV_IPRATELIMIT_H
#define SV_IPRATELIMIT_H

#include "tier1/netadr.h"

// returns false if this IP exceeds rate limits
bool CheckConnectionLessRateLimits(netadr_t& adr);

#endif  // SV_IPRATELIMIT_H
