// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "shaderlib/CShader.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

DEFINE_FALLBACK_SHADER(WriteZ, WriteZ_DX6)

BEGIN_SHADER_FLAGS(WriteZ_DX6, "Help for WriteZ_DX6", SHADER_NOT_EDITABLE)
BEGIN_SHADER_PARAMS
END_SHADER_PARAMS

SHADER_INIT_PARAMS() {}

SHADER_INIT {}

SHADER_DRAW {
  SHADOW_STATE {
    pShaderShadow->EnableColorWrites(false);
    pShaderShadow->EnableAlphaWrites(false);
    pShaderShadow->DrawFlags(SHADER_DRAW_POSITION);
  }
  DYNAMIC_STATE {}
  Draw();
}
END_SHADER
