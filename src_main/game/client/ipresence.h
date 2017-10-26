// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Interface for setting Rich Presence contexts and properties.
//
//=============================================================================

#ifndef IPRESENCE_H
#define IPRESENCE_H
#ifdef _WIN32
#pragma once
#endif

//--------------------------------------------------------------------
// Purpose: Rich Presence interface
//--------------------------------------------------------------------
class IPresence
{
public:
	virtual void 		UserSetContext( unsigned int nUserIndex, unsigned int nContextId, unsigned int nContextValue, bool bAsync = false ) = 0;
	virtual void 		UserSetProperty( unsigned int nUserIndex, unsigned int nPropertyId, unsigned int nBytes, const void *pvValue, bool bAsync = false ) = 0;
	virtual void		SetupGameProperties( CUtlVector< XUSER_CONTEXT > &contexts, CUtlVector< XUSER_PROPERTY > &properties ) = 0;
	virtual uint		GetPresenceID( const char *pIDName ) = 0;
	virtual const char	*GetPropertyIdString( const uint32_t id ) = 0;
	virtual void		GetPropertyDisplayString( uint32_t id, uint32_t value, char *pOutput, int nBytes ) = 0;

	// Stats reporting
	virtual void		StartStatsReporting( HANDLE handle, bool bArbitrated ) = 0;
	virtual void		SetStat( uint32_t iPropertyId, int iPropertyValue, int dataType ) = 0;
	virtual void		UploadStats() = 0;
};

extern IPresence *presence;

#endif // IPRESENCE_H
