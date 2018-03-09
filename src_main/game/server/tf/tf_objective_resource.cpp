// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $

#include "cbase.h"
#include "tf_objective_resource.h"
#include "shareddefs.h"
#include <coordsize.h>

 
#include "tier0/include/memdbgon.h"

// Datatable
IMPLEMENT_SERVERCLASS_ST( CTFObjectiveResource, DT_TFObjectiveResource )
END_SEND_TABLE()


BEGIN_DATADESC( CTFObjectiveResource )
END_DATADESC()


LINK_ENTITY_TO_CLASS( tf_objective_resource, CTFObjectiveResource );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFObjectiveResource::Spawn( void )
{
	BaseClass::Spawn();
}
