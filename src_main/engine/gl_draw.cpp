// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "render_pch.h"

#include "gl_drawlights.h"

#include "cdll_int.h"
#include "decal.h"
#include "draw.h"
#include "gl_cvars.h"
#include "gl_matsysiface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "screen.h"
#include "view.h"

 
#include "tier0/include/memdbgon.h"

Vector g_CurrentViewOrigin(0, 0, 0), g_CurrentViewForward(1, 0, 0),
    g_CurrentViewRight(0, -1, 0), g_CurrentViewUp(0, 0, 1);
Vector g_MainViewOrigin(0, 0, 0), g_MainViewForward(1, 0, 0),
    g_MainViewRight(0, -1, 0), g_MainViewUp(0, 0, 1);

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pMaterial -
//-----------------------------------------------------------------------------
void GL_UnloadMaterial(IMaterial *pMaterial) {
  if (pMaterial) {
    pMaterial->DecrementReferenceCount();
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pName -
// Output : IMaterial
//-----------------------------------------------------------------------------
static IMaterial *GL_LoadMaterialNoRef(const char *pName,
                                       const char *pTextureGroupName) {
  IMaterial *material = NULL;

  if (mat_loadtextures.GetInt()) {
    material = materials->FindMaterial(pName, pTextureGroupName);
  } else {
    material = g_materialEmpty;
  }

  return material;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pName -
// Output : IMaterial
//-----------------------------------------------------------------------------
IMaterial *GL_LoadMaterial(const char *pName, const char *pTextureGroupName) {
  IMaterial *material;

  material = GL_LoadMaterialNoRef(pName, pTextureGroupName);
  if (material) {
    material->IncrementReferenceCount();
  }
  return material;
}
