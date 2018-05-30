// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "materialsystem/MaterialSystemUtil.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "tier1/KeyValues.h"

#include "tier0/include/memdbgon.h"

// Little utility class to deal with material references
CMaterialReference::CMaterialReference(ch const *material_name,
                                       const ch *pTextureGroupName,
                                       bool bComplain)
    : m_pMaterial{nullptr} {
  if (material_name) {
    Assert(pTextureGroupName);
    Init(material_name, pTextureGroupName, bComplain);
  }
}

CMaterialReference::~CMaterialReference() { Shutdown(); }

// Attach to a material
void CMaterialReference::Init(ch const *material_name,
                              const ch *pTextureGroupName, bool bComplain) {
  IMaterial *pMaterial =
      materials->FindMaterial(material_name, pTextureGroupName, bComplain);
  Assert(pMaterial);
  Init(pMaterial);
}

void CMaterialReference::Init(const ch *material_name,
                              KeyValues *pVMTKeyValues) {
  // CreateMaterial has a refcount of 1
  Shutdown();
  m_pMaterial = materials->CreateMaterial(material_name, pVMTKeyValues);
}

void CMaterialReference::Init(const ch *material_name,
                              const ch *pTextureGroupName,
                              KeyValues *pVMTKeyValues) {
  IMaterial *pMaterial = materials->FindProceduralMaterial(
      material_name, pTextureGroupName, pVMTKeyValues);
  Assert(pMaterial);
  Init(pMaterial);
}

void CMaterialReference::Init(IMaterial *pMaterial) {
  if (m_pMaterial != pMaterial) {
    Shutdown();
    m_pMaterial = pMaterial;
    if (m_pMaterial) {
      m_pMaterial->IncrementReferenceCount();
    }
  }
}

void CMaterialReference::Init(CMaterialReference &ref) {
  if (m_pMaterial != ref.m_pMaterial) {
    Shutdown();
    m_pMaterial = ref.m_pMaterial;
    if (m_pMaterial) {
      m_pMaterial->IncrementReferenceCount();
    }
  }
}

// Detach from a material
void CMaterialReference::Shutdown() {
  if (m_pMaterial && materials) {
    m_pMaterial->DecrementReferenceCount();
    m_pMaterial = nullptr;
  }
}

// Little utility class to deal with texture references
CTextureReference::CTextureReference() : m_pTexture{nullptr} {}

CTextureReference::CTextureReference(const CTextureReference &ref) {
  m_pTexture = ref.m_pTexture;
  if (m_pTexture) {
    m_pTexture->IncrementReferenceCount();
  }
}

void CTextureReference::operator=(CTextureReference &ref) {
  m_pTexture = ref.m_pTexture;
  if (m_pTexture) {
    m_pTexture->IncrementReferenceCount();
  }
}

CTextureReference::~CTextureReference() {
  // Shutdown();
}

// Attach to a texture
void CTextureReference::Init(ch const *pTextureName,
                             const ch *pTextureGroupName, bool bComplain) {
  Shutdown();
  m_pTexture =
      materials->FindTexture(pTextureName, pTextureGroupName, bComplain);
  if (m_pTexture) {
    m_pTexture->IncrementReferenceCount();
  }
}

void CTextureReference::Init(ITexture *pTexture) {
  Shutdown();

  m_pTexture = pTexture;
  if (m_pTexture) {
    m_pTexture->IncrementReferenceCount();
  }
}

void CTextureReference::InitProceduralTexture(const ch *pTextureName,
                                              const ch *pTextureGroupName,
                                              int w, int h, ImageFormat fmt,
                                              int nFlags) {
  Shutdown();

  m_pTexture = materials->CreateProceduralTexture(
      pTextureName, pTextureGroupName, w, h, fmt, nFlags);
}

void CTextureReference::InitRenderTarget(int w, int h,
                                         RenderTargetSizeMode_t sizeMode,
                                         ImageFormat fmt,
                                         MaterialRenderTargetDepth_t depth,
                                         bool bHDR,
                                         ch *pStrOptionalName /* = nullptr */) {
  Shutdown();

  int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT;
  if (depth == MATERIAL_RT_DEPTH_ONLY) textureFlags |= TEXTUREFLAGS_POINTSAMPLE;

  int renderTargetFlags = bHDR ? CREATERENDERTARGETFLAGS_HDR : 0;

  // NOTE: Refcount returned by CreateRenderTargetTexture is 1
  m_pTexture = materials->CreateNamedRenderTargetTextureEx(
      pStrOptionalName, w, h, sizeMode, fmt, depth, textureFlags,
      renderTargetFlags);

  Assert(m_pTexture);
}

// Detach from a texture.
void CTextureReference::Shutdown(bool bDeleteIfUnReferenced) {
  if (m_pTexture && materials) {
    m_pTexture->DecrementReferenceCount();
    if (bDeleteIfUnReferenced) {
      m_pTexture->DeleteIfUnreferenced();
    }
    m_pTexture = nullptr;
  }
}
