// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef TOOLEDITMENUBUTTON_H
#define TOOLEDITMENUBUTTON_H

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
namespace vgui {
class Panel;
}

class CToolMenuButton;

//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton *CreateToolEditMenuButton(vgui::Panel *parent,
                                          const char *panelName,
                                          const char *text,
                                          vgui::Panel *pActionTarget);

#endif  // TOOLEDITMENUBUTTON_H
