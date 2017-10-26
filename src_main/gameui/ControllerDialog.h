// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef CONTROLLERDIALOG_H
#define CONTROLLERDIALOG_H

#include "OptionsDialog.h"

class CControllerDialog : public COptionsDialogXbox {
  DECLARE_CLASS_SIMPLE(CControllerDialog, COptionsDialogXbox);

 public:
  CControllerDialog(vgui::Panel *parent);

  virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
};

#endif  // CONTROLLERDIALOG_H
