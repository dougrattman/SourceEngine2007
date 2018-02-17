// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//=============================================================================

#ifndef STEAM_GAMESERVER_H
#define STEAM_GAMESERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "steam_api.h"
#include "isteamgameserver.h"
#include "isteammasterserverupdater.h"

enum EServerMode
{
	eServerModeNoAuthentication = 1, // Don't authenticate user logins and don't list on the server list
	eServerModeAuthentication = 2, // Authenticate users, list on the server list, don't run VAC on clients that connect
	eServerModeAuthenticationAndSecure = 3, // Authenticate users, list on the server list and VAC protect clients
};    	

// Note: if you pass MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE for usQueryPort, then it will use "GameSocketShare" mode, 
// which means that the game is responsible for sending and receiving UDP packets for the master 
// server updater. See references to GameSocketShare in isteammasterserverupdater.h.
//
// Pass 0 for usGamePort or usSpectatorPort if you're not using that. Then, the master server updater will report 
// what's running based on that.
S_API bool SteamGameServer_Init( uint32_t unIP, uint16_t usPort, uint16_t usGamePort, uint16_t usSpectatorPort, uint16_t usQueryPort, EServerMode eServerMode, int nGameAppId, const char *pchGameDir, const char *pchVersionString );
S_API void SteamGameServer_Shutdown();
S_API void SteamGameServer_RunCallbacks();

S_API bool SteamGameServer_BSecure();
S_API uint64_t SteamGameServer_GetSteamID();

S_API ISteamGameServer *SteamGameServer();
S_API ISteamUtils *SteamGameServerUtils();
S_API ISteamMasterServerUpdater *SteamMasterServerUpdater();

#define STEAM_GAMESERVER_CALLBACK( thisclass, func, param, var ) CCallback< thisclass, param, true > var; void func( param *pParam )


#endif // STEAM_GAMESERVER_H
