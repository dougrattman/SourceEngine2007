// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef COMMENTARYEXPLANATIONDIALOG_H
#define COMMENTARYEXPLANATIONDIALOG_H

#include "tier1/UtlVector.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog that explains the commentary mode
//-----------------------------------------------------------------------------
class CCommentaryExplanationDialog : public vgui::Frame {
  DECLARE_CLASS_SIMPLE(CCommentaryExplanationDialog, vgui::Frame);

 public:
  CCommentaryExplanationDialog(vgui::Panel *parent, char *pszFinishCommand);
  ~CCommentaryExplanationDialog();

  virtual void OnKeyCodePressed(vgui::KeyCode code);
  virtual void OnCommand(const char *command);
  virtual void OnClose(void);

 private:
  char m_pszFinishCommand[512];
};

#endif  // COMMENTARYEXPLANATIONDIALOG_H
