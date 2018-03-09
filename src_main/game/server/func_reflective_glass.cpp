// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "modelentities.h"

 
#include "tier0/include/memdbgon.h"

class CFuncReflectiveGlass : public CFuncBrush
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncReflectiveGlass, CFuncBrush );
	DECLARE_SERVERCLASS();
};

// automatically hooks in the system's callbacks
BEGIN_DATADESC( CFuncReflectiveGlass )
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_reflective_glass, CFuncReflectiveGlass );

IMPLEMENT_SERVERCLASS_ST( CFuncReflectiveGlass, DT_FuncReflectiveGlass )
END_SEND_TABLE()
