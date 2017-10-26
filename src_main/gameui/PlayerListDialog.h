// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef PLAYERLISTDIALOG_H
#define PLAYERLISTDIALOG_H

#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: List of players, their ingame-name and their friends-name
//-----------------------------------------------------------------------------
class CPlayerListDialog : public vgui::Frame {
  DECLARE_CLASS_SIMPLE(CPlayerListDialog, vgui::Frame);

 public:
  CPlayerListDialog(vgui::Panel *parent);
  ~CPlayerListDialog();

  virtual void Activate();

 private:
  MESSAGE_FUNC(OnItemSelected, "ItemSelected");
  virtual void OnCommand(const char *command);

  void ToggleMuteStateOfSelectedUser();
  void RefreshPlayerProperties();

  vgui::ListPanel *m_pPlayerList;
  vgui::Button *m_pMuteButton;
};

#endif  // PLAYERLISTDIALOG_H
