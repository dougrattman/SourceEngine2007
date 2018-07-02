// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "toolutils/toolswitchmenubutton.h"

#include "tier1/keyvalues.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/toolmenubutton.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/menu.h"
#include "vgui_controls/panel.h"

#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Menu to switch between tools
//-----------------------------------------------------------------------------
class CToolHelpMenuButton : public CToolMenuButton {
  DECLARE_CLASS_SIMPLE(CToolHelpMenuButton, CToolMenuButton);

 public:
  CToolHelpMenuButton(char const *toolName, char const *helpBinding,
                      vgui::Panel *parent, const char *panelName,
                      const char *text, vgui::Panel *pActionTarget);
};

//-----------------------------------------------------------------------------
// Global function to create the help menu
//-----------------------------------------------------------------------------
CToolMenuButton *CreateToolHelpMenuButton(
    char const *toolName, char const *helpBinding, vgui::Panel *parent,
    const char *panelName, const char *text, vgui::Panel *pActionTarget) {
  return new CToolHelpMenuButton(toolName, helpBinding, parent, panelName, text,
                                 pActionTarget);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolHelpMenuButton::CToolHelpMenuButton(
    char const *toolName, char const *helpBinding, vgui::Panel *parent,
    const char *panelName, const char *text, vgui::Panel *pActionTarget)
    : BaseClass(parent, panelName, text, pActionTarget) {
  wchar_t *fmt = g_pVGuiLocalize->Find("ToolHelpShowHelp");
  if (fmt) {
    wchar_t desc[256];
    g_pVGuiLocalize->ConvertANSIToUnicode(toolName, desc, sizeof(desc));

    wchar_t buf[512];
    g_pVGuiLocalize->ConstructString(buf, sizeof(buf), fmt, 1, desc);

    AddMenuItem("help", buf, new KeyValues("OnHelp"), pActionTarget, NULL,
                helpBinding);
  }

  SetMenu(m_pMenu);
}
