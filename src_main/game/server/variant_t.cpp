// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "cbase.h"
#include "variant_t.h"

 
#include "tier0/include/memdbgon.h"

void variant_t::SetEntity( CBaseEntity *val ) 
{ 
	eVal = val;
	fieldType = FIELD_EHANDLE; 
}


