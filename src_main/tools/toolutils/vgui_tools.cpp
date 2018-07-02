// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "toolutils/vgui_tools.h"

#include <KeyValues.h>
#include <dme_controls/dmeControls.h>
#include <vgui/IInput.h>
#include <vgui/IVGui.h>
#include <vgui/isurface.h>
#include <vgui_controls/Panel.h>
#include "ienginevgui.h"
#include "tier0/include/vprof.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose:
// Input  : appSystemFactory -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_Startup(CreateInterfaceFn appSystemFactory) {
  // All of the various tools .dlls expose GetVGuiControlsModuleName() to us to
  // make sure we don't have communication across .dlls
  if (!vgui::VGui_InitDmeInterfacesList(GetVGuiControlsModuleName(),
                                        &appSystemFactory, 1))
    return false;

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :  -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_PostInit() {
  // Create any root panels for .dll
  VGUI_CreateToolRootPanel();

  // Make sure we have a panel
  VPANEL root = VGui_GetToolRootPanel();
  if (!root) {
    return false;
  }
  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :  -
//-----------------------------------------------------------------------------
void VGui_Shutdown() {
  VGUI_DestroyToolRootPanel();

  // Make sure anything "marked for deletion"
  //  actually gets deleted before this dll goes away
  vgui::ivgui()->RunFrame();
}
