// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "host_cmd.h"

#include <cinttypes>
#include <limits>
#include "PlayerState.h"
#include "base/include/windows/product_version.h"
#include "build/include/build_config.h"
#include "cdll_engine_int.h"
#include "cdll_int.h"
#include "checksum_engine.h"
#include "cl_main.h"
#include "cmd.h"
#include "const.h"
#include "datacache/idatacache.h"
#include "demo.h"
#include "enginesingleuserfilter.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "gl_matsysiface.h"
#include "hltvserver.h"
#include "host.h"
#include "host_saverestore.h"
#include "host_state.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "keys.h"
#include "networkstringtableclient.h"
#include "networkstringtableserver.h"
#include "pr_edict.h"
#include "profile.h"
#include "proto_version.h"
#include "protocol.h"
#include "r_local.h"
#include "screen.h"
#include "server.h"
#include "string_t.h"
#include "sv_filter.h"
#include "sv_main.h"
#include "sv_steamauth.h"
#include "sys_dll.h"
#include "testscriptmgr.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vprof.h"
#include "vengineserver_impl.h"
#include "world.h"
#include "zone.h"

#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif

#ifdef OS_WIN
#include "sound.h"
#include "voice.h"
#endif

#include "filesystem/IQueuedLoader.h"
#include "ixboxsystem.h"
#include "sv_rcon.h"

extern IXboxSystem *g_pXboxSystem;

#include "tier0/include/memdbgon.h"

#ifndef SWDS
bool g_bInEditMode = false;
bool g_bInCommentaryMode = false;
#endif

ConVar host_name("hostname", "", 0, "Hostname for server.",
                 [](IConVar *var, const ch *pOldValue, f32 flOldValue) {
                   Steam3Server().NotifyOfServerNameChange();
                 });
ConVar host_map("host_map", "", 0, "Current map name.");

ConVar voice_recordtofile("voice_recordtofile", "0", 0,
                          "Record mic data and decompressed voice data into "
                          "'voice_micdata.wav' and 'voice_decompressed.wav'");
ConVar voice_inputfromfile(
    "voice_inputfromfile", "0", 0,
    "Get voice input from 'voice_input.wav' rather than from the microphone.");

ch gpszVersionString[16], gpszProductString[128];
// defaults to Source SDK Base (215) if no steam.inf can be found.
int g_iSteamAppID{215};

int gHostSpawnCount{0};

// If any quit handlers balk, then aborts quit sequence.
bool EngineTool_CheckQuitHandlers();

CON_COMMAND(_restart, "Shutdown and restart the engine.") {
  HostState_Restart();
}

#ifndef SWDS
static void Host_LightCrosshair() {
  if (!host_state.worldmodel) {
    ConMsg("No world model. Please, start the game.\n");
    return;
  }

  Vector end_point, lightmap_color;

  // max_range * sqrt(3)
  VectorMA(MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(),
           end_point);
  R_LightVec(MainViewOrigin(), end_point, true, lightmap_color);

  const u8 r{LinearToTexture(lightmap_color.x)};
  const u8 g{LinearToTexture(lightmap_color.y)};
  const u8 b{LinearToTexture(lightmap_color.z)};

  ConMsg("Luxel Value: %" PRIu8 " %" PRIu8 " %" PRIu8 ".\n", r, g, b);
}

static ConCommand light_crosshair("light_crosshair", Host_LightCrosshair,
                                  "Show texture color at crosshair",
                                  FCVAR_CHEAT);
#endif

// Print client info to console.
static void Host_Status_PrintClient(IClient *client, bool should_show_address,
                                    void (*print)(const ch *fmt, ...)) {
  INetChannelInfo *client_net_channel_info{client->GetNetChannel()};

  const ch *client_state = "challenging";
  if (client->IsActive())
    client_state = "active";
  else if (client->IsSpawned())
    client_state = "spawning";
  else if (client->IsConnected())
    client_state = "connecting";

  if (client_net_channel_info != nullptr) {
    print("# %2i \"%s\" %s %s %.2f %.2f %s", client->GetUserID(),
          client->GetClientName(), client->GetNetworkIDString(),
          COM_FormatSeconds(client_net_channel_info->GetTimeConnected()),
          1000.0f * client_net_channel_info->GetAvgLatency(FLOW_OUTGOING),
          100.0f * client_net_channel_info->GetAvgLoss(FLOW_INCOMING),
          client_state);

    if (should_show_address) {
      print(" %s", client_net_channel_info->GetAddress());
    }
  } else {
    print("#%2i \"%s\" %s %s", client->GetUserID(), client->GetClientName(),
          client->GetNetworkIDString(), client_state);
  }

  print(".\n");
}

static void Host_Client_Printf(const ch *fmt, ...) {
  va_list arg_ptr;
  ch message[1024];

  va_start(arg_ptr, fmt);
  Q_vsnprintf(message, SOURCE_ARRAYSIZE(message), fmt, arg_ptr);
  va_end(arg_ptr);

  host_client->ClientPrintf("%s", message);
}

CON_COMMAND(status, "Display map and connection status.") {
  void (*print)(const char *fmt, ...);

  if (cmd_source == src_command) {
    if (!sv.IsActive()) {
      Cmd_ForwardToServer(args);
      return;
    }
    print = ConMsg;
  } else {
    print = Host_Client_Printf;
  }

  // Server status information.
  print("hostname: %s\n", host_name.GetString());

  const char *secure_reason{""};
  const bool is_secure = Steam3Server().BSecure();
  if (!is_secure && Steam3Server().BWantsSecure()) {
    secure_reason = Steam3Server().BLoggedOn()
                        ? "(secure mode enabled, connected to Steam3)"
                        : "(secure mode enabled, disconnected from Steam3)";
  }

  print("version : %s/%d %d %s %s.\n", gpszVersionString, PROTOCOL_VERSION,
        build_number(), is_secure ? "secure" : "insecure", secure_reason);

  if (NET_IsMultiplayer()) {
    print("udp/ip  :  %s:%i.\n", net_local_adr.ToString(true), sv.GetUDPPort());
  }

  print("map     : %s at: %.2f x, %.2f y, %.2f z.\n", sv.GetMapName(),
        MainViewOrigin()[0], MainViewOrigin()[1], MainViewOrigin()[2]);

  if (hltv && hltv->IsActive()) {
    print("sourcetv:  port %i, delay %.1fs.\n", hltv->GetUDPPort(),
          hltv->GetDirector()->GetDelay());
  }

  int players = sv.GetNumClients();

  print("players : %i (%i max).\n\n", players, sv.GetMaxClients());

  // Early exit for this server.
  if (args.ArgC() == 2) {
    if (!Q_stricmp(args[1], "short")) {
      for (int j = 0; j < sv.GetClientCount(); j++) {
        const IClient *client{sv.GetClient(j)};

        if (!client->IsActive()) continue;

        print("#%i - %s.\n", j + 1, client->GetClientName());
      }
      return;
    }
  }

  // The header for the status rows.
  print("# userid name uniqueid connected ping loss state");
  if (cmd_source == src_command) print(" adr");
  print(".\n");

  for (int j = 0; j < sv.GetClientCount(); j++) {
    IClient *client{sv.GetClient(j)};

    // Not connected yet, maybe challenging.
    if (!client->IsConnected()) continue;

    Host_Status_PrintClient(client, cmd_source == src_command, print);
  }
}

CON_COMMAND(ping, "Display ping to server.") {
  if (cmd_source == src_command) {
    Cmd_ForwardToServer(args);
    return;
  }

  host_client->ClientPrintf("Client ping times:\n");

  for (int i = 0; i < sv.GetClientCount(); i++) {
    IClient *client{sv.GetClient(i)};

    if (!client->IsConnected() || client->IsFakeClient()) continue;

    host_client->ClientPrintf(
        "%4.0f ms : %s.\n",
        1000.0f * client->GetNetChannel()->GetAvgLatency(FLOW_OUTGOING),
        client->GetClientName());
  }
}

extern void GetPlatformMapPath(const char *pMapPath, char *pPlatformMapPath,
                               int maxLength);

bool CL_HL2Demo_MapCheck(const char *map_name) {
  if (CL_IsHL2Demo() && !sv.IsDedicated()) {
    if (!Q_stricmp(map_name, "d1_trainstation_01") ||
        !Q_stricmp(map_name, "d1_trainstation_02") ||
        !Q_stricmp(map_name, "d1_town_01") ||
        !Q_stricmp(map_name, "d1_town_01a") ||
        !Q_stricmp(map_name, "d1_town_02") ||
        !Q_stricmp(map_name, "d1_town_03") ||
        !Q_stricmp(map_name, "background01") ||
        !Q_stricmp(map_name, "background03")) {
      return true;
    }
    return false;
  }

  return true;
}

bool CL_PortalDemo_MapCheck(const char *map_name) {
  if (CL_IsPortalDemo() && !sv.IsDedicated()) {
    if (!Q_stricmp(map_name, "testchmb_a_00") ||
        !Q_stricmp(map_name, "testchmb_a_01") ||
        !Q_stricmp(map_name, "testchmb_a_02") ||
        !Q_stricmp(map_name, "testchmb_a_03") ||
        !Q_stricmp(map_name, "testchmb_a_04") ||
        !Q_stricmp(map_name, "testchmb_a_05") ||
        !Q_stricmp(map_name, "testchmb_a_06") ||
        !Q_stricmp(map_name, "background1")) {
      return true;
    }
    return false;
  }

  return true;
}

static void Host_Map_Helper(const CCommand &args, bool is_edit_mode,
                            bool is_background, bool has_commentary) {
  if (cmd_source != src_command) return;

  if (args.ArgC() < 2) {
    Warning("No map specified\n");
    return;
  }

  char map_path[SOURCE_MAX_PATH];
  GetPlatformMapPath(args[1], map_path, SOURCE_ARRAYSIZE(map_path));

  Plat_TimestampedLog("Engine::Host_Map_Helper: Map %s load.", map_path);

  // If I was in edit mode reload config file
  // to overwrite WC edit key bindings
#if !defined(SWDS)
  if (!is_edit_mode) {
    if (g_bInEditMode) {
      // Re-read config from disk
      Host_ReadConfiguration();
      g_bInEditMode = false;
    }
  } else {
    g_bInEditMode = true;
  }

  g_bInCommentaryMode = has_commentary;
#endif

  // If there is a .bsp on the end, strip it off!
  usize i = Q_strlen(map_path);
  if (i > 4 && !Q_strcasecmp(map_path + i - 4, ".bsp")) {
    map_path[i - 4] = 0;
  }

  if (!g_pVEngineServer->IsMapValid(map_path)) {
    Warning("map load failed: %s not found or invalid\n", map_path);
    return;
  }

  if (!CL_HL2Demo_MapCheck(map_path)) {
    Warning("map load failed: %s not found or invalid\n", map_path);
    return;
  }

  if (!CL_PortalDemo_MapCheck(map_path)) {
    Warning("map load failed: %s not found or invalid\n", map_path);
    return;
  }

  // Stop demo loop
  cl.demonum = -1;

  // Stop old game
  Host_Disconnect(false);
  HostState_NewGame(map_path, false, is_background);

  if (args.ArgC() == 10) {
    if (Q_stricmp(args[2], "setpos") == 0 &&
        Q_stricmp(args[6], "setang") == 0) {
      Vector newpos{strtof(args[3], nullptr), strtof(args[4], nullptr),
                    strtof(args[5], nullptr)};
      QAngle newangle{strtof(args[7], nullptr), strtof(args[8], nullptr),
                      strtof(args[9], nullptr)};

      HostState_SetSpawnPoint(newpos, newangle);
    }
  }
}

// map <servername>
// command from the console.  Active clients are kicked off.
void Host_Map_f(const CCommand &args) {
  Host_Map_Helper(args, false, false, false);
}

// handle a map_edit <servername> command from the console.
// Active clients are kicked off.
// UNDONE: protect this from use if not in dev. mode
#ifndef SWDS
CON_COMMAND(map_edit, "") { Host_Map_Helper(args, true, false, false); }
#endif

// Purpose: Runs a map as the background
void Host_Map_Background_f(const CCommand &args) {
  Host_Map_Helper(args, false, true, false);
}

// Purpose: Runs a map in commentary mode
void Host_Map_Commentary_f(const CCommand &args) {
  Host_Map_Helper(args, false, false, true);
}

// Restarts the current server for a dead player.
CON_COMMAND(restart,
            "Restart the game on the same level (add setpos to jump to current "
            "view position on restart).") {
  if (
#if !defined(SWDS)
      demoplayer->IsPlayingBack() ||
#endif
      !sv.IsActive())
    return;

  if (sv.IsMultiplayer()) return;

  if (cmd_source != src_command) return;

  const bool should_remember_location =
      args.ArgC() == 2 && !Q_stricmp(args[1], "setpos");

  Host_Disconnect(false);  // stop old game

  if (!CL_HL2Demo_MapCheck(sv.GetMapName())) {
    Warning("map load failed: %s not found or invalid\n", sv.GetMapName());
    return;
  }

  if (!CL_PortalDemo_MapCheck(sv.GetMapName())) {
    Warning("map load failed: %s not found or invalid\n", sv.GetMapName());
    return;
  }

  HostState_NewGame(sv.GetMapName(), should_remember_location, false);
}

// Restarts the current server for a dead player
CON_COMMAND(reload,
            "Reload the most recent saved game (add setpos to jump to current "
            "view position on reload).") {
#ifndef SWDS
  const char *pSaveName;
  char name[MAX_OSPATH];
#endif

  if (
#if !defined(SWDS)
      demoplayer->IsPlayingBack() ||
#endif
      !sv.IsActive())
    return;

  if (sv.IsMultiplayer()) return;

  if (cmd_source != src_command) return;

  bool remember_location = false;
  if (args.ArgC() == 2 && !Q_stricmp(args[1], "setpos")) {
    remember_location = true;
  }

  // See if there is a most recently saved game
  // Restart that game if there is
  // Otherwise, restart the starting game map
#ifndef SWDS
  pSaveName = saverestore->FindRecentSave(name, sizeof(name));

  // Put up loading plaque
  SCR_BeginLoadingPlaque();

  Host_Disconnect(false);  // stop old game

  if (pSaveName && saverestore->SaveFileExists(pSaveName)) {
    HostState_LoadGame(pSaveName, remember_location);
  } else
#endif
  {
    if (!CL_HL2Demo_MapCheck(host_map.GetString())) {
      Warning("map load failed: %s not found or invalid\n",
              host_map.GetString());
      return;
    }

    if (!CL_PortalDemo_MapCheck(host_map.GetString())) {
      Warning("map load failed: %s not found or invalid\n",
              host_map.GetString());
      return;
    }

    HostState_NewGame(host_map.GetString(), remember_location, false);
  }
}

// Purpose: Goes to a new map, taking all clients along
void Host_Changelevel_f(const CCommand &args) {
  if (args.ArgC() < 2) {
    ConMsg("changelevel <levelname> : continue game on a new level\n");
    return;
  }

  if (!sv.IsActive()) {
    ConMsg("Can't changelevel, not running server\n");
    return;
  }

  if (!g_pVEngineServer->IsMapValid(args[1])) {
    Warning("changelevel failed: %s not found\n", args[1]);
    return;
  }

  if (!CL_HL2Demo_MapCheck(args[1])) {
    Warning("changelevel failed: %s not found\n", args[1]);
    return;
  }

  if (!CL_PortalDemo_MapCheck(args[1])) {
    Warning("changelevel failed: %s not found\n", args[1]);
    return;
  }

  HostState_ChangeLevelMP(args[1], args[2]);
}

// Purpose: Changing levels within a unit, uses save/restore
void Host_Changelevel2_f(const CCommand &args) {
  if (args.ArgC() < 2) {
    ConMsg(
        "changelevel2 <levelname> : continue game on a new level in the "
        "unit\n");
    return;
  }

  if (!sv.IsActive()) {
    ConMsg("Can't changelevel2, not in a map\n");
    return;
  }

  if (!g_pVEngineServer->IsMapValid(args[1])) {
    if (!CL_IsHL2Demo() ||
        (CL_IsHL2Demo() && !(!Q_stricmp(args[1], "d1_trainstation_03") ||
                             !Q_stricmp(args[1], "d1_town_02a")))) {
      Warning("changelevel2 failed: %s not found\n", args[1]);
      return;
    }
  }

#if !defined(SWDS)
  // needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't
  // a valid map
  if (CL_IsHL2Demo() && !sv.IsDedicated() &&
      !Q_stricmp(args[1], "d1_trainstation_03")) {
    void CL_DemoTransitionFromTrainstation();
    CL_DemoTransitionFromTrainstation();
    return;
  }

  // needs to be before CL_HL2Demo_MapCheck() check as d1_trainstation_03 isn't
  // a valid map
  if (CL_IsHL2Demo() && !sv.IsDedicated() &&
      !Q_stricmp(args[1], "d1_town_02a") &&
      !Q_stricmp(args[2], "d1_town_02_02a")) {
    void CL_DemoTransitionFromRavenholm();
    CL_DemoTransitionFromRavenholm();
    return;
  }

  if (CL_IsPortalDemo() && !sv.IsDedicated() &&
      !Q_stricmp(args[1], "testchmb_a_07")) {
    void CL_DemoTransitionFromTestChmb();
    CL_DemoTransitionFromTestChmb();
    return;
  }

#endif

  // allow a level transition to d1_trainstation_03 so the Host_Changelevel()
  // can act on it
  if (!CL_HL2Demo_MapCheck(args[1])) {
    Warning("changelevel failed: %s not found\n", args[1]);
    return;
  }

  HostState_ChangeLevelSP(args[1], args[2]);
}

// Purpose: Shut down client connection and any server
void Host_Disconnect(bool should_show_main_menu) {
#ifndef SWDS
  if (!sv.IsDedicated()) {
    cl.Disconnect(should_show_main_menu);
  }
#endif

  HostState_GameShutdown();
}

// Kill the client and any local server.
CON_COMMAND(disconnect, "Disconnect game from server.") {
  cl.demonum = -1;
  Host_Disconnect(true);
}

#define VERSION_KEY "PatchVersion="
#define PRODUCT_KEY "ProductName="
#define APPID_KEY "AppID="

void Host_Version() {
  Q_strcpy(gpszVersionString, HALFLIFE_VER_PRODUCTVERSION_INFO_STR);
  Q_strcpy(gpszProductString, HALFLIFE_VER_COMPANYNAME_STR);

  const ch *steam_inf_file_name{"steam.inf"};

  // Mod's steam.inf is first option, the steam.inf in the game GCF.
  FileHandle_t fp = g_pFileSystem->Open(steam_inf_file_name, "r");
  if (fp) {
    const u32 steam_inf_file_size{g_pFileSystem->Size(fp)};
    auto *steam_inf_data = (char *)_alloca(steam_inf_file_size + 1);
    const i32 bytes_read{
        g_pFileSystem->Read(steam_inf_data, steam_inf_file_size, fp)};

    g_pFileSystem->Close(fp);

    steam_inf_data[bytes_read] = '\0';

    // Read
    const char *steam_inf_ptr = steam_inf_data;
    int gotKeys = 0;

    while (true) {
      steam_inf_ptr = COM_Parse(steam_inf_ptr);
      if (!steam_inf_ptr) break;

      if (Q_strlen(com_token) <= 0 || gotKeys >= 3) break;

      if (!Q_strnicmp(com_token, VERSION_KEY, Q_strlen(VERSION_KEY))) {
        Q_strncpy(gpszVersionString, com_token + Q_strlen(VERSION_KEY),
                  SOURCE_ARRAYSIZE(gpszVersionString) - 1);
        gpszVersionString[SOURCE_ARRAYSIZE(gpszVersionString) - 1] = '\0';
        ++gotKeys;
        continue;
      }

      if (!Q_strnicmp(com_token, PRODUCT_KEY, Q_strlen(PRODUCT_KEY))) {
        Q_strncpy(gpszProductString, com_token + Q_strlen(PRODUCT_KEY),
                  SOURCE_ARRAYSIZE(gpszProductString) - 1);
        gpszProductString[SOURCE_ARRAYSIZE(gpszProductString) - 1] = '\0';
        ++gotKeys;
        continue;
      }

      if (!Q_strnicmp(com_token, APPID_KEY, Q_strlen(APPID_KEY))) {
        char app_id[32];
        Q_strncpy(app_id, com_token + Q_strlen(APPID_KEY),
                  SOURCE_ARRAYSIZE(app_id) - 1);
        g_iSteamAppID = atoi(app_id);
        ++gotKeys;
        continue;
      }
    }
  }
}

CON_COMMAND(version, "Print version info string.") {
  ConMsg("Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION,
         gpszVersionString, gpszProductString);
  ConMsg("Exe build: " __TIME__ " " __DATE__ " (%i)\n", build_number());
}

CON_COMMAND(pause, "Toggle the server pause state.") {
#ifndef SWDS
  if (!sv.IsDedicated()) {
    if (!cl.m_szLevelName[0]) return;
  }
#endif

  if (cmd_source == src_command) {
    Cmd_ForwardToServer(args);
    return;
  }

  if (!sv.IsPausable()) return;

  // toggle paused state
  sv.SetPaused(!sv.IsPaused());

  // send text messaage who paused the game
  sv.BroadcastPrintf("%s %s the game\n", host_client->GetClientName(),
                     sv.IsPaused() ? "paused" : "unpaused");
}

CON_COMMAND(setpause, "Set the pause state of the server.") {
#ifndef SWDS
  if (!cl.m_szLevelName[0]) return;
#endif

  if (cmd_source == src_command) {
    Cmd_ForwardToServer(args);
    return;
  }

  sv.SetPaused(true);
}

CON_COMMAND(unpause, "Unpause the game.") {
#ifndef SWDS
  if (!cl.m_szLevelName[0]) return;
#endif

  if (cmd_source == src_command) {
    Cmd_ForwardToServer(args);
    return;
  }

  sv.SetPaused(false);
}

// Kicks a user off of the server using their userid or uniqueid
CON_COMMAND(kickid, "Kick a player by userid or uniqueid, with a message.") {
  if (args.ArgC() <= 1) {
    ConMsg("Usage:  kickid <userid | uniqueid> {message}\n");
    return;
  }

  // get the first argument
  const char *arg1 = args[1];
  const char *kick_message = nullptr;
  const char *kSteamClientPrefix{"STEAM_"};
  char network_id[128];

  int user_id = -1;
  int args_start_count = 1;
  bool is_steam_id = false;

  // if the first letter is a character then we're searching for a uniqueid
  // (e.g. STEAM_).
  if (*arg1 < '0' || *arg1 > '9') {
    if (!Q_strnicmp(arg1, kSteamClientPrefix, strlen(kSteamClientPrefix)) &&
        Q_strstr(args[2], ":")) {
      // SteamID (need to reassemble it)
      Q_snprintf(network_id, SOURCE_ARRAYSIZE(network_id), "%s:%s:%s", arg1,
                 args[3], args[5]);
      args_start_count = 5;
      is_steam_id = true;
    } else {
      // some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
      // NOTE: assumed to be one argument
      Q_snprintf(network_id, SOURCE_ARRAYSIZE(network_id), "%s", arg1);
    }
  } else {
    // this is a userid
    user_id = Q_atoi(arg1);
  }

  // check for a message
  if (args.ArgC() > args_start_count) {
    usize length = 0;
    kick_message = args.ArgS();

    for (int j = 1; j <= args_start_count; j++) {
      length += strlen(args[j]) + 1;  // +1 for the space between args
    }

    // SteamIDs don't have spaces between the args values
    if (is_steam_id) length -= 5;

    // safety check
    if (length > strlen(kick_message)) {
      kick_message = nullptr;
    } else {
      kick_message += length;
    }
  }

  IClient *client = nullptr;
  int client_idx;

  // find this client
  for (client_idx = 0; client_idx < sv.GetClientCount(); client_idx++) {
    client = sv.GetClient(client_idx);

    if (!client->IsConnected()) continue;

    // searching by UserID
    if (user_id != -1) {
      // found!
      if (client->GetUserID() == user_id) break;
    } else {
      // searching by UniqueID
      if (Q_stricmp(client->GetNetworkIDString(), network_id) == 0) break;
    }
  }

  // now kick them
  if (client_idx < sv.GetClientCount()) {
    const char *who_kick =
        cmd_source != src_command ? host_client->m_Name : "Console";

    // can't kick yourself!
    if (host_client == client && !sv.IsDedicated()) return;

    if (user_id != -1 || !client->IsFakeClient()) {
      if (kick_message) {
        client->Disconnect("Kicked by %s : %s.", who_kick, kick_message);
      } else {
        client->Disconnect("Kicked by %s.", who_kick);
      }
    }
  } else {
    if (user_id != -1) {
      ConMsg("userid \"%d\" not found.\n", user_id);
    } else {
      ConMsg("uniqueid \"%s\" not found.\n", network_id);
    }
  }
}

// Kicks a user off of the server using their name
CON_COMMAND(kick, "Kick a player by name.") {
  if (args.ArgC() <= 1) {
    ConMsg("Usage:  kick <name>\n");
    return;
  }

  char name[64];
  // copy the name to a local buffer
  memset(name, 0, sizeof(name));
  Q_strncpy(name, args.ArgS(), SOURCE_ARRAYSIZE(name));
  char *client_name = name;

  // safety check
  if (client_name && client_name[0] != '\0') {
    // HACK-HACK
    // check for the name surrounded by quotes (comes in this way from rcon)
    usize len = Q_strlen(client_name) - 1;  // (minus one since we start at 0)
    if (client_name[0] == '"' && client_name[len] == '"') {
      // get rid of the quotes at the beginning and end
      client_name[len] = '\0';
      client_name++;
    }

    IClient *client = nullptr;
    int i;

    for (i = 0; i < sv.GetClientCount(); i++) {
      client = sv.GetClient(i);

      if (!client->IsConnected()) continue;

      // found!
      if (Q_strcasecmp(client->GetClientName(), client_name) == 0) break;
    }

    const char *who_kick = "Console";

    // now kick them
    if (i < sv.GetClientCount()) {
      if (cmd_source != src_command) who_kick = host_client->m_Name;

      // can't kick yourself!
      if (host_client == client && !sv.IsDedicated()) return;

      client->Disconnect("Kicked by %s.", who_kick);
    } else {
      ConMsg("name \"%s\" not found.\n", client_name);
    }
  }
}

// Dump memory stats
CON_COMMAND(memory, "Print memory stats.") {
  ConMsg("Heap used:\n");

  const usize total_memory_bytes{g_pMemAlloc->GetSize(nullptr)};
  if (total_memory_bytes == std::numeric_limits<usize>::max()) {
    ConMsg("<heap corruption detected>.\n");
  } else {
    ConMsg("%6.2f MB (%zu bytes).\n", total_memory_bytes / (1024.0f * 1024.0f),
           total_memory_bytes);
  }

#ifdef VPROF_ENABLED
  ConMsg("\nVideo memory used:\n");

  const CVProfile *profiler{&g_VProfCurrentProfile};
  f32 total{0.0f};

  for (int i = 0; i < profiler->GetNumCounters(); i++) {
    if (profiler->GetCounterGroup(i) == COUNTER_GROUP_TEXTURE_GLOBAL) {
      const f32 value = profiler->GetCounterValue(i) / (1024.0f * 1024.0f);
      total += value;

      const char *texture_mem_counter_name{profiler->GetCounterName(i)};
      if (!Q_strnicmp(texture_mem_counter_name, "TexGroup_Global_",
                      SOURCE_ARRAYSIZE("TexGroup_Global_") - 1)) {
        texture_mem_counter_name += SOURCE_ARRAYSIZE("TexGroup_Global_") - 1;
      }

      ConMsg("%6.2f MB: %s.\n", value, texture_mem_counter_name);
    }
  }

  ConMsg("------------------\n");
  ConMsg("%6.2f MB: total.\n", total);
#endif

  ConMsg("\nHunk memory used:\n");
  Hunk_Print();
}

#ifndef SWDS
// MOTODO move all demo commands to demoplayer

// Purpose: Gets number of valid demo names
int Host_GetNumDemos() {
  int c = 0;

  for (int i = 0; i < MAX_DEMOS; ++i) {
    const char *demo_name = cl.demos[i];
    if (!demo_name[0]) break;

    ++c;
  }

  return c;
}

void Host_PrintDemoList() {
  int demos_count = Host_GetNumDemos();
  int next = cl.demonum;

  if (next >= demos_count || next < 0) next = 0;

  for (int i = 0; i < MAX_DEMOS; ++i) {
    const char *demo_name = cl.demos[i];
    if (!demo_name[0]) break;

    bool is_next_demo = next == i;

    DevMsg("%3s % 2i : %20s.\n", is_next_demo ? "-->" : "   ", i, cl.demos[i]);
  }

  if (!demos_count) {
    DevMsg(
        "No demos in list, use startdemos <demoname> <demoname2> to "
        "specify.\n");
  }
}

// Con commands related to demos, not available on dedicated servers

// Purpose: Specify list of demos for the "demos" command
CON_COMMAND(startdemos, "Play demos in demo sequence.") {
  int c = args.ArgC() - 1;
  if (c > MAX_DEMOS) {
    Msg("Max %i demos in demoloop.\n", MAX_DEMOS);
    c = MAX_DEMOS;
  }

  Msg("%i demo(s) in loop.\n", c);

  for (int i = 1; i < c + 1; i++) {
    Q_strncpy(cl.demos[i - 1], args[i], SOURCE_ARRAYSIZE(cl.demos[0]));
  }

  cl.demonum = 0;

  Host_PrintDemoList();

  if (!sv.IsActive() && !demoplayer->IsPlayingBack()) {
    CL_NextDemo();
  } else {
    cl.demonum = -1;
  }
}

// Purpose: Return to looping demos, optional resume demo index
CON_COMMAND(demos, "Demo demo file sequence.") {
  const int old_demo_num = std::exchange(cl.demonum, -1);
  Host_Disconnect(false);
  cl.demonum = old_demo_num;

  if (cl.demonum == -1) cl.demonum = 0;

  if (args.ArgC() == 2) {
    int demos_count = Host_GetNumDemos();

    if (demos_count >= 1) {
      cl.demonum = std::clamp(Q_atoi(args[1]), 0, demos_count - 1);
      DevMsg("Jumping to %s.\n", cl.demos[cl.demonum]);
    }
  }

  Host_PrintDemoList();

  CL_NextDemo();
}

// Purpose: Stop current demo
CON_COMMAND_F(stopdemo, "Stop playing back a demo.", FCVAR_DONTRECORD) {
  if (!demoplayer->IsPlayingBack()) return;

  Host_Disconnect(true);
}

// Purpose: Skip to next demo
CON_COMMAND(nSextdemo, "Play next demo in sequence.") {
  if (args.ArgC() == 2) {
    int numdemos = Host_GetNumDemos();

    if (numdemos >= 1) {
      cl.demonum = std::clamp(Q_atoi(args[1]), 0, numdemos - 1);
      DevMsg("Jumping to %s.\n", cl.demos[cl.demonum]);
    }
  }

  Host_EndGame(false, "Moving to next demo...");
}

// Purpose: Print out the current demo play order
CON_COMMAND(demolist, "Print demo sequence list.") { Host_PrintDemoList(); }

CON_COMMAND_F(soundfade, "Fade client volume.", FCVAR_SERVER_CAN_EXECUTE) {
  if (args.ArgC() != 3 && args.ArgC() != 5) {
    Msg("soundfade <percent> <hold> [<out> <int>]\n");
    return;
  }

  f32 percent = std::clamp(strtof(args[1], nullptr), 0.0f, 100.0f);
  f32 holdTime = std::max(0.0f, strtof(args[2], nullptr));
  f32 inTime = 0.0f, outTime = 0.0f;

  if (args.ArgC() == 5) {
    outTime = std::max(0.0f, strtof(args[3], nullptr));
    inTime = std::max(0.0f, strtof(args[4], nullptr));
  }

  S_SoundFade(percent, holdTime, inTime, outTime);
}
#endif  // !SWDS

CON_COMMAND(killserver, "Shutdown the server.") {
  Host_Disconnect(true);

  if (!sv.IsDedicated()) {
    // Close network sockets.
    NET_SetMutiplayer(false);
  }
}

#ifndef SWDS
void Host_VoiceRecordStart_f(void) {
  if (cl.IsActive()) {
    const char *pUncompressedFile = nullptr, *pDecompressedFile = nullptr,
               *pInputFile = nullptr;

    if (voice_recordtofile.GetInt()) {
      pUncompressedFile = "voice_micdata.wav";
      pDecompressedFile = "voice_decompressed.wav";
    }

    if (voice_inputfromfile.GetInt()) {
      pInputFile = "voice_input.wav";
    }

#if !defined(NO_VOICE)
    if (Voice_RecordStart(pUncompressedFile, pDecompressedFile, pInputFile)) {
    }
#endif
  }
}

void Host_VoiceRecordStop_f(void) {
  if (cl.IsActive()) {
#if !defined(NO_VOICE)
    if (Voice_IsRecording()) {
      CL_SendVoicePacket(true);
      Voice_RecordStop();
    }
#endif
  }
}
#endif

// Purpose: Wrapper for modelloader->Print() function call
CON_COMMAND(listmodels, "List loaded models.") { modelloader->Print(); }

CON_COMMAND_F(incrementvar, "Increment specified convar value.",
              FCVAR_DONTRECORD) {
  if (args.ArgC() != 5) {
    Warning("Usage: incrementvar varName minValue maxValue delta\n");
    return;
  }

  const char *varName = args[1];
  if (!varName) {
    ConDMsg("incrementvar without a varname.\n");
    return;
  }

  const ConVar *con_var = (ConVar *)g_pCVar->FindVar(varName);
  if (!con_var) {
    ConDMsg("cvar \"%s\" not found.\n", varName);
    return;
  }

  const f32 current_val{con_var->GetFloat()};
  const f32 min_val{strtof(args[2], nullptr)};
  const f32 max_val{strtof(args[3], nullptr)};
  const f32 delta{strtof(args[4], nullptr)};
  const f32 new_val{std::clamp(current_val + delta, min_val, max_val)};

  // Conver incrementvar command to direct sets to avoid any problems with state
  // in a demo loop.
  Cbuf_AddText(va("%s %f", varName, new_val));

  ConDMsg("%s = %f.\n", con_var->GetName(), new_val);
}

CON_COMMAND_F(multvar, "Multiply specified convar value.", FCVAR_DONTRECORD) {
  if (args.ArgC() != 5) {
    Warning("Usage: multvar varName minValue maxValue factor.\n");
    return;
  }

  const char *con_var_name{args[1]};
  if (!con_var_name) {
    ConDMsg("multvar without a varname.\n");
    return;
  }

  const ConVar *con_var = g_pCVar->FindVar(con_var_name);
  if (!con_var) {
    ConDMsg("cvar \"%s\" not found.\n", con_var_name);
    return;
  }

  const f32 current_val{con_var->GetFloat()};
  const f32 min_val{strtof(args[2], nullptr)};
  const f32 max_val{strtof(args[3], nullptr)};
  const f32 factor{strtof(args[4], nullptr)};
  const f32 new_val{std::clamp(current_val * factor, min_val, max_val)};

  // Conver incrementvar command to direct sets to avoid any problems with state
  // in a demo loop.
  Cbuf_AddText(va("%s %f", con_var_name, new_val));

  ConDMsg("%s = %f.\n", con_var->GetName(), new_val);
}

CON_COMMAND(dumpstringtables, "Print string tables to console.") {
  SV_PrintStringTables();
#ifndef SWDS
  CL_PrintStringTables();
#endif
}

void Host_Quit_f() {
#if !defined(SWDS)
  if (!EngineTool_CheckQuitHandlers()) {
    return;
  }
#endif

  HostState_Shutdown();
}

// Register shared commands
ConCommand quit("quit", Host_Quit_f, "Exit the engine.");
static ConCommand cmd_exit("exit", Host_Quit_f, "Exit the engine.");

#ifndef SWDS
static ConCommand startvoicerecord("+voicerecord", Host_VoiceRecordStart_f);
static ConCommand endvoicerecord("-voicerecord", Host_VoiceRecordStop_f);
#endif

#ifdef _DEBUG
// Purpose: Force a 0 pointer crash. useful for testing minidumps
static ConCommand crash("crash",
                        []() {
                          char *p = 0;
                          *p = '\0';
                        },
                        "Cause the engine to crash (Debug!!)");
#endif

CON_COMMAND_F(flush, "Flush unlocked cache memory.", FCVAR_CHEAT) {
#if !defined(SWDS)
  g_ClientDLL->InvalidateMdlCache();
#endif  // SWDS
  serverGameDLL->InvalidateMdlCache();
  g_pDataCache->Flush(true);
}

CON_COMMAND_F(flush_locked, "Flush unlocked and locked cache memory.",
              FCVAR_CHEAT) {
#if !defined(SWDS)
  g_ClientDLL->InvalidateMdlCache();
#endif  // SWDS
  serverGameDLL->InvalidateMdlCache();
  g_pDataCache->Flush(false);
}

CON_COMMAND(cache_print,
            "cache_print [section]\nPrint out contents of cache memory.") {
  const char *section{args.ArgC() == 2 ? args[1] : nullptr};
  g_pDataCache->OutputReport(DC_DETAIL_REPORT, section);
}

CON_COMMAND(cache_print_lru,
            "cache_print_lru [section]\nPrint out contents of cache memory.") {
  const char *section{args.ArgC() == 2 ? args[1] : nullptr};
  g_pDataCache->OutputReport(DC_DETAIL_REPORT_LRU, section);
}

CON_COMMAND(cache_print_summary,
            "cache_print_summary [section]\nPrint out a summary contents of "
            "cache memory.") {
  const char *section{args.ArgC() == 2 ? args[1] : nullptr};
  g_pDataCache->OutputReport(DC_SUMMARY_REPORT, section);
}
