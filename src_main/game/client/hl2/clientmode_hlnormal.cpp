// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Draws the normal TF2 or HL2 HUD.

#include "cbase.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "clientmode_hlnormal.h"
#include "hud.h"
#include "ienginevgui.h"
#include "iinput.h"
#include "vgui_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

extern bool g_bRollingCredits;

vgui::HScheme g_hVGuiCombineScheme = 0;

// Instance the singleton and expose the interface to it.
IClientMode *GetClientModeNormal() {
  static ClientModeHLNormal g_ClientModeNormal;
  return &g_ClientModeNormal;
}

// Purpose: this is the viewport that contains all the hud elements
class CHudViewport : public CBaseViewport {
 private:
  DECLARE_CLASS_SIMPLE(CHudViewport, CBaseViewport);

 protected:
  virtual void ApplySchemeSettings(vgui::IScheme *pScheme) {
    BaseClass::ApplySchemeSettings(pScheme);

    gHUD.InitColors(pScheme);

    SetPaintBackgroundEnabled(false);
  }

  virtual void CreateDefaultPanels(){/* don't create any panels yet*/};
};

ClientModeHLNormal::ClientModeHLNormal() {
  m_pViewport = new CHudViewport();
  m_pViewport->Start(gameuifuncs, gameeventmanager);
}

ClientModeHLNormal::~ClientModeHLNormal() {}

void ClientModeHLNormal::Init() {
  BaseClass::Init();

  // Load up the combine control panel scheme
  g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx(
      enginevgui->GetPanel(PANEL_CLIENTDLL), "resource/CombinePanelScheme.res",
      "CombineScheme");
  if (!g_hVGuiCombineScheme) {
    Warning("Couldn't load combine panel scheme!\n");
  }
}

bool ClientModeHLNormal::ShouldDrawCrosshair() {
  return g_bRollingCredits == false;
}
