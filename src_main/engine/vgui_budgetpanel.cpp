// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Does anyone ever read this?

#include "client_pch.h"

#include "vgui_budgetpanel.h"

#include "IVRenderView.h"
#include "cmd.h"
#include "engine/ivmodelinfo.h"
#include "gl_cvars.h"
#include "gl_matsysiface.h"
#include "ivprofexport.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "mathlib/mathlib.h"
#include "tier0/include/fasttimer.h"
#include "tier0/include/vprof.h"
#include "tier1/convar.h"
#include "tier2/tier2.h"
#include "vgui/ischeme.h"
#include "vgui/ivgui.h"
#include "vgui_BudgetPanel.h"
#include "vgui_baseui_interface.h"
#include "vgui_controls/label.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "vprof_engine.h"
#include "vprof_record.h"

 
#include "tier0/include/memdbgon.h"

#ifdef VPROF_ENABLED

CON_COMMAND(vprof_adddebuggroup1,
            "add a new budget group dynamically for debugging") {
  VPROF_BUDGET("vprof_adddebuggroup1", "vprof_adddebuggroup1");
}

void IN_BudgetDown() { GetBudgetPanel()->UserCmd_ShowBudgetPanel(); }

void IN_BudgetUp() { GetBudgetPanel()->UserCmd_HideBudgetPanel(); }

static ConCommand startshowbudget("+showbudget", IN_BudgetDown, "",
                                  FCVAR_CHEAT);
static ConCommand endshowbudget("-showbudget", IN_BudgetUp, "", FCVAR_CHEAT);

// Globals.
static CBudgetPanelEngine *g_pBudgetPanel = NULL;

CBudgetPanelEngine *GetBudgetPanel() { return g_pBudgetPanel; }

CBudgetPanelEngine::CBudgetPanelEngine(vgui::Panel *pParent,
                                       const char *pElementName)
    : BaseClass(pParent, pElementName,
                BUDGETFLAG_CLIENT | BUDGETFLAG_OTHER | BUDGETFLAG_HIDDEN) {
  g_pBudgetPanel = this;
  m_bShowBudgetPanelHeld = false;
}

CBudgetPanelEngine::~CBudgetPanelEngine() {
  Assert(g_pBudgetPanel == this);
  g_pBudgetPanel = NULL;
}

void CBudgetPanelEngine::PostChildPaint() {
  int r = 255;
  int g = 0;

  int nDXSupportLevel = g_pMaterialSystemHardwareConfig->GetDXSupportLevel();
  if ((g_fFrameRate >= 60) || (nDXSupportLevel <= 80 && g_fFrameRate >= 30) ||
      (nDXSupportLevel <= 70 && g_fFrameRate >= 20)) {
    r = 0;
    g = 255;
  }

  int yPos = 20;
  g_pMatSystemSurface->DrawColoredText(
      m_hFont, 600, yPos, r, g, 0, 255,
      "%3i fps (showbudget 3D driver time included)",
      RoundFloatToInt(g_fFrameRate));
  yPos += 14;

  g_pMatSystemSurface->DrawColoredText(m_hFont, 600, yPos, r, g, 0, 255,
                                       "%5.1f ms",
                                       g_fFrameTimeLessBudget * 1000.0f);
  yPos += 14;
#ifndef _XBOX
  if (VProfRecord_IsPlayingBack()) {
    int iCur = VProfPlayback_GetCurrentTick();
    char str[512];
    Q_snprintf(str, sizeof(str), "VPROF playback (tick %d, %d%%)", iCur,
               (int)(VProfPlayback_GetCurrentPercent() * 100));
    g_pMatSystemSurface->DrawColoredText(m_hFont, 600, yPos, 255, 0, 0, 255,
                                         "%s", str);
    yPos += 14;
  }
#endif
  BaseClass::PostChildPaint();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBudgetPanelEngine::UserCmd_ShowBudgetPanel() {
  Cbuf_AddText("vprof_on\n");
  m_bShowBudgetPanelHeld = true;
  SetVisible(true);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBudgetPanelEngine::UserCmd_HideBudgetPanel() {
  Cbuf_AddText("vprof_off\n");
  m_bShowBudgetPanelHeld = false;
  SetVisible(false);
}

void CBudgetPanelEngine::OnTick() {
  // Go away if we were on and sv_cheats is now on.
  if (m_bShowBudgetPanelHeld && !CanCheat()) {
    UserCmd_HideBudgetPanel();
  }

  BaseClass::OnTick();
  SetVisible(m_bShowBudgetPanelHeld);
}

void CBudgetPanelEngine::SetTimeLabelText() {
  for (int i = 0; i < m_TimeLabels.Count(); i++) {
    char text[512];
    Q_snprintf(text, sizeof(text), "%dms",
               (int)(i * GetConfigData().m_flTimeLabelInterval));
    m_TimeLabels[i]->SetText(text);
  }
}

void CBudgetPanelEngine::SetHistoryLabelText() {
  Assert(m_HistoryLabels.Count() == 3);
  m_HistoryLabels[0]->SetText("20 fps (50 ms)");
  m_HistoryLabels[1]->SetText("30 fps (33 1/3 ms)");
  m_HistoryLabels[2]->SetText("60 fps (16 2/3 ms)");
}

bool CBudgetPanelEngine::IsBudgetPanelShown() const {
  return m_bShowBudgetPanelHeld;
}

#endif  // VPROF_ENABLED
