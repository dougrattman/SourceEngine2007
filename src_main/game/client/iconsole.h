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

#if !defined( ICONSOLE_H )
#define ICONSOLE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

the_interface IConsole
{
public:
	virtual void		Create( vgui::VPANEL parent ) = 0;
	virtual void		Destroy( void ) = 0;
};

extern IConsole *console;

#endif // ICONSOLE_H