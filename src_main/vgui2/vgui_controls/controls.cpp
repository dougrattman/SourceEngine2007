// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "vgui_controls/Controls.h"

#include <locale.h>

#include "tier0/include/memdbgon.h"

extern int
    g_nYou_Must_Add_Public_Vgui_Controls_Vgui_ControlsCpp_To_Your_Project;

namespace vgui {
static char g_szControlsModuleName[256];

// Purpose: Initializes the controls
extern "C" {
extern int _heapmin();
}

bool VGui_InitInterfacesList(const char *moduleName,
                             CreateInterfaceFn *factoryList, int numFactories) {
  g_nYou_Must_Add_Public_Vgui_Controls_Vgui_ControlsCpp_To_Your_Project = 1;

  // If you hit this error, then you need to include memoverride.cc in the
  // project somewhere or else you'll get crashes later when vgui_controls
  // allocates KeyValues and vgui tries to delete them.
#if !defined(NO_MALLOC_OVERRIDE)
  if (_heapmin() != 1) {
    Assert(false);
    Error("Must include memoverride.cc in your project.");
  }
#endif
  // keep a record of this module name
  strcpy_s(g_szControlsModuleName, moduleName);

  // initialize our locale (must be done for every vgui dll/exe)
  // "" makes it use the default locale, required to make iswprint() work
  // correctly in different languages
  setlocale(LC_CTYPE, "");
  setlocale(LC_TIME, "");
  setlocale(LC_COLLATE, "");
  setlocale(LC_MONETARY, "");

  // NOTE: Vgui expects to use these interfaces which are defined in tier3.lib
  if (!g_pVGui || !g_pVGuiInput || !g_pVGuiPanel || !g_pVGuiSurface ||
      !g_pVGuiSchemeManager || !g_pVGuiSystem) {
    Warning("vgui_controls is missing a required interface!\n");
    return false;
  }

  return true;
}

// Purpose: returns the name of the module this has been compiled into
const char *GetControlsModuleName() { return g_szControlsModuleName; }

}  // namespace vgui
