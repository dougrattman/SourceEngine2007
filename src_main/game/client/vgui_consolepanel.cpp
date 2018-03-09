// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $

#include "cbase.h"
#include "iconsole.h"

 
#include "tier0/include/memdbgon.h"

class CConPanel;

class CConsole : public IConsole
{
private:
	CConPanel *conPanel;
public:
	CConsole( void )
	{
		conPanel = NULL;
	}
	
	void Create( vgui::VPANEL parent )
	{
		/*
		conPanel = new CConPanel( parent );
		*/
	}

	void Destroy( void )
	{
		/*
		if ( conPanel )
		{
			conPanel->SetParent( (vgui::Panel *)NULL );
			delete conPanel;
		}
		*/
	}
};

static CConsole g_Console;
IConsole *console = ( IConsole * )&g_Console;
