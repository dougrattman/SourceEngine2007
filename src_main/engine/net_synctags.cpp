// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/convar.h"

 
#include "tier0/include/memdbgon.h"

#ifdef _DEBUG

ConVar net_synctags(
    "net_synctags", "0", 0,
    "Insert tokens into the net stream to find client/server mismatches.");

#endif
