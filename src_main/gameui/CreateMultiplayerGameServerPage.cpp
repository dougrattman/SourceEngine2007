// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "CreateMultiplayerGameServerPage.h"

#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "ModInfo.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/RadioButton.h"
#include "vstdlib/random.h"  // for SRC

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

#define RANDOM_MAP "#GameUI_RandomMap"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::CCreateMultiplayerGameServerPage(
    vgui::Panel *parent, const char *name)
    : PropertyPage(parent, name) {
  m_pSavedData = NULL;

  // we can use this if we decide we want to put "listen server" at the end of
  // the game name
  m_pMapList = new ComboBox(this, "MapList", 12, false);

  m_pEnableBotsCheck = new CheckButton(this, "EnableBotsCheck", "");
  m_pEnableBotsCheck->SetVisible(false);
  m_pEnableBotsCheck->SetEnabled(false);

  LoadControlSettings("Resource/CreateMultiplayerGameServerPage.res");

  LoadMapList();
  m_szMapName[0] = 0;

  // initialize hostname
  SetControlString("ServerNameEdit", ModInfo().GetGameName());  // szHostName);

  // initialize password
  //	SetControlString("PasswordEdit",
  // engine->pfnGetCvarString("sv_password"));
  ConVarRef var("sv_password");
  if (var.IsValid()) {
    SetControlString("PasswordEdit", var.GetString());
  }
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::~CCreateMultiplayerGameServerPage() {}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::EnableBots(KeyValues *data) {
  m_pSavedData = data;

  int quota = data->GetInt("bot_quota", 0);
  SetControlInt("BotQuotaCombo", quota);
  m_pEnableBotsCheck->SetSelected((quota > 0));

  int difficulty = data->GetInt("bot_difficulty", 0);
  difficulty = std::max(difficulty, 0);
  difficulty = std::min(3, difficulty);

  char buttonName[64];
  Q_snprintf(buttonName, sizeof(buttonName), "SkillLevel%d", difficulty);
  vgui::RadioButton *button =
      dynamic_cast<vgui::RadioButton *>(FindChildByName(buttonName));
  if (button) {
    button->SetSelected(true);
  }
}

//-----------------------------------------------------------------------------
// Purpose: called to get the info from the dialog
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnApplyChanges() {
  KeyValues *kv = m_pMapList->GetActiveItemUserData();
  strncpy(m_szMapName, kv->GetString("mapname", ""), DATA_STR_LENGTH);

  if (m_pSavedData) {
    int quota = GetControlInt("BotQuotaCombo", 0);
    if (!m_pEnableBotsCheck->IsSelected()) {
      quota = 0;
    }
    m_pSavedData->SetInt("bot_quota", quota);
    ConVarRef bot_quota("bot_quota");
    bot_quota.SetValue(quota);

    int difficulty = 0;
    for (int i = 0; i < 4; ++i) {
      char buttonName[64];
      Q_snprintf(buttonName, sizeof(buttonName), "SkillLevel%d", i);
      vgui::RadioButton *button =
          dynamic_cast<vgui::RadioButton *>(FindChildByName(buttonName));
      if (button) {
        if (button->IsSelected()) {
          difficulty = i;
          break;
        }
      }
    }
    m_pSavedData->SetInt("bot_difficulty", difficulty);
    ConVarRef bot_difficulty("bot_difficulty");
    bot_difficulty.SetValue(difficulty);
  }
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMaps(const char *pszPathID) {
  KeyValues *hiddenMaps = ModInfo().GetHiddenMaps();

  FileFindHandle_t map_finder = NULL;
  const char *map_path =
      g_pFullFileSystem->FindFirst("maps/*.bsp", &map_finder);
  const char *str = nullptr;
  char *ext = nullptr;

  while (map_path) {
    char map_name[256];

    // FindFirst ignores the pszPathID, so check it here
    // TODO: this doesn't find maps in fallback dirs
    _snprintf(map_name, ARRAYSIZE(map_name), "maps/%s", map_path);
    if (!g_pFullFileSystem->FileExists(map_name, pszPathID)) {
      goto nextFile;
    }

    // remove the text 'maps/' and '.bsp' from the file name to get the map name

    str = Q_strstr(map_path, "maps");
    if (str) {
      strncpy(map_name, str + 5, ARRAYSIZE(map_name) - 1);  // maps + \\ = 5
    } else {
      strncpy(map_name, map_path, ARRAYSIZE(map_name) - 1);
    }
    ext = Q_strstr(map_name, ".bsp");
    if (ext) {
      *ext = 0;
    }

    //!! hack: strip out single player HL maps
    // this needs to be specified in a seperate file
    if (!stricmp(ModInfo().GetGameName(), "Half-Life") &&
        (map_name[0] == 'c' || map_name[0] == 't') && map_name[2] == 'a' &&
        map_name[1] >= '0' && map_name[1] <= '5') {
      goto nextFile;
    }

    // strip out maps that shouldn't be displayed
    if (hiddenMaps) {
      if (hiddenMaps->GetInt(map_name, 0)) {
        goto nextFile;
      }
    }

    // add to the map list
    m_pMapList->AddItem(map_name, new KeyValues("data", "mapname", map_name));

    // get the next file
  nextFile:
    map_path = g_pFullFileSystem->FindNext(map_finder);
  }
  g_pFullFileSystem->FindClose(map_finder);
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMapList() {
  // clear the current list (if any)
  m_pMapList->DeleteAllItems();

  // add special "name" to represent loading a randomly selected map
  m_pMapList->AddItem(RANDOM_MAP, new KeyValues("data", "mapname", RANDOM_MAP));

  // iterate the filesystem getting the list of all the files
  // UNDONE: steam wants this done in a special way, need to support that
  const char *pathID = "MOD";
  if (!stricmp(ModInfo().GetGameName(), "Half-Life")) {
    pathID = NULL;  // hl is the base dir
  }

  // Load the GameDir maps
  LoadMaps(pathID);

  // If we're not the Valve directory and we're using a "fallback_dir" in
  // gameinfo.txt then include those maps... (pathID is NULL if we're
  // "Half-Life")
  const char *pszFallback = ModInfo().GetFallbackDir();
  if (pathID && pszFallback[0]) {
    LoadMaps("GAME_FALLBACK");
  }

  // set the first item to be selected
  m_pMapList->ActivateItem(0);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameServerPage::IsRandomMapSelected() {
  const char *mapname =
      m_pMapList->GetActiveItemUserData()->GetString("mapname");
  if (!stricmp(mapname, RANDOM_MAP)) {
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameServerPage::GetMapName() {
  int count = m_pMapList->GetItemCount();

  // if there is only one entry it's the special "select random map" entry
  if (count <= 1) return NULL;

  const char *mapname =
      m_pMapList->GetActiveItemUserData()->GetString("mapname");
  if (!strcmp(mapname, RANDOM_MAP)) {
    int which = RandomInt(1, count - 1);
    mapname = m_pMapList->GetItemUserData(which)->GetString("mapname");
  }

  return mapname;
}

//-----------------------------------------------------------------------------
// Purpose: Sets currently selected map in the map combobox
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SetMap(const char *mapName) {
  for (int i = 0; i < m_pMapList->GetItemCount(); i++) {
    if (!m_pMapList->IsItemIDValid(i)) continue;

    if (!stricmp(m_pMapList->GetItemUserData(i)->GetString("mapname"),
                 mapName)) {
      m_pMapList->ActivateItem(i);
      break;
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnCheckButtonChecked() {
  SetControlEnabled("SkillLevel0", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("SkillLevel1", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("SkillLevel2", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("SkillLevel3", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("BotQuotaCombo", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("BotQuotaLabel", m_pEnableBotsCheck->IsSelected());
  SetControlEnabled("BotDifficultyLabel", m_pEnableBotsCheck->IsSelected());
}
