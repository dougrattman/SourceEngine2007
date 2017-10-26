// Copyright � 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#ifndef VGUI_ROOTPANEL_DOD_H
#define VGUI_ROOTPANEL_DOD_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include "tier1/UtlVector.h"


class CPanelEffect;


// Serial under of effect, for safe lookup
typedef unsigned int EFFECT_HANDLE;

//-----------------------------------------------------------------------------
// Purpose: Sits between engine and client .dll panels
//  Responsible for drawing screen overlays
//-----------------------------------------------------------------------------
class C_DODRootPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
  C_DODRootPanel( vgui::VPANEL parent );
	virtual 	~C_DODRootPanel( void );

	// Draw Panel effects here
	virtual void		PostChildPaint();

	// Clear list of Panel Effects
	virtual void		LevelInit( void );
	virtual void		LevelShutdown( void );

	// Run effects and let them decide whether to remove themselves
	void 	OnTick( void );

private:

	// Render all panel effects
	void		RenderPanelEffects( void );

	// List of current panel effects
	CUtlVector< CPanelEffect *> m_Effects;
};


#endif // VGUI_ROOTPANEL_DOD_H
