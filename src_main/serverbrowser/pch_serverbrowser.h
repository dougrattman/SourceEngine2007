// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "winlite.h"
#undef CreateDialog

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "tier0/include/memdbgoff.h"
#include "tier0/include/memdbgon.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/pch_vgui_controls.h"
#include "vstdlib/pch_vstdlib.h"

#include "tier3/tier3.h"

// steam3 API
#include "steam/isteammasterserverupdater.h"
//#include "steam/steam_querypackets.h"
#include "steam/isteamfriends.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamuser.h"
#include "steam/steam_api.h"

#include "IVGuiModule.h"
#include "ServerBrowser/IServerBrowser.h"
#include "vgui_controls/Controls.h"

#include "FileSystem.h"
#include "IRunGameEngine.h"
#include "iappinformation.h"
#include "modlist.h"
#include "proto_oob.h"
#include "tier1/netadr.h"

#include "offlinemode.h"

// serverbrowser files

#include "DialogAddServer.h"
#include "DialogGameInfo.h"
#include "DialogServerPassword.h"
#include "IGameList.h"
#include "ServerBrowser.h"
#include "ServerListCompare.h"
#include "servercontextmenu.h"
#include "vacbannedconnrefuseddialog.h"

// game list
#include "BaseGamesPage.h"
#include "CustomGames.h"
#include "FavoriteGames.h"
#include "FriendsGames.h"
#include "HistoryGames.h"
#include "InternetGames.h"
#include "LanGames.h"
#include "ServerBrowserDialog.h"
#include "SpectateGames.h"

#if defined(STEAM)
#define IsSteam() true
#else
#define IsSteam() false
#endif

using namespace vgui;