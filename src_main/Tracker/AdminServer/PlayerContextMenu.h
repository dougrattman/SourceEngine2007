// Copyright © 1996-2001, Valve LLC, All rights reserved.

#ifndef PLAYERCONTEXTMENU_H
#define PLAYERCONTEXTMENU_H

#include "vgui_controls/Menu.h"

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CPlayerContextMenu : public vgui::Menu {
 public:
  CPlayerContextMenu(vgui::Panel *parent);
  ~CPlayerContextMenu();

  // call this to activate the menu
  void ShowMenu(vgui::Panel *target, unsigned int serverID);

 private:
  vgui::Panel *parent;  // so we can send it messages
};

#endif  // PLAYERCONTEXTMENU_H
