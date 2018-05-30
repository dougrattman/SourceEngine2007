// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VGUI_TOOLS_H
#define VGUI_TOOLS_H

#include "interface.h"

#include <vgui/VGUI.h>

// Every tool must expose this back to us
extern char const *GetVGuiControlsModuleName();

bool VGui_Startup(CreateInterfaceFn appSystemFactory);
bool VGui_PostInit();
void VGui_Shutdown();

// Must be implemented by .dll
void VGUI_CreateToolRootPanel();
void VGUI_DestroyToolRootPanel();
vgui::VPANEL VGui_GetToolRootPanel();

#endif  // VGUI_TOOLS_H
