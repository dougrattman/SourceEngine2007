// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "vgui_int.h"
#include "ienginevgui.h"
#include "c_csrootpanel.h"

 
#include "tier0/include/memdbgon.h"

C_CSRootPanel *g_pCSRootPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateClientDLLRootPanel( void )
{
	g_pCSRootPanel = new C_CSRootPanel( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyClientDLLRootPanel( void )
{
	delete g_pCSRootPanel;
	g_pCSRootPanel = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetClientDLLRootPanel( void )
{
	return g_pCSRootPanel->GetVPanel();
}