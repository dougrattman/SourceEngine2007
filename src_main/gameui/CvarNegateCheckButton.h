// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef CVARNEGATECHECKBUTTON_H
#define CVARNEGATECHECKBUTTON_H

#include "vgui_controls/CheckButton.h"

class CCvarNegateCheckButton : public vgui::CheckButton {
  DECLARE_CLASS_SIMPLE(CCvarNegateCheckButton, vgui::CheckButton);

 public:
  CCvarNegateCheckButton(vgui::Panel *parent, const char *panelName,
                         const char *text, char const *cvarname);
  ~CCvarNegateCheckButton();

  virtual void SetSelected(bool state);
  virtual void Paint();

  void Reset();
  void ApplyChanges();
  bool HasBeenModified();

 private:
  MESSAGE_FUNC(OnButtonChecked, "CheckButtonChecked");

  char *m_pszCvarName;
  bool m_bStartState;
};

#endif  // CVARNEGATECHECKBUTTON_H
