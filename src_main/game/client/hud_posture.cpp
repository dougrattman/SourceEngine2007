// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "cbase.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVgui.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>
#include "hud.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "hudelement.h"
#include "iclientmode.h"

using namespace vgui;

 
#include "tier0/include/memdbgon.h"

#define HUD_POSTURE_UPDATES_PER_SECOND 10
#define HUD_POSTURE_FADE_TIME 0.4f
#define CROUCHING_CHARACTER_INDEX \
  92  // index of the crouching dude in the TTF font

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudPosture : public CHudElement, public vgui::Panel {
  DECLARE_CLASS_SIMPLE(CHudPosture, vgui::Panel);

 public:
  CHudPosture(const char *pElementName);
  bool ShouldDraw(void);
};

DECLARE_HUDELEMENT(CHudPosture);

namespace {
// Don't pass a 0 pPlayer. Doesn't check for it.
inline bool PlayerIsDucking(C_BasePlayer *pPlayer) {
  return pPlayer->m_Local.m_bDucked &&        // crouching
         pPlayer->GetGroundEntity() != NULL;  // but not jumping
}
}  // namespace

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudPosture::CHudPosture(const char *pElementName)
    : CHudElement(pElementName), BaseClass(NULL, "HudPosture") {
  vgui::Panel *pParent = g_pClientMode->GetViewport();
  SetParent(pParent);

  SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudPosture::ShouldDraw() { return false; }
