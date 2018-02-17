// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: interface to utility functions in Steam
//
//=============================================================================

#ifndef ISTEAMUTILS_H
#define ISTEAMUTILS_H
#ifdef _WIN32
#pragma once
#endif

#include "isteamclient.h"

//-----------------------------------------------------------------------------
// Purpose: interface to user independent utility functions
//-----------------------------------------------------------------------------
class ISteamUtils
{
public:
	// return the number of seconds since the user 
	virtual uint32_t GetSecondsSinceAppActive() = 0;
	virtual uint32_t GetSecondsSinceComputerActive() = 0;

	// the universe this client is connecting to
	virtual EUniverse GetConnectedUniverse() = 0;

	// Steam server time - in PST, number of seconds since January 1, 1970 (i.e unix time)
	virtual uint32_t GetServerRealTime() = 0;

	// returns the 2 digit ISO 3166-1-alpha-2 format country code this client is running in (as looked up via an IP-to-location database)
	// e.g "US" or "UK".
	virtual const char *GetIPCountry() = 0;

	// returns true if the image exists, and valid sizes were filled out
	virtual bool GetImageSize( int iImage, uint32_t *pnWidth, uint32_t *pnHeight ) = 0;

	// returns true if the image exists, and the buffer was successfully filled out
	// results are returned in RGBA format
	// the destination buffer size should be 4 * height * width * sizeof(char)
	virtual bool GetImageRGBA( int iImage, uint8_t *pubDest, int nDestBufferSize ) = 0;

	// returns the IP of the reporting server for valve - currently only used in Source engine games
	virtual bool GetCSERIPPort( uint32_t *unIP, uint16_t *usPort ) = 0;
};

#define STEAMUTILS_INTERFACE_VERSION "SteamUtils002"


// callbacks


//-----------------------------------------------------------------------------
// Purpose: The country 
//-----------------------------------------------------------------------------
struct IPCountry_t
{
	enum { k_iCallback = k_iSteamUtilsCallbacks + 1 };
};

#endif // ISTEAMUTILS_H
