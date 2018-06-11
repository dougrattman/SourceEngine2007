// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_TEXTUREDX8_H_
#define MATERIALSYSTEM_SHADERAPIDX9_TEXTUREDX8_H_

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"
#include "locald3dtypes.h"
#include "shaderapi/ishaderapi.h"

class CPixelWriter;

// Texture creation
IDirect3DBaseTexture *CreateD3DTexture(int width, int height, int depth,
                                       ImageFormat dstFormat, int numLevels,
                                       int creationFlags);

// Texture destruction
void DestroyD3DTexture(IDirect3DBaseTexture *pTex);

unsigned long GetD3DTextureRefCount(IDirect3DBaseTexture *pTex);

// Stats...
int TextureCount();

// Info for texture loading
struct TextureLoadInfo_t {
  ShaderAPITextureHandle_t m_TextureHandle;
  int m_nCopy;
  IDirect3DBaseTexture *m_pTexture;
  int m_nLevel;
  D3DCUBEMAP_FACES m_CubeFaceID;
  int m_nWidth;
  int m_nHeight;
  int m_nZOffset;  // What z-slice of the volume texture are we loading?
  ImageFormat m_SrcFormat;
  u8 *m_pSrcData;
};

// Texture image upload
void LoadTexture(TextureLoadInfo_t &info);

// Upload to a sub-piece of a texture
void LoadSubTexture(TextureLoadInfo_t &info, int xOffset, int yOffset,
                    int srcStride);

// Lock, unlock a texture...
bool LockTexture(ShaderAPITextureHandle_t textureHandle, int copy,
                 IDirect3DBaseTexture *pTexture, int level,
                 D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset,
                 int width, int height, bool bDiscard, CPixelWriter &writer);

bool UnlockTexture(ShaderAPITextureHandle_t textureHandle, int copy,
                   IDirect3DBaseTexture *pTexture, int level,
                   D3DCUBEMAP_FACES cubeFaceID);

#endif  // MATERIALSYSTEM_SHADERAPIDX9_TEXTUREDX8_H_
