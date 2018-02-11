// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "ControllerDialog.h"

#include "BasePanel.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

CControllerDialog::CControllerDialog(vgui::Panel *parent)
    : BaseClass(parent,
                true)  // TRUE second param says we want the controller options
{}

void CControllerDialog::ApplySchemeSettings(vgui::IScheme *pScheme) {
  BaseClass::ApplySchemeSettings(pScheme);

  SetControlString("TitleLabel", "#GameUI_Controller");
}
