// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef GAMECONSOLEDIALOG_H
#define GAMECONSOLEDIALOG_H

#include "Color.h"
#include "EngineInterface.h"
#include "tier1/UtlVector.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/consoledialog.h"

//-----------------------------------------------------------------------------
// Purpose: Game/dev console dialog
//-----------------------------------------------------------------------------
class CGameConsoleDialog : public vgui::CConsoleDialog {
  DECLARE_CLASS_SIMPLE(CGameConsoleDialog, vgui::CConsoleDialog);

 public:
  CGameConsoleDialog();

 private:
  MESSAGE_FUNC(OnClosedByHittingTilde, "ClosedByHittingTilde");
  MESSAGE_FUNC_CHARPTR(OnCommandSubmitted, "CommandSubmitted", command);

  virtual void OnKeyCodeTyped(vgui::KeyCode code);
  virtual void OnCommand(const char *command);
};

#endif  // GAMECONSOLEDIALOG_H
