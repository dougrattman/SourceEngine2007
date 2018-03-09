// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: HL2 specific input handling
//
// $NoKeywords: $

#include "cbase.h"
#include "kbutton.h"
#include "input.h"

 
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: HL Input interface
//-----------------------------------------------------------------------------
class CHLInput : public CInput
{
public:
};

static CHLInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;
