// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef KEYTOGGLECHECKBUTTON_H
#define KEYTOGGLECHECKBUTTON_H

#include "vgui_controls/CheckButton.h"

class CKeyToggleCheckButton : public vgui::CheckButton {
 public:
  CKeyToggleCheckButton(vgui::Panel *parent, const char *panelName,
                        const char *text, char const *keyname,
                        char const *cmdname);
  ~CKeyToggleCheckButton();

  // virtual void	SetSelected( bool state );
  virtual void Paint();

  void Reset();
  void ApplyChanges();
  bool HasBeenModified();

 private:
  typedef vgui::CheckButton BaseClass;

  char *m_pszKeyName;
  char *m_pszCmdName;

  bool m_bStartValue;
};
#endif  // KEYTOGGLECHECKBUTTON_H
