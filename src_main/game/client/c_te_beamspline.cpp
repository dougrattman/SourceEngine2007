// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $

#include "cbase.h"
#include "c_basetempentity.h"
#include "tier0/include/vprof.h"

 
#include "tier0/include/memdbgon.h"

#define MAX_SPLINE_POINTS 16
//-----------------------------------------------------------------------------
// Purpose: BeamSpline TE
//-----------------------------------------------------------------------------
class C_TEBeamSpline : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEBeamSpline, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEBeamSpline( void );
	virtual			~C_TEBeamSpline( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	Vector			m_vecPoints[ MAX_SPLINE_POINTS ];
	int				m_nPoints;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamSpline::C_TEBeamSpline( void )
{
	int i;
	for ( i = 0; i < MAX_SPLINE_POINTS; i++ )
	{
		m_vecPoints[ i ].Init();
	}
	m_nPoints = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEBeamSpline::~C_TEBeamSpline( void )
{
}

void TE_BeamSpline( IRecipientFilter& filter, float delay,
	int points, Vector* rgPoints )
{
	DevMsg( 1, "Beam spline with %i points invoked\n", points );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEBeamSpline::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEBeamSpline::PostDataUpdate" );

	DevMsg( 1, "Beam spline with %i points received\n", m_nPoints );
}

// Expose the TE to the engine.
IMPLEMENT_CLIENTCLASS_EVENT( C_TEBeamSpline, DT_TEBeamSpline, CTEBeamSpline );

BEGIN_RECV_TABLE_NOBASE(C_TEBeamSpline, DT_TEBeamSpline)
	RecvPropInt( RECVINFO( m_nPoints )),
	RecvPropArray(
		RecvPropVector( RECVINFO(m_vecPoints[0])),
		m_vecPoints)
END_RECV_TABLE()

