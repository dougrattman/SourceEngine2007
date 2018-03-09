// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

 
#include "tier0/include/memdbgon.h"

BEGIN_VS_SHADER( Detail, "Help for Detail" )

	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
	}

	SHADER_FALLBACK
	{
		return "UnlitGeneric";
	}

	SHADER_INIT
	{
	}

	SHADER_DRAW
	{
	}

END_SHADER

