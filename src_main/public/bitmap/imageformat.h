// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IMAGEFORMAT_H
#define IMAGEFORMAT_H

#include <type_traits>
#include "base/include/base_types.h"
#include "build/include/build_config.h"

enum NormalDecodeMode_t {
  NORMAL_DECODE_NONE = 0,
  NORMAL_DECODE_ATI2N = 1,
  NORMAL_DECODE_ATI2N_ALPHA = 2
};

#ifdef OS_WIN
using D3DFORMAT = enum _D3DFORMAT : int;
#endif

// The various image format types

enum ImageFormat {
  IMAGE_FORMAT_UNKNOWN = -1,
  IMAGE_FORMAT_RGBA8888 = 0,
  IMAGE_FORMAT_ABGR8888,
  IMAGE_FORMAT_RGB888,
  IMAGE_FORMAT_BGR888,
  IMAGE_FORMAT_RGB565,
  IMAGE_FORMAT_I8,
  IMAGE_FORMAT_IA88,
  IMAGE_FORMAT_P8,
  IMAGE_FORMAT_A8,
  IMAGE_FORMAT_RGB888_BLUESCREEN,
  IMAGE_FORMAT_BGR888_BLUESCREEN,
  IMAGE_FORMAT_ARGB8888,
  IMAGE_FORMAT_BGRA8888,
  IMAGE_FORMAT_DXT1,
  IMAGE_FORMAT_DXT3,
  IMAGE_FORMAT_DXT5,
  IMAGE_FORMAT_BGRX8888,
  IMAGE_FORMAT_BGR565,
  IMAGE_FORMAT_BGRX5551,
  IMAGE_FORMAT_BGRA4444,
  IMAGE_FORMAT_DXT1_ONEBITALPHA,
  IMAGE_FORMAT_BGRA5551,
  IMAGE_FORMAT_UV88,
  IMAGE_FORMAT_UVWQ8888,
  IMAGE_FORMAT_RGBA16161616F,
  IMAGE_FORMAT_RGBA16161616,
  IMAGE_FORMAT_UVLX8888,
  IMAGE_FORMAT_R32F,  // Single-channel 32-bit floating point
  IMAGE_FORMAT_RGB323232F,
  IMAGE_FORMAT_RGBA32323232F,

  // Depth-stencil texture formats for shadow depth mapping
  IMAGE_FORMAT_NV_DST16,   //
  IMAGE_FORMAT_NV_DST24,   //
  IMAGE_FORMAT_NV_INTZ,    // Vendor-specific depth-stencil texture
  IMAGE_FORMAT_NV_RAWZ,    // formats for shadow depth mapping
  IMAGE_FORMAT_ATI_DST16,  //
  IMAGE_FORMAT_ATI_DST24,  //
  IMAGE_FORMAT_NV_NULL,    // Dummy format which takes no video memory

  // Compressed normal map formats
  IMAGE_FORMAT_ATI2N,  // One-surface ATI2N / DXN format
  IMAGE_FORMAT_ATI1N,  // Two-surface ATI1N format

  NUM_IMAGE_FORMATS
};

// Color structures

struct BGRA8888_t {
  u8 b;  // change the order of names to change the
  u8 g;  //  order of the output ARGB or BGRA, etc...
  u8 r;  //  Last one is MSB, 1st is LSB.
  u8 a;
  constexpr inline BGRA8888_t &operator=(const BGRA8888_t &in) {
    static_assert(std::is_standard_layout_v<BGRA8888_t>,
                  "Verify BGRA8888_t has standart layout.");
    static_assert(sizeof(u32) == sizeof(decltype(in)),
                  "Verify in is of same size as u32.");

    *(u32 *)(std::uintptr_t)this = *(u32 *)(std::uintptr_t)&in;
    return *this;
  }
};

struct RGBA8888_t {
  u8 r;  // change the order of names to change the
  u8 g;  //  order of the output ARGB or BGRA, etc...
  u8 b;  //  Last one is MSB, 1st is LSB.
  u8 a;
  constexpr inline RGBA8888_t &operator=(const BGRA8888_t &in) {
    r = in.r;
    g = in.g;
    b = in.b;
    a = in.a;
    return *this;
  }
};

struct RGB888_t {
  u8 r;
  u8 g;
  u8 b;
  constexpr inline RGB888_t &operator=(const BGRA8888_t &in) {
    r = in.r;
    g = in.g;
    b = in.b;
    return *this;
  }
  constexpr inline bool operator==(const RGB888_t &in) const {
    return (r == in.r) && (g == in.g) && (b == in.b);
  }
  constexpr inline bool operator!=(const RGB888_t &in) const {
    return (r != in.r) || (g != in.g) || (b != in.b);
  }
};

struct BGR888_t {
  u8 b;
  u8 g;
  u8 r;
  constexpr inline BGR888_t &operator=(const BGRA8888_t &in) {
    r = in.r;
    g = in.g;
    b = in.b;
    return *this;
  }
};

struct BGR565_t {
  u16 b : 5;  // order of names changes
  u16 g : 6;  // byte order of output to 32 bit
  u16 r : 5;
  constexpr inline BGR565_t &operator=(const BGRA8888_t &in) {
    r = in.r >> 3;
    g = in.g >> 2;
    b = in.b >> 3;
    return *this;
  }
  constexpr inline BGR565_t &Set(u32 red, u32 green, u32 blue) {
    r = red >> 3;
    g = green >> 2;
    b = blue >> 3;
    return *this;
  }
};

struct BGRA5551_t {
  u16 b : 5;  // order of names changes
  u16 g : 5;  // byte order of output to 32 bit
  u16 r : 5;
  u16 a : 1;
  constexpr inline BGRA5551_t &operator=(const BGRA8888_t &in) {
    r = in.r >> 3;
    g = in.g >> 3;
    b = in.b >> 3;
    a = in.a >> 7;
    return *this;
  }
};

struct BGRA4444_t {
  u16 b : 4;  // order of names changes
  u16 g : 4;  // byte order of output to 32 bit
  u16 r : 4;
  u16 a : 4;
  constexpr inline BGRA4444_t &operator=(const BGRA8888_t &in) {
    r = in.r >> 4;
    g = in.g >> 4;
    b = in.b >> 4;
    a = in.a >> 4;
    return *this;
  }
};

struct RGBX5551_t {
  u16 r : 5;
  u16 g : 5;
  u16 b : 5;
  u16 x : 1;
  constexpr inline RGBX5551_t &operator=(const BGRA8888_t &in) {
    r = in.r >> 3;
    g = in.g >> 3;
    b = in.b >> 3;
    return *this;
  }
};

// Some important constants
#define ARTWORK_GAMMA (2.2f)
#define IMAGE_MAX_DIM (2048)

// Information about each image format
struct ImageFormatInfo_t {
  const char *m_pName;
  int m_NumBytes;
  int m_NumRedBits;
  int m_NumGreeBits;
  int m_NumBlueBits;
  int m_NumAlphaBits;
  bool m_IsCompressed;
};

// Various methods related to pixelmaps and color formats.
namespace ImageLoader {
bool GetInfo(const char *fileName, int *width, int *height,
             enum ImageFormat *imageFormat, f32 *sourceGamma);
int GetMemRequired(int width, int height, int depth, ImageFormat imageFormat,
                   bool mipmap);
int GetMipMapLevelByteOffset(int width, int height,
                             enum ImageFormat imageFormat, int skipMipLevels);
void GetMipMapLevelDimensions(int *width, int *height, int skipMipLevels);
int GetNumMipMapLevels(int width, int height, int depth = 1);
bool Load(u8 *imageData, const char *fileName, int width, int height,
          enum ImageFormat imageFormat, f32 targetGamma, bool mipmap);
bool Load(u8 *imageData, FILE *fp, int width, int height,
          enum ImageFormat imageFormat, f32 targetGamma, bool mipmap);

// convert from any image format to any other image format.
// return false if the conversion cannot be performed.
// Strides denote the number of bytes per each line,
// by default assumes width * # of bytes per pixel
bool ConvertImageFormat(const u8 *src, enum ImageFormat srcImageFormat, u8 *dst,
                        enum ImageFormat dstImageFormat, int width, int height,
                        int srcStride = 0, int dstStride = 0);

// must be used in conjunction with ConvertImageFormat() to pre-swap and
// post-swap
void PreConvertSwapImageData(u8 *pImageData, int nImageSize,
                             ImageFormat imageFormat, int width = 0,
                             int stride = 0);
void PostConvertSwapImageData(u8 *pImageData, int nImageSize,
                              ImageFormat imageFormat, int width = 0,
                              int stride = 0);
void ByteSwapImageData(u8 *pImageData, int nImageSize, ImageFormat imageFormat,
                       int width = 0, int stride = 0);
bool IsFormatValidForConversion(ImageFormat fmt);

// convert back and forth from D3D format to ImageFormat, regardless of
// whether it's supported or not

#ifdef OS_WIN
ImageFormat D3DFormatToImageFormat(D3DFORMAT format);
D3DFORMAT ImageFormatToD3DFormat(ImageFormat format);
#endif

// Flags for ResampleRGBA8888
enum {
  RESAMPLE_NORMALMAP = 0x1,
  RESAMPLE_ALPHATEST = 0x2,
  RESAMPLE_NICE_FILTER = 0x4,
  RESAMPLE_CLAMPS = 0x8,
  RESAMPLE_CLAMPT = 0x10,
  RESAMPLE_CLAMPU = 0x20,
};

struct ResampleInfo_t {
  ResampleInfo_t()
      : m_nFlags(0),
        m_flAlphaThreshhold(0.4f),
        m_flAlphaHiFreqThreshhold(0.4f),
        m_nSrcDepth(1),
        m_nDestDepth(1) {
    m_flColorScale[0] = 1.0f, m_flColorScale[1] = 1.0f,
    m_flColorScale[2] = 1.0f, m_flColorScale[3] = 1.0f;
    m_flColorGoal[0] = 0.0f, m_flColorGoal[1] = 0.0f, m_flColorGoal[2] = 0.0f,
    m_flColorGoal[3] = 0.0f;
  }

  u8 *m_pSrc;
  u8 *m_pDest;

  int m_nSrcWidth;
  int m_nSrcHeight;
  int m_nSrcDepth;

  int m_nDestWidth;
  int m_nDestHeight;
  int m_nDestDepth;

  f32 m_flSrcGamma;
  f32 m_flDestGamma;

  f32 m_flColorScale[4];  // Color scale factors RGBA
  f32 m_flColorGoal[4];   // Color goal values RGBA		DestColor =
                          // ColorGoal
                          // + scale * (SrcColor - ColorGoal)

  f32 m_flAlphaThreshhold;
  f32 m_flAlphaHiFreqThreshhold;

  int m_nFlags;
};

bool ResampleRGBA8888(const ResampleInfo_t &info);
bool ResampleRGBA16161616(const ResampleInfo_t &info);
bool ResampleRGB323232F(const ResampleInfo_t &info);

void ConvertNormalMapRGBA8888ToDUDVMapUVLX8888(const u8 *src, int width,
                                               int height, u8 *dst_);
void ConvertNormalMapRGBA8888ToDUDVMapUVWQ8888(const u8 *src, int width,
                                               int height, u8 *dst_);
void ConvertNormalMapRGBA8888ToDUDVMapUV88(const u8 *src, int width, int height,
                                           u8 *dst_);

void ConvertIA88ImageToNormalMapRGBA8888(const u8 *src, int width, int height,
                                         u8 *dst, f32 bumpScale);

void NormalizeNormalMapRGBA8888(u8 *src, int numTexels);

// Gamma correction
void GammaCorrectRGBA8888(u8 *src, u8 *dst, int width, int height, int depth,
                          f32 srcGamma, f32 dstGamma);

// Makes a gamma table
void ConstructGammaTable(u8 *pTable, f32 srcGamma, f32 dstGamma);

// Gamma corrects using a previously constructed gamma table
void GammaCorrectRGBA8888(u8 *pSrc, u8 *pDst, int width, int height, int depth,
                          u8 *pGammaTable);

// Generates a number of mipmap levels
void GenerateMipmapLevels(u8 *pSrc, u8 *pDst, int width, int height, int depth,
                          ImageFormat imageFormat, f32 srcGamma, f32 dstGamma,
                          int numLevels = 0);

// operations on square images (src and dst can be the same)
bool RotateImageLeft(const u8 *src, u8 *dst, int widthHeight,
                     ImageFormat imageFormat);
bool RotateImage180(const u8 *src, u8 *dst, int widthHeight,
                    ImageFormat imageFormat);
bool FlipImageVertically(void *pSrc, void *pDst, int nWidth, int nHeight,
                         ImageFormat imageFormat, int nDstStride = 0);
bool FlipImageHorizontally(void *pSrc, void *pDst, int nWidth, int nHeight,
                           ImageFormat imageFormat, int nDstStride = 0);
bool SwapAxes(u8 *src, int widthHeight, ImageFormat imageFormat);

// Returns info about each image format
ImageFormatInfo_t const &ImageFormatInfo(ImageFormat fmt);

// Gets the name of the image format
inline char const *GetName(ImageFormat fmt) {
  return ImageFormatInfo(fmt).m_pName;
}

// Gets the size of the image format in bytes
inline int SizeInBytes(ImageFormat fmt) {
  return ImageFormatInfo(fmt).m_NumBytes;
}

// Does the image format support transparency?
inline bool IsTransparent(ImageFormat fmt) {
  return ImageFormatInfo(fmt).m_NumAlphaBits > 0;
}

// Is the image format compressed?
inline bool IsCompressed(ImageFormat fmt) {
  return ImageFormatInfo(fmt).m_IsCompressed;
}

// Is any channel > 8 bits?
inline bool HasChannelLargerThan8Bits(ImageFormat fmt) {
  ImageFormatInfo_t info = ImageFormatInfo(fmt);
  return (info.m_NumRedBits > 8 || info.m_NumGreeBits > 8 ||
          info.m_NumBlueBits > 8 || info.m_NumAlphaBits > 8);
}
}  // namespace ImageLoader

#endif  // IMAGEFORMAT_H
