// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEMUTIL_H
#define MATERIALSYSTEMUTIL_H

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"  // ImageFormat
#include "materialsystem/imaterialsystem.h"  // RenderTargetSizeMode_t and MaterialRenderTargetDepth_t

class IMaterial;
class ITexture;
class KeyValues;

// Little utility class to deal with material references.
class CMaterialReference {
 public:
  CMaterialReference(ch const *material_name = nullptr,
                     const ch *pTextureGroupName = nullptr,
                     bool bComplain = true);
  ~CMaterialReference();

  // Attach to a material
  void Init(const ch *material_name, const ch *pTextureGroupName,
            bool bComplain = true);
  void Init(const ch *material_name, KeyValues *pVMTKeyValues);
  void Init(IMaterial *pMaterial);
  void Init(CMaterialReference &ref);
  void Init(const ch *material_name, const ch *pTextureGroupName,
            KeyValues *pVMTKeyValues);

  // Detach from a material
  void Shutdown();
  bool IsValid() const { return m_pMaterial != nullptr; }

  // Automatic casts to IMaterial
  operator IMaterial *() { return m_pMaterial; }
  operator IMaterial const *() const { return m_pMaterial; }
  IMaterial *operator->() { return m_pMaterial; }

 private:
  IMaterial *m_pMaterial;
};

// Little utility class to deal with texture references
class CTextureReference {
 public:
  CTextureReference();
  CTextureReference(const CTextureReference &ref);
  ~CTextureReference();

  // Attach to a texture
  void Init(ch const *pTexture, const ch *pTextureGroupName,
            bool bComplain = true);
  void InitProceduralTexture(const ch *pTextureName,
                             const ch *pTextureGroupName, int w, int h,
                             ImageFormat fmt, int nFlags);
  void InitRenderTarget(int w, int h, RenderTargetSizeMode_t sizeMode,
                        ImageFormat fmt, MaterialRenderTargetDepth_t depth,
                        bool bHDR, ch *pStrOptionalName = nullptr);
  void Init(ITexture *pTexture);

  // Detach from a texture
  void Shutdown(bool bDeleteIfUnReferenced = false);
  bool IsValid() const { return m_pTexture != nullptr; }

  // Automatic casts to ITexture
  operator ITexture *() { return m_pTexture; }
  operator ITexture const *() const { return m_pTexture; }
  ITexture *operator->() { return m_pTexture; }

  // Assignment operator
  void operator=(CTextureReference &ref);

 private:
  ITexture *m_pTexture;
};

#endif  // !MATERIALSYSTEMUTIL_H
