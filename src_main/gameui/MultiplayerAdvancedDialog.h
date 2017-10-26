// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef MULTIPLAYERADVANCEDDIALOG_H
#define MULTIPLAYERADVANCEDDIALOG_H

#include "ScriptObject.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: Displays a game-specific list of options
//-----------------------------------------------------------------------------
class CMultiplayerAdvancedDialog : public vgui::Frame {
  DECLARE_CLASS_SIMPLE(CMultiplayerAdvancedDialog, vgui::Frame);

 public:
  CMultiplayerAdvancedDialog(vgui::Panel *parent);
  ~CMultiplayerAdvancedDialog();

  virtual void Activate();

 private:
  void CreateControls();
  void DestroyControls();
  void GatherCurrentValues();
  void SaveValues();

  CInfoDescription *m_pDescription;

  mpcontrol_t *m_pList;

  CPanelListPanel *m_pListPanel;

  virtual void OnCommand(const char *command);
  virtual void OnClose();
  virtual void OnKeyCodeTyped(vgui::KeyCode code);
};

#endif  // MULTIPLAYERADVANCEDDIALOG_H
