// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "CommandCheckButton.h"

#include "EngineInterface.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

CCommandCheckButton::CCommandCheckButton(Panel *parent, const char *panelName,
                                         const char *text, const char *downcmd,
                                         const char *upcmd)
    : CheckButton(parent, panelName, text) {
  m_pszDown = downcmd ? _strdup(downcmd) : NULL;
  m_pszUp = upcmd ? _strdup(upcmd) : NULL;
}

CCommandCheckButton::~CCommandCheckButton() {
  free(m_pszDown);
  free(m_pszUp);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *panel -
//-----------------------------------------------------------------------------
void CCommandCheckButton::SetSelected(bool state) {
  BaseClass::SetSelected(state);

  if (IsSelected() && m_pszDown) {
    engine->ClientCmd_Unrestricted(m_pszDown);
    engine->ClientCmd_Unrestricted("\n");
  } else if (!IsSelected() && m_pszUp) {
    engine->ClientCmd_Unrestricted(m_pszUp);
    engine->ClientCmd_Unrestricted("\n");
  }
}
