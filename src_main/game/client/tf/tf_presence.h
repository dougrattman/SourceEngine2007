// Copyright � 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: TF implementation of the IPresence interface
//
//=============================================================================

#ifndef TF_PRESENCE_H
#define TF_PRESENCE_H
#ifdef _WIN32
#pragma once
#endif

#include "gameeventlistener.h"
#include "basepresence.h"
#include "hl2orange.spa.h"

//-----------------------------------------------------------------------------
// Purpose: TF implementation for setting user contexts and properties.
//-----------------------------------------------------------------------------
class CTF_Presence : public CBasePresence, public CGameEventListener
{
public:
	// IGameEventListener Interface
	virtual void	FireGameEvent( IGameEvent * event );

	// CBaseGameSystemPerFrame overrides
	virtual bool		Init( void );
	virtual void		LevelInitPreEntity( void );

	// IPresence interface
	virtual void		SetupGameProperties( CUtlVector< XUSER_CONTEXT > &contexts, CUtlVector< XUSER_PROPERTY > &properties );
	virtual uint		GetPresenceID( const char *pIDName );
	virtual const char 	*GetPropertyIdString( const unsigned int id );
	virtual void		GetPropertyDisplayString( uint32_t id, uint32_t value, char *pOutput, int nBytes );
	virtual void		UploadStats();

private:
	bool	m_bIsInCommentary;

#if defined( _X360 )
	XSESSION_VIEW_PROPERTIES		m_ViewProperties[3];
#endif

};

#endif // TF_PRESENCE_H
