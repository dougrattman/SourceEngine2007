// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef VGUI_BUDGETHISTORYPANEL_H
#define VGUI_BUDGETHISTORYPANEL_H

#include "vgui_controls/Frame.h"

class CBaseBudgetPanel;

class CBudgetHistoryPanel : public vgui::Panel {
  typedef vgui::Panel BaseClass;

 public:
  CBudgetHistoryPanel(CBaseBudgetPanel *pParent, const char *pPanelName);
  ~CBudgetHistoryPanel();
  void SetData(double *pData, int numCategories, int numItems,
               int offsetIntoData);
  void SetRange(float fMin, float fMax);

 protected:
  void Paint();

 private:
  void DrawBudgetLine(float val);

 private:
  CBaseBudgetPanel *m_pBudgetPanel;

  double *m_pData;
  int m_nGroups;
  int m_nSamplesPerGroup;
  int m_nSampleOffset;
  float m_fRangeMin;
  float m_fRangeMax;
};

#endif  // VGUI_BUDGETHISTORYPANEL_H
