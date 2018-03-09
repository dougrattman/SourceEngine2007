// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Naively sets the depth buffer values without testing the old values
// and without writing to alpha or color

#include "shaderlib/CShader.h"

 
#include "tier0/include/memdbgon.h"

DEFINE_FALLBACK_SHADER(SetZ, SetZ_DX6)

BEGIN_SHADER_FLAGS(SetZ_DX6, "Help for SetZ_DX6", SHADER_NOT_EDITABLE)
BEGIN_SHADER_PARAMS
END_SHADER_PARAMS

SHADER_INIT_PARAMS() {}

SHADER_INIT {}

SHADER_DRAW {
  SHADOW_STATE {
    pShaderShadow->EnableColorWrites(false);
    pShaderShadow->EnableAlphaWrites(false);
    pShaderShadow->DepthFunc(SHADER_DEPTHFUNC_ALWAYS);

    pShaderShadow->DrawFlags(SHADER_DRAW_POSITION);
  }
  DYNAMIC_STATE {}
  Draw();
}
END_SHADER
