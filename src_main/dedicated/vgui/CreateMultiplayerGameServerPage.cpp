// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "CreateMultiplayerGameServerPage.h"

#include "base/include/windows/windows_light.h"

#include <Winsock2.h>
#include <cstdio>
#include "FileSystem.h"
#include "OfflineMode.h"
#include "mainpanel.h"
#include "netapi.h"
#include "steam.h"
#include "tier0/include/icommandline.h"
#include "tier1/keyvalues.h"
#include "vgui/IVGui.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/MessageBox.h"
#include "vstdlib/random.h"  // for SRC

#include "tier0/include/memdbgon.h"

using namespace vgui;

// filesystem_steam.cpp implements this useful function - mount all the caches
// for a given app ID.
extern void MountDependencies(int iAppId, CUtlVector<unsigned int> &depList);

bool IsEp1EraAppID(int iSteamAppId) {
  return iSteamAppId == 211 || iSteamAppId == 215;
}

// Checks the liblist.gam file for a "fallback_dir"
const char *GetLiblistFallbackDir(const char *pszGameDir) {
  static char szFallback[512];
  char szTemp[512];

  szFallback[0] = 0;

  sprintf_s(szTemp, "%s\\liblist.gam", pszGameDir);
  g_pFullFileSystem->GetLocalCopy(szTemp);

  FileHandle_t hFile = g_pFullFileSystem->Open(szTemp, "rt");

  if (hFile) {
    char szLine[512];

    // look for the line starting with 'fallback_dir'
    while (!g_pFullFileSystem->EndOfFile(hFile)) {
      // get a single line
      szLine[0] = 0;
      g_pFullFileSystem->ReadLine(szLine, sizeof(szLine) - 1, hFile);
      szLine[sizeof(szLine) - 1] = 0;

      if (!_strnicmp(szLine, "fallback_dir", 12)) {
        // we got the line, get the value between the quotes
        char *start = strchr(szLine, '\"');

        if (!start) {
          break;
        }

        char *end = strchr(start + 1, '\"');
        if (!end) {
          break;
        }

        // copy out between start and end
        int bytesToCopy = end - start - 1;
        if (bytesToCopy >= sizeof(szFallback) - 1) {
          bytesToCopy = sizeof(szFallback) - 1;
          break;
        }

        if (bytesToCopy > 0) {
          strncpy_s(szFallback, start + 1, bytesToCopy);
          szFallback[bytesToCopy] = 0;
        }
      }
    }

    g_pFullFileSystem->Close(hFile);
  }

  return szFallback;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::CCreateMultiplayerGameServerPage(
    vgui::Panel *parent, const char *name)
    : Frame(parent, name) {
  memset(&m_iServer, 0x0, sizeof(serveritem_t));

  // as we are a popup frame we need to store this seperately
  m_MainPanel = parent;
  m_pSavedData = nullptr;
  m_pGameInfo = nullptr;
  m_szMod[0] = '\0';

  SetMinimumSize(310, 350);
  SetSize(310, 350);
  SetSizeable(false);

  SetTitle("#Start_Server_Title", true);

  m_pMapList = new ComboBox(this, "MapList", 10, false);
  m_pMapList->SetEnabled(
      false);  // a mod needs to be chosen first to populate the map list
  m_pMapList->SetEditable(false);

  m_pNetworkCombo = new ComboBox(this, "NetworkCombo", 10, false);
  int defaultItem = m_pNetworkCombo->AddItem("#Internet", nullptr);
  int lanItem = m_pNetworkCombo->AddItem("#LAN", nullptr);
  if (CommandLine()->CheckParm("-steam") && IsSteamInOfflineMode()) {
    defaultItem = lanItem;
  }
  m_pNetworkCombo->ActivateItem(defaultItem);

  m_pNumPlayers = new ComboBox(this, "NumPlayers", 10, false);
  char num[3];
  for (int i = 1; i <= MAX_PLAYERS; i++) {
    sprintf_s(num, "%i", i);
    m_pNumPlayers->AddItem(num, nullptr);
  }
  m_pNumPlayers->ActivateItemByRow(23);  // 24 players by default

  m_pGameCombo = new ComboBox(this, "MODCombo", 10, false);

  m_pStartServerButton =
      new Button(this, "StartButton", "#Start_Server_Button");
  m_pStartServerButton->SetCommand("start");

  m_pCancelButton = new Button(this, "CancelButton", "#Start_Server_Cancel");
  m_pCancelButton->SetCommand("cancel");

  m_pSecureCheck = new CheckButton(this, "SecureCheck", "#Start_Server_Secure");
  m_pSecureCheck->SetSelected(true);

  LoadControlSettingsAndUserConfig("Admin/CreateMultiplayerGameServerPage.res");

  // load some defaults into the controls
  SetControlString("ServerNameEdit", "Half-Life dedicated server");
  strcpy_s(m_szGameName, "Half-Life");

  LoadMODList();

  m_pGameCombo->RequestFocus();

  // get default port from commandline if possible
  m_iPort = 27015;
  const char *portVal = nullptr;
  if (CommandLine()->CheckParm("-port", &portVal) && portVal &&
      atoi(portVal) > 0) {
    m_iPort = atoi(portVal);
  }
  SetControlInt("PortEdit", m_iPort);

  LoadConfig();

  m_szMapName[0] = 0;
  m_szHostName[0] = 0;
  m_szPassword[0] = 0;
  m_iMaxPlayers = 24;

  if (CommandLine()->CheckParm("-steam") && IsSteamInOfflineMode()) {
    m_pNetworkCombo->SetEnabled(false);
  }

  SetVisible(true);

  if (CommandLine()->CheckParm("-steam") && IsSteamInOfflineMode()) {
    MessageBox *box = new vgui::MessageBox("#Start_Server_Offline_Title",
                                           "#Start_Server_Offline_Warning");
    box->DoModal();
  }
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::~CCreateMultiplayerGameServerPage() {
  SaveConfig();
  if (m_pSavedData) {
    m_pSavedData->deleteThis();
    m_pSavedData = nullptr;
  }
  if (m_pGameInfo) {
    m_pGameInfo->deleteThis();
    m_pGameInfo = nullptr;
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnResetData() {
  m_pGameCombo->SetEnabled(true);
}

//-----------------------------------------------------------------------------
// Purpose: loads settings from a config file
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadConfig() {
  // free any old filters
  if (m_pSavedData) {
    m_pSavedData->deleteThis();
  }
  m_pSavedData = new KeyValues("Server");

  if (!m_pSavedData->LoadFromFile(g_pFullFileSystem, "Server.vdf", "CONFIG")) {
    // file not successfully loaded
  } else {
    if (m_pSavedData->FindKey("RconPassword", false)) {
      const char *password = m_pSavedData->GetString("RconPassword", "");
      if (strlen(password) > 0) {
        SetControlString("RCONPasswordEdit", password);
      }
    }
    if (m_pSavedData->FindKey("MaxPlayers", false)) {
      int maxPlayers = m_pSavedData->GetInt("MaxPlayers", -1);
      if (maxPlayers > 0 && maxPlayers <= 32) {
        m_pNumPlayers->ActivateItemByRow(maxPlayers - 1);
      }
    }
    if (m_pSavedData->FindKey("MOD", false)) {
      const char *mod = m_pSavedData->GetString("MOD", "");
      if (strlen(mod) > 0) {
        // look for the item in the dropdown
        m_szMod[0] = 0;
        for (int i = 0; i < m_pGameCombo->GetItemCount(); i++) {
          if (!m_pGameCombo->IsItemIDValid(i)) continue;

          if (!_stricmp(m_pGameCombo->GetItemUserData(i)->GetString("gamedir"),
                        mod)) {
            // item found in list, activate
            m_pGameCombo->ActivateItem(i);
            break;
          }
        }
      }
    }
    if (m_pSavedData->FindKey("Map", false)) {
      const char *map = m_pSavedData->GetString("Map", "");
      if (strlen(map) > 0) {
        SetControlString("MapList", map);
      }
    }

    if (m_pSavedData->FindKey("Network", false)) {
      int nwIndex = m_pSavedData->GetInt("Network");
      if (nwIndex > 0 && nwIndex < 2) {
        m_pNetworkCombo->ActivateItemByRow(nwIndex);
      }
    }
    if (m_pSavedData->FindKey("Secure", false)) {
      int secure = m_pSavedData->GetInt("Secure");
      m_pSecureCheck->SetSelected(secure);
    }
    if (m_pSavedData->FindKey("ServerName", false)) {
      const char *serverName = m_pSavedData->GetString("ServerName", "");
      if (strlen(serverName) > 0) {
        SetControlString("ServerNameEdit", serverName);
      }
    }
    m_iPort = m_pSavedData->GetInt("Port", m_iPort);
    SetControlInt("PortEdit", m_iPort);
  }
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SetConfig(
    const char *serverName, const char *rconPassword, int maxPlayers,
    const char *mod, const char *map, int network, int secure, int port) {
  m_pSavedData->SetInt("MaxPlayers", maxPlayers);
  m_pSavedData->SetString("RconPassword", rconPassword);
  m_pSavedData->SetString("ServerName", serverName);
  m_pSavedData->SetString("MOD", mod);
  m_pSavedData->SetString("Map", map);
  m_pSavedData->SetInt("Secure", secure);
  m_pSavedData->SetInt("Network", network);
  m_pSavedData->SetInt("Port", port);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SaveConfig() {
  m_pSavedData->SaveToFile(g_pFullFileSystem, "Server.vdf", "CONFIG");
}

//-----------------------------------------------------------------------------
// Purpose: returns true if one of the characters in the string is not a valid
// alpha numeric character
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameServerPage::BadRconChars(const char *pass) {
  bool bad = false;

  for (unsigned int i = 0; i < strlen(pass); i++) {
    bad |= !static_cast<bool>(isalnum(pass[i]));
  }
  return bad;
}

const char *ToString(int val) {
  static char text[256];
  sprintf_s(text, "%i", val);
  return text;
}

//-----------------------------------------------------------------------------
// Purpose: called to get the info from the dialog
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnCommand(const char *cmd) {
  char cvars[1024];
  int secure = GetControlInt("SecureCheck", 1);
  m_pNumPlayers->GetText(cvars, 1024);
  m_iMaxPlayers = atoi(cvars);
  strncpy_s(m_szHostName, GetControlString("ServerNameEdit", ""),
            DATA_STR_LENGTH);
  strncpy_s(m_szPassword, GetControlString("RCONPasswordEdit", ""),
            DATA_STR_LENGTH);
  m_iPort = GetControlInt("PortEdit", 27015);

  if (!_stricmp(cmd, "cancel")) {
    vgui::ivgui()->PostMessage(m_MainPanel->GetVPanel(), new KeyValues("Quit"),
                               0);
    Close();
  } else if (!_stricmp(cmd, "start")) {
    // save our current settings
    SetConfig(m_szHostName, m_szPassword, m_iMaxPlayers, m_szMod, GetMapName(),
              m_pNetworkCombo->GetActiveItem() != 0, secure, m_iPort);
    SaveConfig();

    // create the command to execute
    bool isLanOnly = (m_pNetworkCombo->GetActiveItem() != 0);
    CommandLine()->AppendParm(source::tier0::command_line_switches::kGamePath,
                              m_szMod);
    CommandLine()->AppendParm("-maxplayers", ToString(m_iMaxPlayers));
    CommandLine()->AppendParm("+sv_lan", ToString(isLanOnly));
    CommandLine()->AppendParm("+map", GetMapName());
    CommandLine()->AppendParm("-port", ToString(m_iPort));
    if (!secure)  // if they don't want it secure...
    {
      CommandLine()->AppendParm("-insecure", "");
    }

    //	strncpy(m_szPassword, GetControlString("RCONPasswordEdit", ""),
    // DATA_STR_LENGTH);
    if (strlen(m_szPassword) < 3 || BadRconChars(m_szPassword)) {
      MessageBox *dlg = new MessageBox("#Start_Server_RCON_Error_Title",
                                       "#Start_Server_RCON_Error");
      dlg->DoModal();
    } else {
      sprintf_s(cvars,
                "rcon_password \"%s\"\nsetmaster enable\nhostname \"%s\"\n",
                m_szPassword, m_szHostName);

      m_pGameCombo->SetEnabled(false);
      m_pNumPlayers->SetEnabled(false);

      netadr_t local;
      net->GetLocalIP(&local);
      local.port = ::htons(m_iPort);

      for (int i = 0; i < 4; i++) {
        m_iServer.ip[i] = local.ip[i];
      }
      m_iServer.port = (local.port & 0xff) << 8 | (local.port & 0xff00) >> 8;

      strcpy_s(m_iServer.name, m_szHostName);
      strcpy_s(m_iServer.map, GetMapName());
      strcpy_s(m_iServer.gameDir, m_szMod);
      m_iServer.maxPlayers = m_iMaxPlayers;

      SetVisible(false);

      // mount the caches
      KeyValues *gameData = m_pGameCombo->GetActiveItemUserData();
      if (CommandLine()->CheckParm("-steam")) {
        if (gameData) {
          KeyValues *pFileSystem = gameData->FindKey("FileSystem");
          if (!pFileSystem)
            Error("Game %s missing FileSystem key.",
                  gameData->GetString("game"));

          // Mods just specify their app ID (CS, HL2, HL2MP, etc), and it mounts
          // all the necessary caches.
          int iAppId = pFileSystem->GetInt("SteamAppId");
          if (iAppId) {
            CUtlVector<unsigned int> depList;
            MountDependencies(iAppId, depList);

            char gameinfoFilename[SOURCE_MAX_PATH];
            sprintf_s(gameinfoFilename, "%s\\gameinfo.txt", m_iServer.gameDir);
            g_pFullFileSystem->GetLocalCopy(gameinfoFilename);
          }
        }
      }

      // Launch the old dedicated server if necessary.
      if (LaunchOldDedicatedServer(gameData)) {
        vgui::ivgui()->PostMessage(m_MainPanel->GetVPanel(),
                                   new KeyValues("Quit"), 0);
        Close();
      }

      CMainPanel::GetInstance()->StartServer(cvars);
    }
  }
}

bool CCreateMultiplayerGameServerPage::LaunchOldDedicatedServer(
    KeyValues *pGameInfo) {
  return false;
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMODList() {
  m_pGameCombo->DeleteAllItems();

  // add steam games
  if (CommandLine()->CheckParm("-steam")) {
    const char *pSteamGamesFilename = "hlds_steamgames.vdf";

    KeyValues *gamesFile = new KeyValues(pSteamGamesFilename);

    if (gamesFile->LoadFromFile(g_pFullFileSystem, pSteamGamesFilename,
                                nullptr)) {
      for (KeyValues *kv = gamesFile->GetFirstSubKey(); kv != nullptr;
           kv = kv->GetNextKey()) {
        const char *pGameDir = kv->GetString("gamedir", nullptr);
        if (!pGameDir)
          Error("Mod %s in %s missing 'gamedir'.", kv->GetName(),
                pSteamGamesFilename);

        AddMod(pGameDir, pSteamGamesFilename, kv);
      }
    }
    gamesFile->deleteThis();
    gamesFile = nullptr;
  }

  // For backward compatibility, check inside the dedicated server's own
  // directory for mods.
  LoadModListInDirectory(".");

  // Also, check in SourceMods.
  char sourceModsDir[SOURCE_MAX_PATH];
  if (system()->GetRegistryString(
          "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\SourceModInstallPath",
          sourceModsDir, sizeof(sourceModsDir)))
    LoadModListInDirectory(sourceModsDir);

  m_pGameCombo->ActivateItem(0);
}

void CCreateMultiplayerGameServerPage::LoadModListInDirectory(
    const char *pDirectoryName) {
  char searchString[SOURCE_MAX_PATH * 2];
  strcpy_s(searchString, pDirectoryName);
  Q_AppendSlash(searchString, sizeof(searchString));
  strcat_s(searchString, "*.*");

  FileFindHandle_t findHandle = 0;
  const char *filename =
      g_pFullFileSystem->FindFirst(searchString, &findHandle);
  while (filename) {
    // add to the mod list
    if (filename[0] != '.' && g_pFullFileSystem->FindIsDirectory(findHandle)) {
      char fullFilename[SOURCE_MAX_PATH];
      if (_stricmp(pDirectoryName, ".") == 0) {
        // If we don't do this, then the games in hlds_steamgames.vdf will get
        // listed twice since their gamedir is listed as "cstrike" and "hl2mp",
        // not ".\cstrike" or ".\hl2mp".
        strcpy_s(fullFilename, filename);
      } else {
        strcpy_s(fullFilename, pDirectoryName);
        Q_AppendSlash(fullFilename, sizeof(fullFilename));
        strcat_s(fullFilename, filename);
      }

      LoadPossibleMod(fullFilename);
    }

    filename = g_pFullFileSystem->FindNext(findHandle);
  }
  g_pFullFileSystem->FindClose(findHandle);
}

void CCreateMultiplayerGameServerPage::LoadPossibleMod(
    const char *pGameDirName) {
  char gameInfoFilename[1024];
  sprintf_s(gameInfoFilename, "%s\\gameinfo.txt", pGameDirName);
  if (!g_pFullFileSystem->FileExists(gameInfoFilename)) return;

  // don't want to add single player games to the list
  KeyValues *pGameInfo = new KeyValues("GameInfo");
  bool loadedFile =
      pGameInfo->LoadFromFile(g_pFullFileSystem, gameInfoFilename);
  if (!loadedFile) return;

  AddMod(pGameDirName, gameInfoFilename, pGameInfo);

  pGameInfo->deleteThis();
  pGameInfo = nullptr;
}

void CCreateMultiplayerGameServerPage::AddMod(const char *pGameDirName,
                                              const char *pGameInfoFilename,
                                              KeyValues *pGameInfo) {
  // Don't re-add something with the same gamedir name (this can happen with
  // games listed in hlds_steamgames.vdf, since after the first time a game is
  // run, it'll also have a gameinfo.txt that will be found).
  for (int i = 0; i < m_pGameCombo->GetItemCount(); i++) {
    if (!m_pGameCombo->IsItemIDValid(i)) continue;

    if (_stricmp(m_pGameCombo->GetItemUserData(i)->GetString("gamedir"),
                 pGameDirName) == 0)
      return;
  }

  // If this mod supports multiplayer, then we'll add it.
  const char *gameType = pGameInfo->GetString("type", "singleplayer_only");
  if (_stricmp(gameType, "singleplayer_only") != 0) {
    // Validate the gameinfo.txt format..
    KeyValues *pSub = pGameInfo->FindKey("FileSystem");
    if (!pSub) Error("%s missing FileSystem key.", pGameInfoFilename);

    int iSteamAppId = pSub->GetInt("SteamAppId", -1);
    if (iSteamAppId == -1)
      Error("%s missing FileSystem\\SteamAppId key.", pGameInfoFilename);

    // If we're not supporting old games in here and its appid is old, forget
    // about it.
    if (IsEp1EraAppID(iSteamAppId)) return;

    const char *gameName = pGameInfo->GetString("game", nullptr);
    if (!gameName) Error("%s missing 'game' key.", pGameInfoFilename);

    // add to drop-down combo and mod list
    KeyValues *kv = pGameInfo->MakeCopy();
    kv->SetString("gamedir", pGameDirName);

    m_pGameCombo->AddItem(gameName, kv);

    kv->deleteThis();
    kv = nullptr;
  }
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps for the given path into the map
// list Returns: number of maps loaded into the list
//-----------------------------------------------------------------------------
int CCreateMultiplayerGameServerPage::LoadMaps(const char *pszMod) {
  // iterate the filesystem getting the list of all the files
  // UNDONE: steam wants this done in a special way, need to support that
  FileFindHandle_t findHandle = 0;
  char szSearch[256];
  sprintf_s(szSearch, "%s/maps/*.bsp", pszMod);

  int iMapsFound = 0;

  const char *pszFilename = g_pFullFileSystem->FindFirst(szSearch, &findHandle);

  KeyValues *hiddenMaps = nullptr;
  if (m_pGameInfo) {
    hiddenMaps = m_pGameInfo->FindKey("hidden_maps");
  }

  while (pszFilename) {
    // remove the text 'maps/' and '.bsp' from the file name to get the map name
    char mapname[256];
    const char *str = strstr(pszFilename, "maps");
    if (str) {
      strncpy_s(mapname, str + 5, sizeof(mapname) - 1);  // maps + \\ = 5
    } else {
      strncpy_s(mapname, pszFilename, sizeof(mapname) - 1);
    }

    char *ext = strstr(mapname, ".bsp");
    if (ext) {
      *ext = 0;
    }

    //!! hack: strip out single player HL maps
    // this needs to be specified in a seperate file
    if ((mapname[0] == 'c' || mapname[0] == 't') && mapname[2] == 'a' &&
        mapname[1] >= '0' && mapname[1] <= '5') {
      goto nextFile;
    }

    // strip out maps that shouldn't be displayed
    if (hiddenMaps) {
      if (hiddenMaps->GetInt(mapname, 0)) {
        goto nextFile;
      }
    }

    iMapsFound++;

    // add to the map list
    m_pMapList->AddItem(mapname, nullptr);

    // get the next file
  nextFile:
    pszFilename = g_pFullFileSystem->FindNext(findHandle);
  }

  g_pFullFileSystem->FindClose(findHandle);

  return iMapsFound;
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMapList() {
  int iMapsFound = 0;

  // clear the current list (if any)
  m_pMapList->DeleteAllItems();

  Assert(strlen(m_szMod) > 0);
  if (strlen(m_szMod) < 1) {
    m_pMapList->SetEnabled(false);
    return;
  }

  m_pMapList->SetEnabled(true);
  m_pStartServerButton->SetEnabled(true);

  if (CommandLine()->CheckParm("-steam")) {
    KeyValues *userData = m_pGameCombo->GetActiveItemUserData();
    if (userData && userData->GetString("DedicatedServerStartMap", nullptr)) {
      // set only
      m_pMapList->AddItem(userData->GetString("DedicatedServerStartMap"),
                          nullptr);
      m_pMapList->ActivateItemByRow(0);
      m_pMapList->SetEnabled(false);
      return;
    }
  }

  // Load the maps for the GameDir
  iMapsFound += LoadMaps(m_szMod);

  // If we're using a "fallback_dir" in liblist.gam then include those maps...
  const char *pszFallback = GetLiblistFallbackDir(m_szMod);
  if (pszFallback[0]) {
    iMapsFound += LoadMaps(pszFallback);
  }

  if (iMapsFound < 1) {
    m_pMapList->SetEnabled(false);
  }

  // set the first item to be selected
  m_pMapList->ActivateItemByRow(0);
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the map selected from the map combo
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameServerPage::GetMapName() {
  m_pMapList->GetText(m_szMapName, DATA_STR_LENGTH);
  return m_szMapName;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameServerPage::GetRconPassword() {
  return m_szPassword;
}

//-----------------------------------------------------------------------------
// Purpose: updates "s" with the details of the chosen server to run
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::GetServer(serveritem_t &s) {
  s = m_iServer;
  strcpy_s(s.name, m_iServer.name);
  strcpy_s(s.rconPassword, m_iServer.rconPassword);
  memcpy(s.ip, m_iServer.ip, sizeof(m_iServer.ip));
  memcpy(s.pings, m_iServer.pings, 3 * sizeof(int));
  strcpy_s(s.gameDir, m_iServer.gameDir);
  strcpy_s(s.map, m_iServer.map);
  strcpy_s(s.gameDescription, m_iServer.gameDescription);
}

//-----------------------------------------------------------------------------
// Purpose: Handles changes to combo boxes
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnTextChanged(Panel *panel) {
  if (panel == m_pGameCombo) {
    // see if we should update the game name
    bool updateHostname = false;
    char hostname[256];
    GetControlString("ServerNameEdit", m_szHostName, sizeof(m_szHostName));
    sprintf_s(hostname, "%s dedicated server", m_szGameName);
    if (!_stricmp(m_szHostName, hostname)) {
      updateHostname = true;
    }

    // update the game name
    m_pGameCombo->GetText(m_szGameName, sizeof(m_szGameName));

    // Copy the gamedir into m_szMod.
    KeyValues *gameData = m_pGameCombo->GetActiveItemUserData();
    if (!gameData) Error("Missing gameData for active item.");

    const char *pGameDir = gameData->GetString("gamedir", nullptr);
    if (!pGameDir) Error("Game %s missing 'gamedir' key.", m_szGameName);

    strcpy_s(m_szMod, pGameDir);

    // re-load the GameInfo KeyValues
    if (m_pGameInfo) {
      m_pGameInfo->deleteThis();
    }
    char liblist[1024];
    sprintf_s(liblist, "%s\\gameinfo.txt", m_szMod);
    m_pGameInfo = new KeyValues("GameInfo");
    m_pGameInfo->LoadFromFile(g_pFullFileSystem, liblist);

    // redo the hostname with the new game name
    if (updateHostname) {
      sprintf_s(hostname, "%s dedicated server", m_szGameName);
      SetControlString("ServerNameEdit", hostname);
    }

    // reload the list of maps we display
    LoadMapList();
  }
}

#endif  // _WIN32
