// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "TextureDX8.h"

#include "ColorFormatDX8.h"
#include "ShaderAPIDX8_Global.h"
#include "UtlVector.h"
#include "base/include/windows/com_ptr.h"
#include "locald3dtypes.h"
#include "materialsystem/IMaterialSystem.h"
#include "recording.h"
#include "shaderapi/IShaderUtil.h"
#include "shaderapi/ishaderAPI.h"
#include "textureheap.h"
#include "tier0/include/vprof.h"
#include "tier1/utlbuffer.h"

#include "tier0/include/memdbgon.h"

static int s_TextureCount = 0;

// Stats...

int TextureCount() { return s_TextureCount; }

static bool IsVolumeTexture(IDirect3DBaseTexture *texture) {
  return texture && texture->GetType() == D3DRTYPE_VOLUMETEXTURE;
}

static HRESULT GetLevelDesc(IDirect3DBaseTexture *texture, UINT level,
                            D3DSURFACE_DESC *pDesc) {
  MEM_ALLOC_D3D_CREDIT();

  if (texture) {
    switch (texture->GetType()) {
      case D3DRTYPE_TEXTURE:
        return ((IDirect3DTexture *)texture)->GetLevelDesc(level, pDesc);

      case D3DRTYPE_CUBETEXTURE:
        return ((IDirect3DCubeTexture *)texture)->GetLevelDesc(level, pDesc);
    }
  }

  return E_INVALIDARG;
}

static HRESULT GetSurfaceFromTexture(IDirect3DBaseTexture *texture, UINT level,
                                     D3DCUBEMAP_FACES cubeFaceID,
                                     IDirect3DSurface **surface) {
  MEM_ALLOC_D3D_CREDIT();

  if (texture) {
    switch (texture->GetType()) {
      case D3DRTYPE_TEXTURE:
        return ((IDirect3DTexture *)texture)->GetSurfaceLevel(level, surface);

      case D3DRTYPE_CUBETEXTURE:
        return ((IDirect3DCubeTexture *)texture)
            ->GetCubeMapSurface(cubeFaceID, level, surface);

      default:
        Assert(0);
        return E_INVALIDARG;
    }
  }

  return E_POINTER;
}

// Gets the image format of a texture
static ImageFormat GetImageFormat(IDirect3DBaseTexture *texture) {
  MEM_ALLOC_D3D_CREDIT();

  if (texture) {
    if (!IsVolumeTexture(texture)) {
      D3DSURFACE_DESC desc;
      HRESULT hr = GetLevelDesc(texture, 0, &desc);

      if (SUCCEEDED(hr))
        return ImageLoader::D3DFormatToImageFormat(desc.Format);
    } else {
      D3DVOLUME_DESC desc;
      IDirect3DVolumeTexture *volume_texture =
          static_cast<IDirect3DVolumeTexture *>(texture);
      HRESULT hr = volume_texture->GetLevelDesc(0, &desc);

      if (SUCCEEDED(hr))
        return ImageLoader::D3DFormatToImageFormat(desc.Format);
    }
  }

  // Bogus baby!
  return (ImageFormat)-1;
}

// Allocates the D3DTexture
IDirect3DBaseTexture *CreateD3DTexture(int width, int height, int nDepth,
                                       ImageFormat dstFormat, int numLevels,
                                       int creation_flags) {
  if (nDepth <= 0) nDepth = 1;

  bool isCubeMap = (creation_flags & TEXTURE_CREATE_CUBEMAP) != 0;
  bool isRenderTarget = (creation_flags & TEXTURE_CREATE_RENDERTARGET) != 0;
  bool isManaged = (creation_flags & TEXTURE_CREATE_MANAGED) != 0;
  bool isDynamic = (creation_flags & TEXTURE_CREATE_DYNAMIC) != 0;
  bool isAutoMipMap = (creation_flags & TEXTURE_CREATE_AUTOMIPMAP) != 0;
  bool isVertexTexture = (creation_flags & TEXTURE_CREATE_VERTEXTEXTURE) != 0;
  bool isAllowNonFilterable =
      (creation_flags & TEXTURE_CREATE_UNFILTERABLE_OK) != 0;
  bool isVolumeTexture = (nDepth > 1);

  // NOTE: This function shouldn't be used for creating depth buffers!
  Assert(!(creation_flags & TEXTURE_CREATE_DEPTHBUFFER) != 0);

  D3DFORMAT d3dFormat =
      ImageLoader::ImageFormatToD3DFormat(FindNearestSupportedFormat(
          dstFormat, isVertexTexture, isRenderTarget, isAllowNonFilterable));

  if (d3dFormat == D3DFMT_UNKNOWN) {
    Warning("ShaderAPIDX8::CreateD3DTexture: Invalid color format!\n");
    Assert(0);
    return 0;
  }

  DWORD usage = isRenderTarget ? D3DUSAGE_RENDERTARGET : 0;
  if (isDynamic) {
    usage |= D3DUSAGE_DYNAMIC;
  }
  if (isAutoMipMap) {
    usage |= D3DUSAGE_AUTOGENMIPMAP;
  }

  IDirect3DBaseTexture *base_texture = NULL;
  HRESULT hr = S_OK;
  if (isCubeMap) {
    IDirect3DCubeTexture *pD3DCubeTexture = NULL;

    hr = Dx9Device()->CreateCubeTexture(
        width, numLevels, usage, d3dFormat,
        isManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, &pD3DCubeTexture, NULL);
    base_texture = pD3DCubeTexture;
  } else if (isVolumeTexture) {
    IDirect3DVolumeTexture *pD3DVolumeTexture = NULL;

    hr = Dx9Device()->CreateVolumeTexture(
        width, height, nDepth, numLevels, usage, d3dFormat,
        isManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, &pD3DVolumeTexture,
        NULL);
    base_texture = pD3DVolumeTexture;
  } else {
    // Override usage and managed params if using special hardware shadow depth
    // map formats...
    if ((d3dFormat == NVFMT_RAWZ) || (d3dFormat == NVFMT_INTZ) ||
        (d3dFormat == D3DFMT_D16) || (d3dFormat == D3DFMT_D24S8) ||
        (d3dFormat == ATIFMT_D16) || (d3dFormat == ATIFMT_D24S8)) {
      // Not putting D3DUSAGE_RENDERTARGET here causes D3D debug spew later, but
      // putting the flag causes this create to fail...
      usage = D3DUSAGE_DEPTHSTENCIL;
      isManaged = false;
    }

    // Override managed param if using special 0 texture format
    if (d3dFormat == NVFMT_NULL) {
      isManaged = false;
    }

    IDirect3DTexture *pD3DTexture = NULL;
    hr = Dx9Device()->CreateTexture(
        width, height, numLevels, usage, d3dFormat,
        isManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, &pD3DTexture, NULL);

    base_texture = pD3DTexture;
  }

  if (FAILED(hr)) {
    switch (hr) {
      case D3DERR_INVALIDCALL:
        Warning("ShaderAPIDX8::CreateD3DTexture: D3DERR_INVALIDCALL\n");
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        // This conditional is here so that we don't complain when testing
        // how much video memory we have. . this is kinda gross.
        if (isManaged) {
          Warning("ShaderAPIDX8::CreateD3DTexture: D3DERR_OUTOFVIDEOMEMORY\n");
        }
        break;
      case E_OUTOFMEMORY:
        Warning("ShaderAPIDX8::CreateD3DTexture: E_OUTOFMEMORY\n");
        break;
      default:
        break;
    }

    return nullptr;
  }

#ifdef MEASURE_DRIVER_ALLOCATIONS
  int nMipCount = numLevels;
  if (!nMipCount) {
    while (width > 1 || height > 1) {
      width >>= 1;
      height >>= 1;
      ++nMipCount;
    }
  }

  int nMemUsed = nMipCount * 1.1f * 1024;
  if (isCubeMap) {
    nMemUsed *= 6;
  }

  VPROF_INCREMENT_GROUP_COUNTER("texture count", COUNTER_GROUP_NO_RESET, 1);
  VPROF_INCREMENT_GROUP_COUNTER("texture driver mem", COUNTER_GROUP_NO_RESET,
                                nMemUsed);
  VPROF_INCREMENT_GROUP_COUNTER("total driver mem", COUNTER_GROUP_NO_RESET,
                                nMemUsed);
#endif

  ++s_TextureCount;

  return base_texture;
}

//-----------------------------------------------------------------------------
// Texture destruction
//-----------------------------------------------------------------------------
void DestroyD3DTexture(IDirect3DBaseTexture *pD3DTex) {
  if (pD3DTex) {
#ifdef MEASURE_DRIVER_ALLOCATIONS
    D3DRESOURCETYPE type = pD3DTex->GetType();
    int nMipCount = pD3DTex->GetLevelCount();
    if (type == D3DRTYPE_CUBETEXTURE) {
      nMipCount *= 6;
    }
    int nMemUsed = nMipCount * 1.1f * 1024;
    VPROF_INCREMENT_GROUP_COUNTER("texture count", COUNTER_GROUP_NO_RESET, -1);
    VPROF_INCREMENT_GROUP_COUNTER("texture driver mem", COUNTER_GROUP_NO_RESET,
                                  -nMemUsed);
    VPROF_INCREMENT_GROUP_COUNTER("total driver mem", COUNTER_GROUP_NO_RESET,
                                  -nMemUsed);
#endif

#ifndef NDEBUG
    ULONG ref =
#endif
        pD3DTex->Release();

#ifndef NDEBUG
    Assert(ref == 0);
#endif

    --s_TextureCount;
  }
}

unsigned long GetD3DTextureRefCount(IDirect3DBaseTexture *texture) {
  if (texture) {
    texture->AddRef();
    return texture->Release();
  }

  return 0UL;
}

// Lock, unlock a texture...

static RECT s_LockedSrcRect;
static D3DLOCKED_RECT s_LockedRect;
#ifdef _DEBUG
static bool s_bInLock = false;
#endif

bool LockTexture(ShaderAPITextureHandle_t bindId, int copy,
                 IDirect3DBaseTexture *base_texture, int level,
                 D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset,
                 int width, int height, bool bDiscard, CPixelWriter &writer) {
  Assert(!s_bInLock);

  source::windows::com_ptr<IDirect3DSurface> surface;
  HRESULT hr = GetSurfaceFromTexture(base_texture, level, cubeFaceID, &surface);

  if (SUCCEEDED(hr)) {
    s_LockedSrcRect.left = xOffset;
    s_LockedSrcRect.right = xOffset + width;
    s_LockedSrcRect.top = yOffset;
    s_LockedSrcRect.bottom = yOffset + height;

    const DWORD flags = D3DLOCK_NOSYSLOCK | (bDiscard ? D3DLOCK_DISCARD : 0);

    RECORD_COMMAND(DX8_LOCK_TEXTURE, 6);
    RECORD_INT(bindId);
    RECORD_INT(copy);
    RECORD_INT(level);
    RECORD_INT(cubeFaceID);
    RECORD_STRUCT(&s_LockedSrcRect, sizeof(s_LockedSrcRect));
    RECORD_INT(flags);

    hr = surface->LockRect(&s_LockedRect, &s_LockedSrcRect, flags);
  }

  if (SUCCEEDED(hr)) {
    writer.SetPixelMemory(GetImageFormat(base_texture), s_LockedRect.pBits,
                          s_LockedRect.Pitch);

#ifndef NDEBUG
    s_bInLock = true;
#endif

    return true;
  }

  return false;
}

bool UnlockTexture(ShaderAPITextureHandle_t bindId, int copy,
                   IDirect3DBaseTexture *texture, int level,
                   D3DCUBEMAP_FACES cubeFaceID) {
  Assert(s_bInLock);

  source::windows::com_ptr<IDirect3DSurface> surface;
  HRESULT hr = GetSurfaceFromTexture(texture, level, cubeFaceID, &surface);

  if (SUCCEEDED(hr)) {
#ifdef RECORD_TEXTURES
    int width = s_LockedSrcRect.right - s_LockedSrcRect.left;
    int height = s_LockedSrcRect.bottom - s_LockedSrcRect.top;
    int imageFormatSize = ImageLoader::SizeInBytes(GetImageFormat(pTexture));
    Assert(imageFormatSize != 0);
    int validDataBytesPerRow = imageFormatSize * width;
    int storeSize = validDataBytesPerRow * height;
    static CUtlVector<u8> tmpMem;
    if (tmpMem.Size() < storeSize) {
      tmpMem.AddMultipleToTail(storeSize - tmpMem.Size());
    }
    u8 *pDst = tmpMem.Base();
    u8 *pSrc = (u8 *)s_LockedRect.pBits;
    RECORD_COMMAND(DX8_SET_TEXTURE_DATA, 3);
    RECORD_INT(validDataBytesPerRow);
    RECORD_INT(height);
    int i;
    for (i = 0; i < height; i++) {
      memcpy(pDst, pSrc, validDataBytesPerRow);
      pDst += validDataBytesPerRow;
      pSrc += s_LockedRect.Pitch;
    }
    RECORD_STRUCT(tmpMem.Base(), storeSize);
#endif  // RECORD_TEXTURES

    RECORD_COMMAND(DX8_UNLOCK_TEXTURE, 4);
    RECORD_INT(bindId);
    RECORD_INT(copy);
    RECORD_INT(level);
    RECORD_INT(cubeFaceID);

    hr = surface->UnlockRect();

#ifndef NDEBUG
    s_bInLock = false;
#endif
  }

  return SUCCEEDED(hr);
}

// Compute texture size based on compression

static constexpr inline int DetermineGreaterPowerOfTwo(int val) {
  int num = 1;
  while (val > num) {
    num <<= 1;
  }

  return num;
}

static constexpr inline int DeterminePowerOfTwo(int val) {
  int pow = 0;
  while ((val & 0x1) == 0x0) {
    val >>= 1;
    ++pow;
  }

  return pow;
}

// Blit in bits

// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
// OPTIMIZE??: could lock the texture directly instead of the surface in dx9.
static void BlitSurfaceBits(TextureLoadInfo_t &info, int xOffset, int yOffset,
                            int srcStride) {
  D3DLOCKED_RECT lockedRect;

  // Get the level of the texture we want to write into
  source::windows::com_ptr<IDirect3DSurface> surface;
  HRESULT hr = GetSurfaceFromTexture(info.m_pTexture, info.m_nLevel,
                                     info.m_CubeFaceID, &surface);

  if (SUCCEEDED(hr)) {
    RECT srcRect{xOffset, yOffset, xOffset + info.m_nWidth,
                 yOffset + info.m_nHeight};

#ifndef RECORD_TEXTURES
    RECORD_COMMAND(DX8_LOCK_TEXTURE, 6);
    RECORD_INT(info.m_TextureHandle);
    RECORD_INT(info.m_nCopy);
    RECORD_INT(info.m_nLevel);
    RECORD_INT(info.m_CubeFaceID);
    RECORD_STRUCT(&srcRect, sizeof(srcRect));
    RECORD_INT(D3DLOCK_NOSYSLOCK);
#endif

    // lock the region (could be the full surface or less)
    hr = surface->LockRect(&lockedRect, &srcRect, D3DLOCK_NOSYSLOCK);
  }

  if (SUCCEEDED(hr)) {
    // garymcthack : need to make a recording command for this.
    ImageFormat dstFormat = GetImageFormat(info.m_pTexture);
    u8 *pImage = (u8 *)lockedRect.pBits;

    ShaderUtil()->ConvertImageFormat(info.m_pSrcData, info.m_SrcFormat, pImage,
                                     dstFormat, info.m_nWidth, info.m_nHeight,
                                     srcStride, lockedRect.Pitch);

#ifndef RECORD_TEXTURES
    RECORD_COMMAND(DX8_UNLOCK_TEXTURE, 4);
    RECORD_INT(info.m_TextureHandle);
    RECORD_INT(info.m_nCopy);
    RECORD_INT(info.m_nLevel);
    RECORD_INT(info.m_CubeFaceID);
#endif

    hr = surface->UnlockRect();
  } else {
    Warning("CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect\n");
    return;
  }

  if (SUCCEEDED(hr)) {
    ;
  } else {
    Warning("CShaderAPIDX8::BlitTextureBits: couldn't unlock texture rect\n");
  }
}

// Blit in bits
static void BlitVolumeBits(TextureLoadInfo_t &info, int xOffset, int yOffset,
                           int srcStride) {
#ifndef RECORD_TEXTURES
  RECORD_COMMAND(DX8_LOCK_TEXTURE, 6);
  RECORD_INT(info.m_TextureHandle);
  RECORD_INT(info.m_nCopy);
  RECORD_INT(info.m_nLevel);
  RECORD_INT(info.m_CubeFaceID);
  RECORD_STRUCT(&srcRect, sizeof(srcRect));
  RECORD_INT(D3DLOCK_NOSYSLOCK);
#endif

  D3DBOX srcBox{xOffset,
                yOffset,
                xOffset + info.m_nWidth,
                yOffset + info.m_nHeight,
                info.m_nZOffset,
                info.m_nZOffset + 1};
  D3DLOCKED_BOX lockedBox;

  auto *volume_texture = static_cast<IDirect3DVolumeTexture *>(info.m_pTexture);
  HRESULT hr = volume_texture->LockBox(info.m_nLevel, &lockedBox, &srcBox,
                                       D3DLOCK_NOSYSLOCK);

  if (SUCCEEDED(hr)) {
    // garymcthack : need to make a recording command for this.
    ImageFormat dstFormat = GetImageFormat(info.m_pTexture);
    u8 *pImage = (u8 *)lockedBox.pBits;
    ShaderUtil()->ConvertImageFormat(info.m_pSrcData, info.m_SrcFormat, pImage,
                                     dstFormat, info.m_nWidth, info.m_nHeight,
                                     srcStride, lockedBox.RowPitch);

#ifndef RECORD_TEXTURES
    RECORD_COMMAND(DX8_UNLOCK_TEXTURE, 4);
    RECORD_INT(info.m_TextureHandle);
    RECORD_INT(info.m_nCopy);
    RECORD_INT(info.m_nLevel);
    RECORD_INT(info.m_CubeFaceID);

    hr = volume_texture->UnlockBox(info.m_nLevel);
#endif
  } else {
    Warning("BlitVolumeBits: couldn't lock volume texture rect (0x%8.8x)\n",
            hr);
    return;
  }

  if (SUCCEEDED(hr)) {
    ;
  } else {
    Warning("BlitVolumeBits: couldn't unlock volume texture rect (0x%8.8x)\n",
            hr);
  }
}

// TODO(d.rattman): How do I blit from D3DPOOL_SYSTEMMEM to D3DPOOL_MANAGED? I
// used to use CopyRects for this.  UpdateSurface doesn't work because it
// can't blit to anything besides D3DPOOL_DEFAULT.  We use this only in the
// case where we need to create a < 4x4 miplevel for a compressed texture.  We
// end up creating a 4x4 system memory texture, and blitting it into the
// proper miplevel. 6) LockRects should be used for copying between SYSTEMMEM
// and MANAGED.  For such a small copy, you'd avoid a significant amount of
// overhead from the old CopyRects code.  Ideally, you should just lock the
// bottom of MANAGED and generate your sub-4x4 data there.

// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
static void BlitTextureBits(TextureLoadInfo_t &info, int xOffset, int yOffset,
                            int srcStride) {
#ifdef RECORD_TEXTURES
  RECORD_COMMAND(DX8_BLIT_TEXTURE_BITS, 14);
  RECORD_INT(info.m_TextureHandle);
  RECORD_INT(info.m_nCopy);
  RECORD_INT(info.m_nLevel);
  RECORD_INT(info.m_CubeFaceID);
  RECORD_INT(xOffset);
  RECORD_INT(yOffset);
  RECORD_INT(info.m_nZOffset);
  RECORD_INT(info.m_nWidth);
  RECORD_INT(info.m_nHeight);
  RECORD_INT(info.m_SrcFormat);
  RECORD_INT(srcStride);
  RECORD_INT(GetImageFormat(info.m_pTexture));
  // strides are in bytes.
  int srcDataSize;
  if (srcStride == 0) {
    srcDataSize = ImageLoader::GetMemRequired(info.m_nWidth, info.m_nHeight, 1,
                                              info.m_SrcFormat, false);
  } else {
    srcDataSize = srcStride * info.m_nHeight;
  }
  RECORD_INT(srcDataSize);
  RECORD_STRUCT(info.m_pSrcData, srcDataSize);
#endif  // RECORD_TEXTURES

  if (!IsVolumeTexture(info.m_pTexture)) {
    Assert(info.m_nZOffset == 0);
    BlitSurfaceBits(info, xOffset, yOffset, srcStride);
  } else {
    BlitVolumeBits(info, xOffset, yOffset, srcStride);
  }
}

// Texture image upload
void LoadTexture(TextureLoadInfo_t &info) {
  MEM_ALLOC_D3D_CREDIT();

  Assert(info.m_pSrcData);
  Assert(info.m_pTexture);

#ifndef NDEBUG
  ImageFormat format = GetImageFormat(info.m_pTexture);
  Assert((format != -1) &&
         (format == FindNearestSupportedFormat(format, false, false, false)));
#endif

  // Copy in the bits...
  BlitTextureBits(info, 0, 0, 0);
}

// Upload to a sub-piece of a texture
void LoadSubTexture(TextureLoadInfo_t &info, int xOffset, int yOffset,
                    int srcStride) {
  Assert(info.m_pSrcData);
  Assert(info.m_pTexture);

#ifndef NDEBUG
  ImageFormat format = GetImageFormat(info.m_pTexture);
  Assert((format == FindNearestSupportedFormat(format, false, false, false)) &&
         (format != -1));
#endif

  // Copy in the bits...
  BlitTextureBits(info, xOffset, yOffset, srcStride);
}
