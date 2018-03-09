// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//

#include "cbase.h"
#include "func_ladder.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: A transient entity used to construct a true CFuncLadder
// TODO(d.rattman):  THIS ENTITY IS OBSOLETE NOW, SHOULD BE REMOVED FROM HERE AND .FGD AT SOME POINT!!!
//-----------------------------------------------------------------------------
class CFuncLadderEndPoint : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncLadderEndPoint, CBaseEntity );

	virtual void Activate();

private:
	bool		Validate();
};

LINK_ENTITY_TO_CLASS( func_ladderendpoint, CFuncLadderEndPoint );

void CFuncLadderEndPoint::Activate()
{
	BaseClass::Activate();

	if ( IsMarkedForDeletion() )
		return;

	Validate();
}

bool CFuncLadderEndPoint::Validate()
{
	// Find the the other end
	Vector startPos = GetAbsOrigin();
	
	CFuncLadderEndPoint *other = dynamic_cast< CFuncLadderEndPoint * >( GetNextTarget() );
	if ( !other )
	{
		DevMsg( 1, "func_ladderendpoint(%s) without matching target\n", GetEntityName().ToCStr() );
		return false;
	}

	Vector endPos = other->GetAbsOrigin();

	CFuncLadder *ladder = ( CFuncLadder * )CreateEntityByName( "func_useableladder" );
	if ( ladder )
	{
		ladder->SetEndPoints( startPos, endPos );
		ladder->SetAbsOrigin( GetAbsOrigin() );
		ladder->SetParent( GetParent() );
		ladder->SetName( GetEntityName() );
		ladder->Spawn();
	}

	// Delete both endpoints
	UTIL_Remove( other );
	UTIL_Remove( this );

	return true;
}
