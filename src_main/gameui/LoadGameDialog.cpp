// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "LoadGameDialog.h"

#include "EngineInterface.h"
#include "FileSystem.h"
#include "tier1/keyvalues.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/QueryBox.h"

 
#include "tier0/include/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose:Constructor
//-----------------------------------------------------------------------------
CLoadGameDialog::CLoadGameDialog(vgui::Panel *parent)
    : BaseClass(parent, "LoadGameDialog") {
  SetDeleteSelfOnClose(true);
  SetBounds(0, 0, 512, 384);
  SetMinimumSize(256, 300);
  SetSizeable(true);

  SetTitle("#GameUI_LoadGame", true);

  vgui::Button *cancel = new vgui::Button(this, "Cancel", "#GameUI_Cancel");
  cancel->SetCommand("Close");

  LoadControlSettings("resource/LoadGameDialog.res");

  SetControlEnabled("delete", false);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadGameDialog::~CLoadGameDialog() {}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadGameDialog::OnCommand(const char *command) {
  if (!stricmp(command, "loadsave")) {
    int saveIndex = GetSelectedItemSaveIndex();
    if (m_SaveGames.IsValidIndex(saveIndex)) {
      const char *shortName = m_SaveGames[saveIndex].szShortName;
      if (shortName && shortName[0]) {
        // Load the game, return to top and switch to engine
        char sz[256];
        Q_snprintf(sz, sizeof(sz), "progress_enable\nload %s\n", shortName);

        engine->ClientCmd_Unrestricted(sz);

        // Close this dialog
        OnClose();
      }
    }
  } else if (!stricmp(command, "Delete")) {
    int saveIndex = GetSelectedItemSaveIndex();
    if (m_SaveGames.IsValidIndex(saveIndex)) {
      // confirm the deletion
      QueryBox *box = new QueryBox("#GameUI_ConfirmDeleteSaveGame_Title",
                                   "#GameUI_ConfirmDeleteSaveGame_Info");
      box->AddActionSignalTarget(this);
      box->SetOKButtonText("#GameUI_ConfirmDeleteSaveGame_OK");
      box->SetOKCommand(new KeyValues("Command", "command", "DeleteConfirmed"));
      box->DoModal();
    }
  } else if (!stricmp(command, "DeleteConfirmed")) {
    int saveIndex = GetSelectedItemSaveIndex();
    if (m_SaveGames.IsValidIndex(saveIndex)) {
      DeleteSaveGame(m_SaveGames[saveIndex].szFileName);

      // reset the list
      ScanSavedGames();
      m_pGameList->MoveScrollBarToTop();
    }
  } else {
    BaseClass::OnCommand(command);
  }
}
