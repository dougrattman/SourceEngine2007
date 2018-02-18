// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VGUI_ASKCONNECTPANEL_H
#define VGUI_ASKCONNECTPANEL_H

#include "vgui_controls/Panel.h"

void SetupDefaultAskConnectAcceptKey();
vgui::Panel *CreateAskConnectPanel(vgui::VPANEL parent);

void ShowAskConnectPanel(const char *pConnectToHostName, float flDuration);
void HideAskConnectPanel();

bool IsAskConnectPanelActive(char *pHostName, int maxHostNameBytes);

#endif  // VGUI_ASKCONNECTPANEL_H
