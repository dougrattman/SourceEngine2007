// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "ChangeGameDialog.h"

#include <cstdio>
#include <memory>
#include "base/include/windows/windows_light.h"

#include "EngineInterface.h"
#include "ModInfo.h"
#include "tier1/keyvalues.h"
#include "vgui_controls/ListPanel.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

CChangeGameDialog::CChangeGameDialog(vgui::Panel *parent)
    : Frame(parent, "ChangeGameDialog") {
  SetSize(400, 340);
  SetMinimumSize(400, 340);
  SetTitle("#GameUI_ChangeGame", true);

  m_pModList = new ListPanel(this, "ModList");
  m_pModList->SetEmptyListText("#GameUI_NoOtherGamesAvailable");
  m_pModList->AddColumnHeader(0, "ModName", "#GameUI_Game", 128);

  LoadModList();
  LoadControlSettings("Resource/ChangeGameDialog.res");

  // if there's a mod in the list, select the first one
  if (m_pModList->GetItemCount() > 0) {
    m_pModList->SetSingleSelectedItem(m_pModList->GetItemIDFromRow(0));
  }
}

CChangeGameDialog::~CChangeGameDialog() {}

// Fills the mod list.
void CChangeGameDialog::LoadModList() {
  // look for third party games
  char search_regexp[SOURCE_MAX_PATH + 5];
  strcpy_s(search_regexp, "*.*");

  // use local filesystem since it has to look outside path system, and will
  // never be used under steam
  WIN32_FIND_DATA win32_find_data{0};
  HANDLE handle = FindFirstFile(search_regexp, &win32_find_data);

  if (handle != INVALID_HANDLE_VALUE) {
    BOOL has_more_files;

    while (true) {
      if ((win32_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
          _strnicmp(win32_find_data.cFileName, ".", 1)) {
        // Check for dlls\*.dll
        char dll_dir[SOURCE_MAX_PATH + 16];
        sprintf_s(dll_dir, "%s\\gameinfo.txt", win32_find_data.cFileName);

        FILE *f;
        if (!fopen_s(&f, dll_dir, "rb")) {
          // find the description
          fseek(f, 0, SEEK_END);

          usize size = ftell(f);

          fseek(f, 0, SEEK_SET);

          std::unique_ptr<char[]> game_info{std::make_unique<char[]>(size + 1)};
          if (fread(game_info.get(), 1, size, f) == size) {
            game_info[size] = '\0';

            CModInfo mod_info;
            mod_info.LoadGameInfoFromBuffer(game_info.get());

            if (strcmp(mod_info.GetGameName(), ModInfo().GetGameName())) {
              // Add the game directory.
              _strlwr_s(win32_find_data.cFileName);

              KeyValues *mod_kv = new KeyValues("Mod");
              mod_kv->SetString("ModName", mod_info.GetGameName());
              mod_kv->SetString("ModDir", win32_find_data.cFileName);

              m_pModList->AddItem(mod_kv, 0, false, false);
            }
          }

          fclose(f);
        }
      }

      has_more_files = FindNextFile(handle, &win32_find_data);
      if (!has_more_files) break;
    }

    FindClose(handle);
  }
}

void CChangeGameDialog::OnCommand(const char *command) {
  if (!_stricmp(command, "OK")) {
    if (m_pModList->GetSelectedItemsCount() > 0) {
      KeyValues *kv = m_pModList->GetItem(m_pModList->GetSelectedItem(0));

      if (kv) {
        // change the game dir and restart the engine
        char cmd[SOURCE_MAX_PATH];
        sprintf_s(cmd, "_setgamedir %s\n", kv->GetString("ModDir"));

        engine->ClientCmd_Unrestricted(cmd);
        // Force restart of entire engine
        engine->ClientCmd_Unrestricted("_restart\n");
      }
    }
  } else if (!_stricmp(command, "Cancel")) {
    Close();
  } else {
    BaseClass::OnCommand(command);
  }
}
