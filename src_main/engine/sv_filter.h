// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(SV_FILTER_H)
#define SV_FILTER_H

#include "filters.h"
#include "tier1/UtlVector.h"
#include "userid.h"

void Filter_Init(void);
void Filter_Shutdown(void);

bool Filter_ShouldDiscardID(unsigned int userid);
bool Filter_ShouldDiscard(const netadr_t& adr);
void Filter_SendBan(const netadr_t& adr);
const char* GetUserIDString(const USERID_t& id);

bool Filter_IsUserBanned(const USERID_t& userid);

extern CUtlVector<ipfilter_t> g_IPFilters;
extern CUtlVector<userfilter_t> g_UserFilters;

#endif  // SV_FILTER_H
