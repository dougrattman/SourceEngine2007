// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bitmap/imageformat.h"

#include <malloc.h>
#include <memory.h>
#include "mathlib/compressed_vector.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"

// nvtc include ddraw which include windows win min/max macro
#define NOMINMAX
#include "nvtc.h"

#include "base/include/compiler_specific.h"
#include "base/include/macros.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlmemory.h"

// Should be last include
#include "tier0/include/memdbgon.h"

namespace ImageLoader {
// Gamma correction
static void ConstructFloatGammaTable(f32 *table, f32 srcGamma, f32 dstGamma) {
  for (u32 i = 0; i < 256; i++) {
    table[i] = 255.0f * pow(i / 255.0f, srcGamma / dstGamma);
  }
}

void ConstructGammaTable(u8 *table, f32 srcGamma, f32 dstGamma) {
  for (u32 i = 0; i < 256; i++) {
    f32 f = 255.0f * pow(i / 255.0f, srcGamma / dstGamma);
    u8 v = std::clamp((int)(f + 0.5f), 0, 255);
    table[i] = v;
  }
}

void GammaCorrectRGBA8888(u8 *pSrc, u8 *pDst, int width, int height, int depth,
                          u8 *pGammaTable) {
  for (int h = 0; h < depth; ++h) {
    for (int i = 0; i < height; ++i) {
      for (int j = 0; j < width; ++j) {
        int idx = (h * width * height + i * width + j) * 4;

        // don't gamma correct alpha
        pDst[idx] = pGammaTable[pSrc[idx]];
        pDst[idx + 1] = pGammaTable[pSrc[idx + 1]];
        pDst[idx + 2] = pGammaTable[pSrc[idx + 2]];
      }
    }
  }
}

void GammaCorrectRGBA8888(u8 *src, u8 *dst, int width, int height, int depth,
                          f32 srcGamma, f32 dstGamma) {
  if (srcGamma == dstGamma) {
    if (src != dst) {
      memcpy(
          dst, src,
          GetMemRequired(width, height, depth, IMAGE_FORMAT_RGBA8888, false));
    }
    return;
  }

  static u8 gamma[256];
  static f32 lastSrcGamma = -1, lastDstGamma = -1;

  if (lastSrcGamma != srcGamma || lastDstGamma != dstGamma) {
    ConstructGammaTable(gamma, srcGamma, dstGamma);
    lastSrcGamma = srcGamma;
    lastDstGamma = dstGamma;
  }

  GammaCorrectRGBA8888(src, dst, width, height, depth, gamma);
}

// Generate a NICE filter kernel
static void GenerateNiceFilter(f32 wratio, f32 hratio, f32 dratio,
                               int kernelDiameter, f32 *pKernel,
                               f32 *pInvKernel) {
  // Compute a kernel...
  int kernelWidth = kernelDiameter * wratio;
  int kernelHeight = kernelDiameter * hratio;
  int kernelDepth = (dratio != 0) ? kernelDiameter * dratio : 1;

  // This is a NICE filter
  // sinc pi*x * a box from -3 to 3 * sinc ( pi * x/3)
  // where x is the pixel # in the destination (shrunken) image.
  // only problem here is that the NICE filter has a very large kernel
  // (7x7 x wratio x hratio x dratio)
  f32 dx = 1.0f / wratio, dy = 1.0f / hratio;
  f32 z, dz;

  if (dratio != 0.0f) {
    dz = 1.0f / (f32)dratio;
    z = -((f32)kernelDiameter - dz) * 0.5f;
  } else {
    dz = 0.0f;
    z = 0.0f;
  }

  f32 total = 0.0f;
  for (int h = 0; h < kernelDepth; ++h) {
    f32 y = -((f32)kernelDiameter - dy) * 0.5f;

    for (int i = 0; i < kernelHeight; ++i) {
      f32 x = -((f32)kernelDiameter - dx) * 0.5f;

      for (int j = 0; j < kernelWidth; ++j) {
        int nKernelIndex = kernelWidth * (i + h * kernelHeight) + j;
        f32 d = sqrt(x * x + y * y + z * z);

        if (d > kernelDiameter * 0.5f) {
          pKernel[nKernelIndex] = 0.0f;
        } else {
          f32 t = M_PI_F * d;

          if (t != 0) {
            f32 sinc = sin(t) / t;
            f32 sinc3 = 3.0f * sin(t / 3.0f) / t;

            pKernel[nKernelIndex] = sinc * sinc3;
          } else {
            pKernel[nKernelIndex] = 1.0f;
          }

          total += pKernel[nKernelIndex];
        }

        x += dx;
      }

      y += dy;
    }

    z += dz;
  }

  // normalize
  f32 flInvFactor = dratio == 0.0 ? wratio * hratio : dratio * wratio * hratio;
  f32 flInvTotal = total != 0.0f ? 1.0f / total : 1.0f;

  for (int h = 0; h < kernelDepth; ++h) {
    for (int i = 0; i < kernelHeight; ++i) {
      int nPixel = kernelWidth * (h * kernelHeight + i);

      for (int j = 0; j < kernelWidth; ++j) {
        pKernel[nPixel + j] *= flInvTotal;
        pInvKernel[nPixel + j] = flInvFactor * pKernel[nPixel + j];
      }
    }
  }
}

// Resample an image
static constexpr inline u8 Clamp(f32 x) {
  return std::clamp((int)(x + 0.5f), 0, 255);
}

struct KernelInfo {
  f32 *m_pKernel;
  f32 *m_pInvKernel;
  int m_nWidth;
  int m_nHeight;
  int m_nDepth;
  int m_nDiameter;
};

enum class KernelType {
  KERNEL_DEFAULT = 0,
  KERNEL_NORMALMAP,
  KERNEL_ALPHATEST,
};

using ApplyKernelFunc = void (*)(const KernelInfo &kernel,
                                 const ResampleInfo_t &info, int wratio,
                                 int hratio, int dratio, f32 *gammaToLinear,
                                 f32 *pAlphaResult);

// Apply Kernel to an image
template <KernelType type, bool is_nice_filter>
class KernelWrapper {
 public:
  static inline int ActualX(int x, const ResampleInfo_t &info) {
    if (info.m_nFlags & RESAMPLE_CLAMPS)
      return std::clamp(x, 0, info.m_nSrcWidth - 1);

    // This works since info.m_nSrcWidth is a power of two.
    // Even for negative #s!
    return x & (info.m_nSrcWidth - 1);
  }

  static inline int ActualY(int y, const ResampleInfo_t &info) {
    if (info.m_nFlags & RESAMPLE_CLAMPT)
      return std::clamp(y, 0, info.m_nSrcHeight - 1);

    // This works since info.m_nSrcHeight is a power of two.
    // Even for negative #s!
    return y & (info.m_nSrcHeight - 1);
  }

  static inline int ActualZ(int z, const ResampleInfo_t &info) {
    if (info.m_nFlags & RESAMPLE_CLAMPU)
      return std::clamp(z, 0, info.m_nSrcDepth - 1);

    // This works since info.m_nSrcDepth is a power of two.
    // Even for negative #s!
    return z & (info.m_nSrcDepth - 1);
  }

  static void ComputeAveragedColor(const KernelInfo &kernel,
                                   const ResampleInfo_t &info, int startX,
                                   int startY, int startZ, f32 *gammaToLinear,
                                   f32 *total) {
    total[0] = total[1] = total[2] = total[3] = 0.0f;
    for (int j = 0, srcZ = startZ; j < kernel.m_nDepth; ++j, ++srcZ) {
      int sz = ActualZ(srcZ, info);
      sz *= info.m_nSrcWidth * info.m_nSrcHeight;

      for (int k = 0, srcY = startY; k < kernel.m_nHeight; ++k, ++srcY) {
        int sy = ActualY(srcY, info);
        sy *= info.m_nSrcWidth;

        int kernelIdx;
        if (is_nice_filter) {
          kernelIdx = kernel.m_nWidth * (k + j * kernel.m_nHeight);
        } else {
          kernelIdx = 0;
        }

        for (int l = 0, srcX = startX; l < kernel.m_nWidth;
             ++l, ++srcX, ++kernelIdx) {
          int sx = ActualX(srcX, info);
          int srcPixel = (sz + sy + sx) << 2;

          f32 flKernelFactor;
          if (is_nice_filter) {
            flKernelFactor = kernel.m_pKernel[kernelIdx];
            if (flKernelFactor == 0.0f) continue;
          } else {
            flKernelFactor = kernel.m_pKernel[0];
          }

          switch (type) {
            case KernelType::KERNEL_NORMALMAP: {
              total[0] += flKernelFactor * info.m_pSrc[srcPixel + 0];
              total[1] += flKernelFactor * info.m_pSrc[srcPixel + 1];
              total[2] += flKernelFactor * info.m_pSrc[srcPixel + 2];
              total[3] += flKernelFactor * info.m_pSrc[srcPixel + 3];
            } break;
            case KernelType::KERNEL_ALPHATEST: {
              total[0] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 0]];
              total[1] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 1]];
              total[2] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 2]];
              if (info.m_pSrc[srcPixel + 3] > 192) {
                total[3] += flKernelFactor * 255.0f;
              }
            } break;
            default: {
              total[0] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 0]];
              total[1] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 1]];
              total[2] +=
                  flKernelFactor * gammaToLinear[info.m_pSrc[srcPixel + 2]];
              total[3] += flKernelFactor * info.m_pSrc[srcPixel + 3];
            }
          }
        }
      }
    }
  }

  static void AddAlphaToAlphaResult(const KernelInfo &kernel,
                                    const ResampleInfo_t &info, int startX,
                                    int startY, int startZ, f32 flAlpha,
                                    f32 *pAlphaResult) {
    for (int j = 0, srcZ = startZ; j < kernel.m_nDepth; ++j, ++srcZ) {
      int sz = ActualZ(srcZ, info);
      sz *= info.m_nSrcWidth * info.m_nSrcHeight;

      for (int k = 0, srcY = startY; k < kernel.m_nHeight; ++k, ++srcY) {
        int sy = ActualY(srcY, info);
        sy *= info.m_nSrcWidth;

        int kernelIdx;
        if (is_nice_filter) {
          kernelIdx =
              k * kernel.m_nWidth + j * kernel.m_nWidth * kernel.m_nHeight;
        } else {
          kernelIdx = 0;
        }

        for (int l = 0, srcX = startX; l < kernel.m_nWidth;
             ++l, ++srcX, ++kernelIdx) {
          int sx = ActualX(srcX, info);
          int srcPixel = sz + sy + sx;

          f32 flKernelFactor;
          if (is_nice_filter) {
            flKernelFactor = kernel.m_pInvKernel[kernelIdx];
            if (flKernelFactor == 0.0f) continue;
          } else {
            flKernelFactor = kernel.m_pInvKernel[0];
          }

          pAlphaResult[srcPixel] += flKernelFactor * flAlpha;
        }
      }
    }
  }

  static void AdjustAlphaChannel(const KernelInfo &kernel,
                                 const ResampleInfo_t &info, int wratio,
                                 int hratio, int dratio, f32 *pAlphaResult) {
    // Find the delta between the alpha + source image
    for (int k = 0; k < info.m_nSrcDepth; ++k) {
      for (int i = 0; i < info.m_nSrcHeight; ++i) {
        int dstPixel =
            i * info.m_nSrcWidth + k * info.m_nSrcWidth * info.m_nSrcHeight;
        for (int j = 0; j < info.m_nSrcWidth; ++j, ++dstPixel) {
          pAlphaResult[dstPixel] =
              fabs(pAlphaResult[dstPixel] - info.m_pSrc[dstPixel * 4 + 3]);
        }
      }
    }

    // Apply the kernel to the image
    int nInitialZ = (dratio >> 1) - ((dratio * kernel.m_nDiameter) >> 1);
    int nInitialY = (hratio >> 1) - ((hratio * kernel.m_nDiameter) >> 1);
    int nInitialX = (wratio >> 1) - ((wratio * kernel.m_nDiameter) >> 1);

    f32 flAlphaThreshhold = (info.m_flAlphaHiFreqThreshhold >= 0)
                                ? 255.0f * info.m_flAlphaHiFreqThreshhold
                                : 255.0f * 0.4f;

    f32 flInvFactor = (dratio == 0) ? 1.0f / (hratio * wratio)
                                    : 1.0f / (hratio * wratio * dratio);

    for (int h = 0; h < info.m_nDestDepth; ++h) {
      int startZ = dratio * h + nInitialZ;
      for (int i = 0; i < info.m_nDestHeight; ++i) {
        int startY = hratio * i + nInitialY;
        int dstPixel = (info.m_nDestWidth * (i + h * info.m_nDestHeight)) << 2;
        for (int j = 0; j < info.m_nDestWidth; ++j, dstPixel += 4) {
          if (info.m_pDest[dstPixel + 3] == 255) continue;

          int startX = wratio * j + nInitialX;
          f32 flAlphaDelta = 0.0f;

          for (int m = 0, srcZ = startZ; m < dratio; ++m, ++srcZ) {
            int sz = ActualZ(srcZ, info);
            sz *= info.m_nSrcWidth * info.m_nSrcHeight;

            for (int k = 0, srcY = startY; k < hratio; ++k, ++srcY) {
              int sy = ActualY(srcY, info);
              sy *= info.m_nSrcWidth;

              for (int l = 0, srcX = startX; l < wratio; ++l, ++srcX) {
                int sx = ActualX(srcX, info);
                int srcPixel = sz + sy + sx;
                flAlphaDelta += pAlphaResult[srcPixel];
              }
            }
          }

          flAlphaDelta *= flInvFactor;
          if (flAlphaDelta > flAlphaThreshhold) {
            info.m_pDest[dstPixel + 3] = 255.0f;
          }
        }
      }
    }
  }

  static void ApplyKernel(const KernelInfo &kernel, const ResampleInfo_t &info,
                          int wratio, int hratio, int dratio,
                          f32 *gammaToLinear, f32 *pAlphaResult) {
    f32 invDstGamma = 1.0f / info.m_flDestGamma;

    // Apply the kernel to the image
    int nInitialZ = (dratio >> 1) - ((dratio * kernel.m_nDiameter) >> 1);
    int nInitialY = (hratio >> 1) - ((hratio * kernel.m_nDiameter) >> 1);
    int nInitialX = (wratio >> 1) - ((wratio * kernel.m_nDiameter) >> 1);

    f32 flAlphaThreshhold = (info.m_flAlphaThreshhold >= 0)
                                ? 255.0f * info.m_flAlphaThreshhold
                                : 255.0f * 0.4f;
    for (int k = 0; k < info.m_nDestDepth; ++k) {
      int startZ = dratio * k + nInitialZ;

      for (int i = 0; i < info.m_nDestHeight; ++i) {
        int startY = hratio * i + nInitialY;
        int dstPixel =
            (i * info.m_nDestWidth + k * info.m_nDestWidth * info.m_nDestHeight)
            << 2;

        for (int j = 0; j < info.m_nDestWidth; ++j, dstPixel += 4) {
          int startX = wratio * j + nInitialX;

          f32 total[4];
          ComputeAveragedColor(kernel, info, startX, startY, startZ,
                               gammaToLinear, total);

          switch (type) {
            case KernelType::KERNEL_NORMALMAP: {
              for (int ch = 0; ch < 4; ++ch)
                info.m_pDest[dstPixel + ch] =
                    Clamp(info.m_flColorGoal[ch] +
                          (info.m_flColorScale[ch] *
                           (total[ch] - info.m_flColorGoal[ch])));
            } break;
            case KernelType::KERNEL_ALPHATEST: {
              // If there's more than 40% coverage, then keep the pixel
              // (renormalize the color based on coverage)
              f32 flAlpha = (total[3] >= flAlphaThreshhold) ? 255 : 0;

              for (int ch = 0; ch < 3; ++ch)
                info.m_pDest[dstPixel + ch] =
                    Clamp(255.0f * pow((info.m_flColorGoal[ch] +
                                        (info.m_flColorScale[ch] *
                                         ((total[ch] > 0 ? total[ch] : 0) -
                                          info.m_flColorGoal[ch]))) /
                                           255.0f,
                                       invDstGamma));
              info.m_pDest[dstPixel + 3] = Clamp(flAlpha);

              AddAlphaToAlphaResult(kernel, info, startX, startY, startZ,
                                    flAlpha, pAlphaResult);
            } break;
            default: {
              for (int ch = 0; ch < 3; ++ch)
                info.m_pDest[dstPixel + ch] =
                    Clamp(255.0f * pow((info.m_flColorGoal[ch] +
                                        (info.m_flColorScale[ch] *
                                         ((total[ch] > 0 ? total[ch] : 0) -
                                          info.m_flColorGoal[ch]))) /
                                           255.0f,
                                       invDstGamma));
              info.m_pDest[dstPixel + 3] = Clamp(
                  info.m_flColorGoal[3] + (info.m_flColorScale[3] *
                                           (total[3] - info.m_flColorGoal[3])));
            }
          }
        }
      }

      if (MSVC_SCOPED_DISABLE_WARNING(4127,
                                      type == KernelType::KERNEL_ALPHATEST)) {
        AdjustAlphaChannel(kernel, info, wratio, hratio, dratio, pAlphaResult);
      }
    }
  }
};

using ApplyKernelDefault_t = KernelWrapper<KernelType::KERNEL_DEFAULT, false>;
using ApplyKernelNormalmap_t =
    KernelWrapper<KernelType::KERNEL_NORMALMAP, false>;
using ApplyKernelAlphatest_t =
    KernelWrapper<KernelType::KERNEL_ALPHATEST, false>;
using ApplyKernelDefaultNice_t =
    KernelWrapper<KernelType::KERNEL_DEFAULT, true>;
using ApplyKernelNormalmapNice_t =
    KernelWrapper<KernelType::KERNEL_NORMALMAP, true>;
using ApplyKernelAlphatestNice_t =
    KernelWrapper<KernelType::KERNEL_ALPHATEST, true>;

static ApplyKernelFunc g_KernelFunc[] = {
    ApplyKernelDefault_t::ApplyKernel,
    ApplyKernelNormalmap_t::ApplyKernel,
    ApplyKernelAlphatest_t::ApplyKernel,
};

static ApplyKernelFunc g_KernelFuncNice[] = {
    ApplyKernelDefaultNice_t::ApplyKernel,
    ApplyKernelNormalmapNice_t::ApplyKernel,
    ApplyKernelAlphatestNice_t::ApplyKernel,
};

bool ResampleRGBA8888(const ResampleInfo_t &info) {
  // No resampling needed, just gamma correction
  if (info.m_nSrcWidth == info.m_nDestWidth &&
      info.m_nSrcHeight == info.m_nDestHeight &&
      info.m_nSrcDepth == info.m_nDestDepth) {
    // Here, we need to gamma convert the source image..
    GammaCorrectRGBA8888(info.m_pSrc, info.m_pDest, info.m_nSrcWidth,
                         info.m_nSrcHeight, info.m_nSrcDepth, info.m_flSrcGamma,
                         info.m_flDestGamma);
    return true;
  }

  // fixme: has to be power of two for now.
  if (!IsPowerOfTwo(info.m_nSrcWidth) || !IsPowerOfTwo(info.m_nSrcHeight) ||
      !IsPowerOfTwo(info.m_nSrcDepth) || !IsPowerOfTwo(info.m_nDestWidth) ||
      !IsPowerOfTwo(info.m_nDestHeight) || !IsPowerOfTwo(info.m_nDestDepth)) {
    return false;
  }

  // fixme: can only downsample for now.
  if ((info.m_nSrcWidth < info.m_nDestWidth) ||
      (info.m_nSrcHeight < info.m_nDestHeight) ||
      (info.m_nSrcDepth < info.m_nDestDepth)) {
    return false;
  }

  // Compute gamma tables...
  static f32 gammaToLinear[256];
  static f32 lastSrcGamma = -1;

  if (lastSrcGamma != info.m_flSrcGamma) {
    ConstructFloatGammaTable(gammaToLinear, info.m_flSrcGamma, 1.0f);
    lastSrcGamma = info.m_flSrcGamma;
  }

  int wratio = info.m_nSrcWidth / info.m_nDestWidth;
  int hratio = info.m_nSrcHeight / info.m_nDestHeight;
  int dratio = (info.m_nSrcDepth != info.m_nDestDepth)
                   ? info.m_nSrcDepth / info.m_nDestDepth
                   : 0;

  KernelInfo kernel;

  f32 *pTempMemory = 0;
  f32 *pTempInvMemory = 0;
  static f32 *kernelCache[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  static f32 *pInvKernelCache[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  f32 pKernelMem[1];
  f32 pInvKernelMem[1];
  if (info.m_nFlags & RESAMPLE_NICE_FILTER) {
    // Kernel size is measured in dst pixels
    kernel.m_nDiameter = 6;

    // Compute a kernel...
    kernel.m_nWidth = kernel.m_nDiameter * wratio;
    kernel.m_nHeight = kernel.m_nDiameter * hratio;
    kernel.m_nDepth = kernel.m_nDiameter * dratio;
    if (kernel.m_nDepth == 0) {
      kernel.m_nDepth = 1;
    }

    // Cache the filter (2d kernels only)....
    int power = -1;

    if ((wratio == hratio) && (dratio == 0)) {
      power = 0;
      int tempWidth = wratio;
      while (tempWidth > 1) {
        ++power;
        tempWidth >>= 1;
      }

      // Don't cache anything bigger than 512x512
      if (power >= 10) {
        power = -1;
      }
    }

    if (power >= 0) {
      if (!kernelCache[power]) {
        kernelCache[power] = new f32[kernel.m_nWidth * kernel.m_nHeight];
        pInvKernelCache[power] = new f32[kernel.m_nWidth * kernel.m_nHeight];
        GenerateNiceFilter(wratio, hratio, dratio, kernel.m_nDiameter,
                           kernelCache[power], pInvKernelCache[power]);
      }

      kernel.m_pKernel = kernelCache[power];
      kernel.m_pInvKernel = pInvKernelCache[power];
    } else {
      // Don't cache non-square kernels, or 3d kernels
      pTempMemory =
          new f32[kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth];
      pTempInvMemory =
          new f32[kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth];
      GenerateNiceFilter(wratio, hratio, dratio, kernel.m_nDiameter,
                         pTempMemory, pTempInvMemory);
      kernel.m_pKernel = pTempMemory;
      kernel.m_pInvKernel = pTempInvMemory;
    }
  } else {
    // Compute a kernel...
    kernel.m_nWidth = wratio;
    kernel.m_nHeight = hratio;
    kernel.m_nDepth = dratio ? dratio : 1;

    kernel.m_nDiameter = 1;

    // Simple implementation of a box filter that doesn't block the stack!
    pKernelMem[0] =
        1.0f / (f32)(kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth);
    pInvKernelMem[0] = 1.0f;
    kernel.m_pKernel = pKernelMem;
    kernel.m_pInvKernel = pInvKernelMem;
  }

  f32 *pAlphaResult = nullptr;
  KernelType type;

  if (info.m_nFlags & RESAMPLE_NORMALMAP) {
    type = KernelType::KERNEL_NORMALMAP;
  } else if (info.m_nFlags & RESAMPLE_ALPHATEST) {
    int nSize =
        info.m_nSrcHeight * info.m_nSrcWidth * info.m_nSrcDepth * sizeof(f32);
    pAlphaResult = (f32 *)malloc(nSize);
    memset(pAlphaResult, 0, nSize);
    type = KernelType::KERNEL_ALPHATEST;
  } else {
    type = KernelType::KERNEL_DEFAULT;
  }

  if (info.m_nFlags & RESAMPLE_NICE_FILTER) {
    g_KernelFuncNice[static_cast<std::underlying_type_t<decltype(type)>>(type)](
        kernel, info, wratio, hratio, dratio, gammaToLinear, pAlphaResult);
    if (pTempMemory) {
      delete[] pTempMemory;
    }
  } else {
    g_KernelFunc[static_cast<std::underlying_type_t<decltype(type)>>(type)](
        kernel, info, wratio, hratio, dratio, gammaToLinear, pAlphaResult);
  }

  if (pAlphaResult) free(pAlphaResult);

  return true;
}

bool ResampleRGBA16161616(const ResampleInfo_t &info) {
  // HDRFIXME: This is some lame shit right here. (We need to get NICE working,
  // etc, etc.)

  // Make sure everything is power of two.
  Assert((info.m_nSrcWidth & (info.m_nSrcWidth - 1)) == 0);
  Assert((info.m_nSrcHeight & (info.m_nSrcHeight - 1)) == 0);
  Assert((info.m_nDestWidth & (info.m_nDestWidth - 1)) == 0);
  Assert((info.m_nDestHeight & (info.m_nDestHeight - 1)) == 0);

  // Make sure that we aren't upscsaling the image. . .we do`n't support that
  // very well.
  Assert(info.m_nSrcWidth >= info.m_nDestWidth);
  Assert(info.m_nSrcHeight >= info.m_nDestHeight);

  int nSampleWidth = info.m_nSrcWidth / info.m_nDestWidth;
  int nSampleHeight = info.m_nSrcHeight / info.m_nDestHeight;

  u16 *pSrc = (u16 *)info.m_pSrc, *pDst = (u16 *)info.m_pDest;

  for (int y = 0; y < info.m_nDestHeight; y++) {
    for (int x = 0; x < info.m_nDestWidth; x++) {
      int accum[4]{0, 0, 0, 0};

      for (int nSampleY = 0; nSampleY < nSampleHeight; nSampleY++) {
        for (int nSampleX = 0; nSampleX < nSampleWidth; nSampleX++) {
          accum[0] +=
              (int)pSrc[((x * nSampleWidth + nSampleX) +
                         (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                            4 +
                        0];
          accum[1] +=
              (int)pSrc[((x * nSampleWidth + nSampleX) +
                         (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                            4 +
                        1];
          accum[2] +=
              (int)pSrc[((x * nSampleWidth + nSampleX) +
                         (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                            4 +
                        2];
          accum[3] +=
              (int)pSrc[((x * nSampleWidth + nSampleX) +
                         (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                            4 +
                        3];
        }
      }
      for (u32 i = 0; i < 4; i++) {
        accum[i] /= (nSampleWidth * nSampleHeight);
        accum[i] = std::max(accum[i], 0);
        accum[i] = std::min(accum[i], 65535);
        pDst[(x + y * info.m_nDestWidth) * 4 + i] = (u16)accum[i];
      }
    }
  }
  return true;
}

bool ResampleRGB323232F(const ResampleInfo_t &info) {
  // HDRFIXME: This is some lame shit right here. (We need to get NICE working,
  // etc, etc.)

  // Make sure everything is power of two.
  Assert((info.m_nSrcWidth & (info.m_nSrcWidth - 1)) == 0);
  Assert((info.m_nSrcHeight & (info.m_nSrcHeight - 1)) == 0);
  Assert((info.m_nDestWidth & (info.m_nDestWidth - 1)) == 0);
  Assert((info.m_nDestHeight & (info.m_nDestHeight - 1)) == 0);

  // Make sure that we aren't upscaling the image. . .we do`n't support that
  // very well.
  Assert(info.m_nSrcWidth >= info.m_nDestWidth);
  Assert(info.m_nSrcHeight >= info.m_nDestHeight);

  int nSampleWidth = info.m_nSrcWidth / info.m_nDestWidth;
  int nSampleHeight = info.m_nSrcHeight / info.m_nDestHeight;

  f32 *pSrc = (f32 *)info.m_pSrc;
  f32 *pDst = (f32 *)info.m_pDest;

  for (int y = 0; y < info.m_nDestHeight; y++) {
    for (int x = 0; x < info.m_nDestWidth; x++) {
      f32 accum[4]{0, 0, 0, 0};

      for (int nSampleY = 0; nSampleY < nSampleHeight; nSampleY++) {
        for (int nSampleX = 0; nSampleX < nSampleWidth; nSampleX++) {
          accum[0] += pSrc[((x * nSampleWidth + nSampleX) +
                            (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                               3 +
                           0];
          accum[1] += pSrc[((x * nSampleWidth + nSampleX) +
                            (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                               3 +
                           1];
          accum[2] += pSrc[((x * nSampleWidth + nSampleX) +
                            (y * nSampleHeight + nSampleY) * info.m_nSrcWidth) *
                               3 +
                           2];
        }
      }

      for (u32 i = 0; i < 3; i++) {
        accum[i] /= (nSampleWidth * nSampleHeight);
        pDst[(x + y * info.m_nDestWidth) * 3 + i] = accum[i];
      }
    }
  }

  return true;
}

// Generates mipmap levels
void GenerateMipmapLevels(u8 *pSrc, u8 *pDst, int width, int height, int depth,
                          ImageFormat imageFormat, f32 srcGamma, f32 dstGamma,
                          int numLevels) {
  int dstWidth = width;
  int dstHeight = height;
  int dstDepth = depth;

  // temporary storage for the mipmaps
  int tempMem = GetMemRequired(dstWidth, dstHeight, dstDepth,
                               IMAGE_FORMAT_RGBA8888, false);
  CUtlMemory<u8> tmpImage;
  tmpImage.EnsureCapacity(tempMem);

  while (true) {
    // This generates a mipmap in RGBA8888, linear space
    ResampleInfo_t info;
    info.m_pSrc = pSrc;
    info.m_pDest = tmpImage.Base();
    info.m_nSrcWidth = width;
    info.m_nSrcHeight = height;
    info.m_nSrcDepth = depth;
    info.m_nDestWidth = dstWidth;
    info.m_nDestHeight = dstHeight;
    info.m_nDestDepth = dstDepth;
    info.m_flSrcGamma = srcGamma;
    info.m_flDestGamma = dstGamma;

    ResampleRGBA8888(info);

    // each mipmap level needs to be color converted separately
    ConvertImageFormat(tmpImage.Base(), IMAGE_FORMAT_RGBA8888, pDst,
                       imageFormat, dstWidth, dstHeight, 0, 0);

    if (numLevels == 0) {
      // We're done after we've made the 1x1 mip level
      if (dstWidth == 1 && dstHeight == 1 && dstDepth == 1) return;
    } else {
      if (--numLevels <= 0) return;
    }

    // Figure out where the next level goes
    int memRequired = ImageLoader::GetMemRequired(dstWidth, dstHeight, dstDepth,
                                                  imageFormat, false);
    pDst += memRequired;

    // shrink by a factor of 2, but clamp at 1 pixel (non-square textures)
    dstWidth = dstWidth > 1 ? dstWidth >> 1 : 1;
    dstHeight = dstHeight > 1 ? dstHeight >> 1 : 1;
    dstDepth = dstDepth > 1 ? dstDepth >> 1 : 1;
  }
}
}  // namespace ImageLoader
