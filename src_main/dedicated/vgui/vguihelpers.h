// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_DEDICATED_VGUIHELPERS_H_
#define SOURCE_DEDICATED_VGUIHELPERS_H_

#include "tier1/interface.h"

int StartVGUI(CreateInterfaceFn dedicatedFactory);
void StopVGUI();
void RunVGUIFrame();
bool VGUIIsRunning();
bool VGUIIsStopping();
bool VGUIIsInConfig();
void VGUIFinishedConfig();
void VGUIPrintf(const char *msg);

#endif  // SOURCE_DEDICATED_VGUIHELPERS_H_
