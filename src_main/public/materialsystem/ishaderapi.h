// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: NOTE: This file is for backward compat!
// We'll get rid of it soon. Most of the contents of this file were moved
// into shaderpi/ishadershadow.h, shaderapi/ishaderdynamic.h, or
// shaderapi/shareddefs.h

#ifndef ISHADERAPI_MS_H
#define ISHADERAPI_MS_H

#include <shaderapi/ishaderdynamic.h>
#include <shaderapi/ishadershadow.h>
#include <shaderapi/shareddefs.h>

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IMaterialVar;

//-----------------------------------------------------------------------------
// Methods that can be called from the SHADER_INIT blocks of shaders
//-----------------------------------------------------------------------------
the_interface IShaderInit {
 public:
  // Loads up a texture
  virtual void LoadTexture(IMaterialVar * pTextureVar,
                           const char *pTextureGroupName) = 0;
  virtual void LoadBumpMap(IMaterialVar * pTextureVar,
                           const char *pTextureGroupName) = 0;
  virtual void LoadCubeMap(IMaterialVar * *ppParams,
                           IMaterialVar * pTextureVar) = 0;
};

#endif  // ISHADERAPI_MS_H
