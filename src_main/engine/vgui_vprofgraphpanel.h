// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VGUI_VPROFGRAPHPANEL_H
#define VGUI_VPROFGRAPHPANEL_H

namespace vgui {
class Panel;
}

// Creates/destroys the vprof graph panel
void CreateVProfGraphPanel(vgui::Panel *pParent);
void DestroyVProfGraphPanel();

#endif  // VGUI_VPROFGRAPHPANEL_H
