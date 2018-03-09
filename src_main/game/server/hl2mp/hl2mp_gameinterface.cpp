// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "hl2mp_gameinterface.h"

 
#include "tier0/include/memdbgon.h"

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = defaultMaxPlayers = 2; 
	maxplayers = 16;
}

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
}

