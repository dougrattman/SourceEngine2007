// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "s3tc_decode.h"

#include <malloc.h>
#include <memory.h>
#include <cstdint>

#include "bitmap/imageformat.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlmemory.h"
#include "tier1/utlvector.h"

#include "tier0/include/memdbgon.h"

struct S3Palette {
  S3RGBA m_Colors[4];
};

struct S3TCBlock_DXT1 {
  uint16_t m_Ref1;  // The two colors that this block blends betwixt.
  uint16_t m_Ref2;
  uint32_t m_PixelBits;
};

struct S3TCBlock_DXT5 {
  uint8_t m_AlphaRef[2];
  uint8_t m_AlphaBits[6];

  uint16_t m_Ref1;  // The two colors that this block blends betwixt.
  uint16_t m_Ref2;
  uint32_t m_PixelBits;
};

// S3TCBlock
int ReadBitInt(const char *pBits, int iBaseBit, int nBits) {
  int ret = 0;
  for (int i = 0; i < nBits; i++) {
    int iBit = iBaseBit + i;
    int val = ((pBits[iBit >> 3] >> (iBit & 7)) & 1) << i;
    ret |= val;
  }
  return ret;
}

void WriteBitInt(char *pBits, int iBaseBit, int nBits, int val) {
  for (int i = 0; i < nBits; i++) {
    int iBit = iBaseBit + i;
    pBits[iBit >> 3] &= ~(1 << (iBit & 7));
    if ((val >> i) & 1) pBits[iBit >> 3] |= (1 << (iBit & 7));
  }
}

int S3TC_BytesPerBlock(ImageFormat format) {
  if (format == IMAGE_FORMAT_DXT1 || format == IMAGE_FORMAT_ATI1N) {
    return 8;
  }

  Assert(format == IMAGE_FORMAT_DXT5 || format == IMAGE_FORMAT_ATI2N);
  return 16;
}

S3PaletteIndex S3TC_GetPixelPaletteIndex(ImageFormat format,
                                         const char *pS3Block, int x, int y) {
  Assert(x >= 0 && x < 4);
  Assert(y >= 0 && y < 4);
  int iQuadPixel = y * 4 + x;
  S3PaletteIndex ret{0, 0};

  if (format == IMAGE_FORMAT_DXT1) {
    const S3TCBlock_DXT1 *pBlock =
        reinterpret_cast<const S3TCBlock_DXT1 *>(pS3Block);
    ret.m_ColorIndex = (pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3;
    ret.m_AlphaIndex = 0;
  } else {
    Assert(format == IMAGE_FORMAT_DXT5);

    const S3TCBlock_DXT5 *pBlock =
        reinterpret_cast<const S3TCBlock_DXT5 *>(pS3Block);

    int64_t &alphaBits = *((int64_t *)pBlock->m_AlphaBits);
    ret.m_ColorIndex =
        (unsigned char)((pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3);
    ret.m_AlphaIndex = (unsigned char)((alphaBits >> (iQuadPixel * 3)) & 7);
  }

  return ret;
}

void S3TC_SetPixelPaletteIndex(ImageFormat format, char *pS3Block, int x, int y,
                               S3PaletteIndex iPaletteIndex) {
  Assert(x >= 0 && x < 4);
  Assert(y >= 0 && y < 4);
  Assert(iPaletteIndex.m_ColorIndex >= 0 && iPaletteIndex.m_ColorIndex < 4);
  Assert(iPaletteIndex.m_AlphaIndex >= 0 && iPaletteIndex.m_AlphaIndex < 8);

  int iQuadPixel = y * 4 + x;
  int iColorBit = iQuadPixel * 2;

  if (format == IMAGE_FORMAT_DXT1) {
    S3TCBlock_DXT1 *pBlock = reinterpret_cast<S3TCBlock_DXT1 *>(pS3Block);

    pBlock->m_PixelBits &= ~(3 << iColorBit);
    pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex
                           << iColorBit;
  } else {
    Assert(format == IMAGE_FORMAT_DXT5);

    S3TCBlock_DXT5 *pBlock = reinterpret_cast<S3TCBlock_DXT5 *>(pS3Block);

    // Copy the color portion in.
    pBlock->m_PixelBits &= ~(3 << iColorBit);
    pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex
                           << iColorBit;

    // Copy the alpha portion in.
    WriteBitInt((char *)pBlock->m_AlphaBits, iQuadPixel * 3, 3,
                iPaletteIndex.m_AlphaIndex);
  }
}

const char *S3TC_GetBlock(const void *pCompressed, ImageFormat format,
                          int nBlocksWidth, int xBlock, int yBlock) {
  int nBytesPerBlock = S3TC_BytesPerBlock(format);
  return &(
      (const char *)
          pCompressed)[((yBlock * nBlocksWidth) + xBlock) * nBytesPerBlock];
}

char *S3TC_GetBlock(void *pCompressed, ImageFormat format, int nBlocksWidth,
                    int xBlock, int yBlock) {
  return (char *)S3TC_GetBlock((const void *)pCompressed, format, nBlocksWidth,
                               xBlock, yBlock);
}

void GenerateRepresentativePalette(
    ImageFormat format,
    S3RGBA **pOriginals,  // Original RGBA colors in the texture. This allows it
                          // to avoid doubly compressing.
    int nBlocks,
    int lPitch,  // (in BYTES)
    char mergedBlocks[16 * MAX_S3TC_BLOCK_BYTES]) {
  Error("GenerateRepresentativePalette: not implemented");
}

void S3TC_MergeBlocks(char **blocks,
                      S3RGBA **pOriginals,  // Original RGBA colors in the
                                            // texture. This allows it to avoid
                                            // doubly compressing.
                      int nBlocks,
                      int lPitch,  // (in BYTES)
                      ImageFormat format) {
  // Figure out a good palette to represent all of these blocks.
  char mergedBlocks[16 * MAX_S3TC_BLOCK_BYTES];
  GenerateRepresentativePalette(format, pOriginals, nBlocks, lPitch,
                                mergedBlocks);

  // Build a remap table to remap block 2's colors to block 1's colors.
  if (format == IMAGE_FORMAT_DXT1) {
    // Grab the palette indices that s3tc.lib made for us.
    const char *pBase = (const char *)mergedBlocks;
    pBase += 4;

    for (int iBlock = 0; iBlock < nBlocks; iBlock++) {
      S3TCBlock_DXT1 *pBlock = ((S3TCBlock_DXT1 *)blocks[iBlock]);

      // Remap all of the block's pixels.
      for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
          int iBaseBit = (y * nBlocks * 4 + x + iBlock * 4) * 2;

          S3PaletteIndex index = {0, 0};
          index.m_ColorIndex = ReadBitInt(pBase, iBaseBit, 2);

          S3TC_SetPixelPaletteIndex(format, (char *)pBlock, x, y, index);
        }
      }

      // Copy block 1's palette to block 2.
      pBlock->m_Ref1 = ((S3TCBlock_DXT1 *)mergedBlocks)->m_Ref1;
      pBlock->m_Ref2 = ((S3TCBlock_DXT1 *)mergedBlocks)->m_Ref2;
    }
  } else {
    Assert(format == IMAGE_FORMAT_DXT5);

    // Skip past the alpha palette.
    const char *pAlphaPalette = mergedBlocks;
    const char *pAlphaBits = mergedBlocks + 2;

    // Skip past the alpha pixel bits and past the color palette.
    const char *pColorPalette = pAlphaBits + 6 * nBlocks;
    const char *pColorBits = pColorPalette + 4;

    for (int iBlock = 0; iBlock < nBlocks; iBlock++) {
      S3TCBlock_DXT5 *pBlock = ((S3TCBlock_DXT5 *)blocks[iBlock]);

      // Remap all of the block's pixels.
      for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
          int iBasePixel = (y * nBlocks * 4 + x + iBlock * 4);

          S3PaletteIndex index;
          index.m_ColorIndex = ReadBitInt(pColorBits, iBasePixel * 2, 2);
          index.m_AlphaIndex = ReadBitInt(pAlphaBits, iBasePixel * 3, 3);

          S3TC_SetPixelPaletteIndex(format, (char *)pBlock, x, y, index);
        }
      }

      // Copy block 1's palette to block 2.
      pBlock->m_AlphaRef[0] = pAlphaPalette[0];
      pBlock->m_AlphaRef[1] = pAlphaPalette[1];
      pBlock->m_Ref1 = *((unsigned short *)pColorPalette);
      pBlock->m_Ref2 = *((unsigned short *)(pColorPalette + 2));
    }
  }
}

S3PaletteIndex S3TC_GetPaletteIndex(unsigned char *pFaceData,
                                    ImageFormat format, int imageWidth, int x,
                                    int y) {
  char *pBlock =
      S3TC_GetBlock(pFaceData, format, imageWidth >> 2, x >> 2, y >> 2);
  return S3TC_GetPixelPaletteIndex(format, pBlock, x & 3, y & 3);
}

void S3TC_SetPaletteIndex(unsigned char *pFaceData, ImageFormat format,
                          int imageWidth, int x, int y,
                          S3PaletteIndex paletteIndex) {
  char *pBlock =
      S3TC_GetBlock(pFaceData, format, imageWidth >> 2, x >> 2, y >> 2);
  S3TC_SetPixelPaletteIndex(format, pBlock, x & 3, y & 3, paletteIndex);
}
