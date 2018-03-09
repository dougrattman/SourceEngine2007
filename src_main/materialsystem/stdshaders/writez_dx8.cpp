// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $


#include "BaseVSShader.h"

#include "writez.inc"

 
#include "tier0/include/memdbgon.h"

DEFINE_FALLBACK_SHADER( WriteZ, WriteZ_DX8 )

BEGIN_VS_SHADER_FLAGS( WriteZ_DX8, "Help for WriteZ", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
	}

	SHADER_FALLBACK
	{
		if ( g_pHardwareConfig->GetDXSupportLevel() < 80 )
			return "WriteZ_DX6";

		return 0;
	}

	SHADER_INIT
	{
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableColorWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			
			writez_Static_Index vshIndex;
			pShaderShadow->SetVertexShader( "writez", vshIndex.GetIndex() );

			pShaderShadow->SetPixelShader( "white" );
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION, 1, 0, 0 );
		}
		DYNAMIC_STATE
		{
			writez_Dynamic_Index vshIndex;
			vshIndex.SetDOWATERFOG( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z );
			pShaderAPI->SetVertexShaderIndex( vshIndex.GetIndex() );
		}
		Draw();
	}
END_SHADER

