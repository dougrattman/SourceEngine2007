// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef CONSOLE_H
#define CONSOLE_H

#include <vgui/VGUI.h>

namespace vgui {
class Panel;
}

typedef unsigned char byte;

//
// console
//
extern bool con_initialized;

void Con_Init();
void Con_Shutdown();  // Free overlay line buffer.
void Con_ClearNotify();

bool Con_IsVisible();
const char* GetConsoleLogFilename();

vgui::Panel* Con_GetConsolePanel();

#endif  // CONSOLE_H
