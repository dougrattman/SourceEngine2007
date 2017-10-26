// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef LOADGAMEDIALOG_H
#define LOADGAMEDIALOG_H

#include "BasePanel.h"
#include "BaseSaveGameDialog.h"
#include "SaveGameBrowserDialog.h"
#include "SaveGameDialog.h"
#include "vgui_controls/KeyRepeat.h"

//-----------------------------------------------------------------------------
// Purpose: Displays game loading options
//-----------------------------------------------------------------------------
class CLoadGameDialog : public CBaseSaveGameDialog {
  DECLARE_CLASS_SIMPLE(CLoadGameDialog, CBaseSaveGameDialog);

 public:
  CLoadGameDialog(vgui::Panel *parent);
  ~CLoadGameDialog();

  virtual void OnCommand(const char *command);
};

//
//
//

class CLoadGameDialogXbox : public CSaveGameBrowserDialog {
  DECLARE_CLASS_SIMPLE(CLoadGameDialogXbox, CSaveGameBrowserDialog);

 public:
  CLoadGameDialogXbox(vgui::Panel *parent);
  virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
  virtual void OnCommand(const char *command);
  virtual void PerformSelectedAction(void);
  virtual void PerformDeletion(void);
  virtual void UpdateFooterOptions(void);

 private:
  void DeleteSaveGame(const SaveGameDescription_t *pSaveDesc);
};

#endif  // LOADGAMEDIALOG_H
