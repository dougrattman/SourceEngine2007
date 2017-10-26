// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_VTF_S3TC_DECODE_H_
#define SOURCE_VTF_S3TC_DECODE_H_

#include <cstdint>
#include "bitmap/imageformat.h"

enum ImageFormat;

struct S3RGBA {
  uint8_t b, g, r, a;
};

struct S3PaletteIndex {
  uint8_t m_AlphaIndex, m_ColorIndex;
};

#define MAX_S3TC_BLOCK_BYTES 16

S3PaletteIndex S3TC_GetPixelPaletteIndex(ImageFormat format,
                                         const char *pS3Block, int x, int y);

void S3TC_SetPixelPaletteIndex(ImageFormat format, char *pS3Block, int x, int y,
                               S3PaletteIndex iPaletteIndex);

// Note: width, x, and y are in texels, not S3 blocks.
S3PaletteIndex S3TC_GetPaletteIndex(unsigned char *pFaceData,
                                    ImageFormat format, int imageWidth, int x,
                                    int y);

// Note: width, x, and y are in texels, not S3 blocks.
void S3TC_SetPaletteIndex(unsigned char *pFaceData, ImageFormat format,
                          int imageWidth, int x, int y,
                          S3PaletteIndex paletteIndex);

const char *S3TC_GetBlock(
    const void *pCompressed, ImageFormat format,
    int nBlocksWide,  // How many blocks wide is the image (pixels wide / 4).
    int xBlock, int yBlock);

char *S3TC_GetBlock(
    void *pCompressed, ImageFormat format,
    int nBlocksWide,  // How many blocks wide is the image (pixels wide / 4).
    int xBlock, int yBlock);

// Merge the two palettes and copy the colors
void S3TC_MergeBlocks(char **blocks,
                      S3RGBA **pOriginals,  // Original RGBA colors in the
                                            // texture. This allows it to avoid
                                            // doubly compressing.
                      int nBlocks,
                      int lPitch,  // (in BYTES)
                      ImageFormat format);

// Convert an RGB565 color to RGBA8888.
inline S3RGBA S3TC_RGBAFrom565(uint16_t color, uint8_t alphaValue = 255) {
  S3RGBA ret;
  ret.a = alphaValue;
  ret.r = (uint8_t)((color >> 11) << 3);
  ret.g = (uint8_t)(((color >> 5) & 0x3F) << 2);
  ret.b = (uint8_t)((color & 0x1F) << 3);
  return ret;
}

// Blend from one color to another..
inline S3RGBA S3TC_RGBABlend(const S3RGBA &a, const S3RGBA &b, int32_t aMul,
                             int32_t bMul, int32_t div) {
  S3RGBA ret;
  ret.r = (uint8_t)(((int32_t)a.r * aMul + (int32_t)b.r * bMul) / div);
  ret.g = (uint8_t)(((int32_t)a.g * aMul + (int32_t)b.g * bMul) / div);
  ret.b = (uint8_t)(((int32_t)a.b * aMul + (int32_t)b.b * bMul) / div);
  ret.a = (uint8_t)(((int32_t)a.a * aMul + (int32_t)b.a * bMul) / div);
  return ret;
}

#endif  // SOURCE_VTF_S3TC_DECODE_H_
