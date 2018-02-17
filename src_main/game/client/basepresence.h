// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Base implementation of the IPresence interface
//
//=============================================================================

#ifndef BASEPRESENCE_H
#define BASEPRESENCE_H
#ifdef _WIN32
#pragma once
#endif

#include "ipresence.h"
#include "igamesystem.h"

//-----------------------------------------------------------------------------
// Purpose: Common implementation for setting user contexts and properties.
// Each client should inherit from this to implement mod-specific presence info.
//-----------------------------------------------------------------------------
class CBasePresence : public IPresence, public CAutoGameSystemPerFrame
{
public:
	// CBaseGameSystemPerFrame overrides
	virtual bool		Init( void );
	virtual void		Shutdown( void );
	virtual void		Update( float frametime );
	virtual char const	*Name( void ) { return "presence"; }

	// IPresence Interface
	virtual void 		UserSetContext( unsigned int nUserIndex, unsigned int nContextId, unsigned int nContextValue, bool bAsync = false );
	virtual void 		UserSetProperty( unsigned int nUserIndex, unsigned int nPropertyId, unsigned int nBytes, const void *pvValue, bool bAsync = false );
	virtual void		SetupGameProperties( CUtlVector< XUSER_CONTEXT > &contexts, CUtlVector< XUSER_PROPERTY > &properties );
	virtual uint		GetPresenceID( const char *pIdName );
	virtual void		GetPropertyDisplayString( uint32_t id, uint32_t value, char *pOutput, int nBytes );
	virtual const char	*GetPropertyIdString( const uint32_t id );

	// Stats reporting
	virtual void		StartStatsReporting( HANDLE handle, bool bArbitrated );
	virtual void		SetStat( uint32_t iPropertyId, int iPropertyValue, int dataType );
	virtual void		UploadStats();

protected:
	bool  	m_bArbitrated;
	bool  	m_bReportingStats;
	HANDLE  	m_hSession;
	CUtlVector< XUSER_PROPERTY >	m_PlayerStats;	

	//---------------------------------------------------------
	// Debug support
	//---------------------------------------------------------
	CON_COMMAND_MEMBER_F( CBasePresence, "user_context", DebugUserSetContext, "Set a Rich Presence Context: user_context <context id> <context value>", 0 )
	CON_COMMAND_MEMBER_F( CBasePresence, "user_property", DebugUserSetProperty, "Set a Rich Presence Property: user_property <property id>", 0 )
};

#endif // BASEPRESENCE_H
