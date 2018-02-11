// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef VGUI_BUDGETPANEL_H
#define VGUI_BUDGETPANEL_H

#include "tier0/include/vprof.h"
#include "vgui/vgui_budgetpanelshared.h"

#define NUM_BUDGET_FPS_LABELS 3

// Use the shared budget panel between the engine and dedicated server.
class CBudgetPanelEngine : public CBudgetPanelShared {
  typedef CBudgetPanelShared BaseClass;

 public:
  CBudgetPanelEngine(vgui::Panel *pParent, const char *pElementName);
  ~CBudgetPanelEngine();

  virtual void SetTimeLabelText();
  virtual void SetHistoryLabelText();

  virtual void PostChildPaint();
  virtual void OnTick(void);

  // Command handlers
  void UserCmd_ShowBudgetPanel(void);
  void UserCmd_HideBudgetPanel(void);

  bool IsBudgetPanelShown() const;

 private:
  bool m_bShowBudgetPanelHeld;
};

CBudgetPanelEngine *GetBudgetPanel(void);

#endif  // VGUI_BUDGETPANEL_H
