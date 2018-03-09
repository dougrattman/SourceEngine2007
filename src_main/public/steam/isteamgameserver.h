// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: interface to steam for game servers
//
//=============================================================================

#ifndef ISTEAMGAMESERVER_H
#define ISTEAMGAMESERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "isteamclient.h"

//-----------------------------------------------------------------------------
// Purpose: Functions for authenticating users via Steam to play on a game server
//-----------------------------------------------------------------------------
class ISteamGameServer
{
public:
	// connection functions
	virtual void LogOn() = 0;
	virtual void LogOff() = 0;
	// status functions
	virtual bool BLoggedOn() = 0;
	virtual bool BSecure() = 0; 
	virtual CSteamID GetSteamID() = 0;

	// user authentication functions
	virtual bool GSGetSteam2GetEncryptionKeyToSendToNewClient( void *pvEncryptionKey, uint32_t *pcbEncryptionKey, uint32_t cbMaxEncryptionKey ) = 0;
	// the IP address and port should be in host order, i.e 127.0.0.1 == 0x7f000001
	virtual bool GSSendUserConnect(  uint32_t unUserID, uint32_t unIPPublic, uint16_t usPort, const void *pvCookie, uint32_t cubCookie ) = 0; // Both Steam2 and Steam3 authentication
	// the IP address should be in host order, i.e 127.0.0.1 == 0x7f000001
	virtual bool GSRemoveUserConnect( uint32_t unUserID ) = 0;
	// do this call once you have a GSClientSteam2Accept_t message about a user
	virtual bool GSSendUserDisconnect( CSteamID steamID, uint32_t unUserID ) = 0;

	// Note that unGameIP is in host order
	virtual void GSSetSpawnCount( uint32_t ucSpawn ) = 0;
	virtual bool GSSetServerType( int32_t nGameAppId, uint32_t unServerFlags, uint32_t unGameIP, uint16_t unGamePort, uint16_t unSpectatorPort, uint16_t usQueryPort, const char *pchGameDir, const char *pchVersion, bool bLANMode ) = 0;
	// Same as GSUpdateStatus, but lets you specify a name for the spectator server (which shows up in the server browser).
	virtual bool GSUpdateStatus( int cPlayers, int cPlayersMax, int cBotPlayers, const char *pchServerName, const char *pSpectatorServerName, const char *pchMapName ) = 0;

	// This is used for bots and local players on listen servers. It creates a CSteamID that you can use to refer to the player,
	// then the info that you set for that player will be communicated to the master servers.
	// Use the regular GSSendUserDisconnect to notify when this player is going away.
	virtual bool GSCreateUnauthenticatedUser( CSteamID *pSteamID ) = 0;

	// (Only can do this on auth'd users - i.e. you've gotten a GSClientSteam2Accept_t message OR
	//  clients created with GSCreateUnauthenticatedUser).
	virtual bool GSSetUserData( CSteamID steamID, const char *pPlayerName, uint32_t nFrags ) = 0;
	
	// This can be called if spectator goes away or comes back (passing 0 means there is no spectator server now).
	virtual void GSUpdateSpectatorPort( uint16_t unSpectatorPort ) = 0;

	// (Optional) a string describing the game type for this server, can be searched for by client 
	// if they use the "gametype" filter option
	virtual void GSSetGameType( const char *pchGameType ) = 0; 
};

#define STEAMGAMESERVER_INTERFACE_VERSION "SteamGameServer003"

// game server flags
const uint32_t k_unServerFlagNone = 0x00;
const uint32_t k_unServerFlagActive		= 0x01;		// server has users playing
const uint32_t k_unServerFlagSecure		= 0x02;		// server wants to be secure
const uint32_t k_unServerFlagDedicated	= 0x04;		// server is dedicated
const uint32_t k_unServerFlagLinux		= 0x08;		// linux build
const uint32_t k_unServerFlagPassworded	= 0x10;		// password protected
const uint32_t k_unServerFlagPrivate		= 0x20;		// server shouldn't list on master server and
    	// won't enforce authentication of users that connect to the server.
    	// Useful when you run a server where the clients may not
    	// be connected to the internet but you want them to play (i.e LANs)



// callbacks


// client has been approved to connect to this game server
struct GSClientApprove_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 1 };
	CSteamID m_SteamID;
};


// client has been denied to connection to this game server
struct GSClientDeny_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 2 };
	CSteamID m_SteamID;
	EDenyReason m_eDenyReason;
	char m_pchOptionalText[128];
};


// request the game server should kick the user
struct GSClientKick_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 3 };
	CSteamID m_SteamID;
	EDenyReason m_eDenyReason;
};

// client has been denied to connect to this game server because of a Steam2 auth failure
struct GSClientSteam2Deny_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 4 };
	uint32_t m_UserID;
	uint32_t m_eSteamError;
};


// client has been accepted by Steam2 to connect to this game server
struct GSClientSteam2Accept_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 5 };
	uint32_t m_UserID;
	uint64_t m_SteamID;
};


// C-API versions of the interface functions
SOURCE_API_EXPORT void *Steam_GetGSHandle( HSteamUser hUser, HSteamPipe hSteamPipe );
SOURCE_API_EXPORT bool Steam_GSSendSteam2UserConnect( void *phSteamHandle, uint32_t unUserID, const void *pvRawKey, uint32_t unKeyLen, uint32_t unIPPublic, uint16_t usPort, const void *pvCookie, uint32_t cubCookie );
SOURCE_API_EXPORT bool Steam_GSSendSteam3UserConnect( void *phSteamHandle, uint64_t ulSteamID, uint32_t unIPPublic, const void *pvCookie, uint32_t cubCookie );
SOURCE_API_EXPORT bool Steam_GSSendUserDisconnect( void *phSteamHandle, uint64_t ulSteamID, uint32_t unUserID );
SOURCE_API_EXPORT bool Steam_GSSendUserStatusResponse( void *phSteamHandle, uint64_t ulSteamID, int nSecondsConnected, int nSecondsSinceLast );
SOURCE_API_EXPORT bool Steam_GSUpdateStatus( void *phSteamHandle, int cPlayers, int cPlayersMax, int cBotPlayers, const char *pchServerName, const char *pchMapName );
SOURCE_API_EXPORT bool Steam_GSRemoveUserConnect( void *phSteamHandle, uint32_t unUserID );
SOURCE_API_EXPORT void Steam_GSSetSpawnCount( void *phSteamHandle, uint32_t ucSpawn );
SOURCE_API_EXPORT bool Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient( void *phSteamHandle, void *pvEncryptionKey, uint32_t *pcbEncryptionKey, uint32_t cbMaxEncryptionKey );
SOURCE_API_EXPORT void Steam_GSLogOn( void *phSteamHandle );
SOURCE_API_EXPORT void Steam_GSLogOff( void *phSteamHandle );
SOURCE_API_EXPORT bool Steam_GSBLoggedOn( void *phSteamHandle );
SOURCE_API_EXPORT bool Steam_GSSetServerType( void *phSteamHandle, int32_t nAppIdServed, uint32_t unServerFlags, uint32_t unGameIP, uint32_t unGamePort, const char *pchGameDir, const char *pchVersion );
SOURCE_API_EXPORT bool Steam_GSBSecure( void *phSteamHandle );
SOURCE_API_EXPORT uint64_t Steam_GSGetSteamID( void *phSteamHandle );

#endif // ISTEAMGAMESERVER_H
