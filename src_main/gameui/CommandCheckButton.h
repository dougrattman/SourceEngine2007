// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef COMMANDCHECKBUTTON_H
#define COMMANDCHECKBUTTON_H

#include "vgui_controls/CheckButton.h"

class CCommandCheckButton : public vgui::CheckButton {
 public:
  CCommandCheckButton(vgui::Panel *parent, const char *panelName,
                      const char *text, const char *downcmd, const char *upcmd);
  ~CCommandCheckButton();

  //	virtual void	OnCheckButtonChecked(vgui::Panel *panel);
  virtual void SetSelected(bool state);

 private:
  typedef vgui::CheckButton BaseClass;

  char *m_pszDown;
  char *m_pszUp;
};
#endif  // COMMANDCHECKBUTTON_H
