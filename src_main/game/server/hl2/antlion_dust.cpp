// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $
//

#include "cbase.h"
#include "antlion_dust.h"

 
#include "tier0/include/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST( CTEAntlionDust, DT_TEAntlionDust )
	SendPropVector( SENDINFO( m_vecOrigin ) ),
	SendPropVector( SENDINFO( m_vecAngles ) ),	
	SendPropBool( SENDINFO( m_bBlockedSpawner ) ),
END_SEND_TABLE()

CTEAntlionDust::CTEAntlionDust( const char *name ) : BaseClass( name )
{
}

CTEAntlionDust::~CTEAntlionDust( void )
{
}

static CTEAntlionDust g_TEAntlionDust( "AntlionDust" );

//-----------------------------------------------------------------------------
// Purpose: Creates antlion dust effect
// Input  : &origin - position
//			&angles - angles
//-----------------------------------------------------------------------------
void UTIL_CreateAntlionDust( const Vector &origin, const QAngle &angles, bool bBlockedSpawner )
{
	g_TEAntlionDust.m_vecOrigin = origin;
	g_TEAntlionDust.m_vecAngles = angles;
	g_TEAntlionDust.m_bBlockedSpawner = bBlockedSpawner;

	//Send it
	CPVSFilter filter( origin );
	g_TEAntlionDust.Create( filter, 0.0f );
}
