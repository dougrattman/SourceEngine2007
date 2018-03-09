// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "shaderlib/CShader.h"

 
#include "tier0/include/memdbgon.h"

BEGIN_SHADER_FLAGS(ViewAlpha, "Help for ViewAlpha", SHADER_NOT_EDITABLE)

BEGIN_SHADER_PARAMS
END_SHADER_PARAMS

SHADER_INIT { LoadTexture(BASETEXTURE); }

SHADER_DRAW {
  SHADOW_STATE {
    pShaderShadow->EnableTexture(SHADER_SAMPLER0, true);
    pShaderShadow->EnableCustomPixelPipe(true);
    pShaderShadow->CustomTextureStages(1);
    pShaderShadow->CustomTextureOperation(
        SHADER_TEXTURE_STAGE0, SHADER_TEXCHANNEL_COLOR, SHADER_TEXOP_SELECTARG1,
        SHADER_TEXARG_TEXTUREALPHA, SHADER_TEXARG_CONSTANTCOLOR);
    pShaderShadow->CustomTextureOperation(
        SHADER_TEXTURE_STAGE0, SHADER_TEXCHANNEL_ALPHA, SHADER_TEXOP_SELECTARG1,
        SHADER_TEXARG_TEXTURE, SHADER_TEXARG_VERTEXCOLOR);

    pShaderShadow->DrawFlags(SHADER_DRAW_POSITION | SHADER_DRAW_TEXCOORD0);
  }
  DYNAMIC_STATE {
    BindTexture(SHADER_SAMPLER0, BASETEXTURE, FRAME);
    SetFixedFunctionTextureTransform(MATERIAL_TEXTURE0, BASETEXTURETRANSFORM);
  }
  Draw();
}
END_SHADER
