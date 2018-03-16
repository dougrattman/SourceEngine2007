// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef PIXELWRITER_H
#define PIXELWRITER_H

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"
#include "build/include/build_config.h"
#include "mathlib/compressed_vector.h"
#include "mathlib/ssemath.h"
#include "tier0/include/dbg.h"

// Color writing class.
class CPixelWriter {
 public:
  SOURCE_FORCEINLINE void SetPixelMemory(ImageFormat format, void *pMemory,
                                         u16 stride) {
    bits_ = (u8 *)pMemory;
    base_ = bits_;
    bytes_per_row_ = stride;
    flags_ = 0;

    switch (format) {
      // NOTE: the low order bits are first in this naming convention.
      case IMAGE_FORMAT_R32F:
        size_ = 4;
        r_shift_ = 0;
        g_shift_ = 0;
        b_shift_ = 0;
        a_shift_ = 0;
        r_mask_ = 0xFFFFFFFF;
        g_mask_ = 0x0;
        b_mask_ = 0x0;
        a_mask_ = 0x0;
        flags_ |= PIXELWRITER_USING_FLOAT_FORMAT;
        break;

      case IMAGE_FORMAT_RGBA32323232F:
        size_ = 16;
        r_shift_ = 0;
        g_shift_ = 32;
        b_shift_ = 64;
        a_shift_ = 96;
        r_mask_ = 0xFFFFFFFF;
        g_mask_ = 0xFFFFFFFF;
        b_mask_ = 0xFFFFFFFF;
        a_mask_ = 0xFFFFFFFF;
        flags_ |= PIXELWRITER_USING_FLOAT_FORMAT;
        break;

      case IMAGE_FORMAT_RGBA16161616F:
        size_ = 8;
        r_shift_ = 0;
        g_shift_ = 16;
        b_shift_ = 32;
        a_shift_ = 48;
        r_mask_ = 0xFFFF;
        g_mask_ = 0xFFFF;
        b_mask_ = 0xFFFF;
        a_mask_ = 0xFFFF;
        flags_ |= PIXELWRITER_USING_FLOAT_FORMAT |
                  PIXELWRITER_USING_16BIT_FLOAT_FORMAT;
        break;

      case IMAGE_FORMAT_RGBA8888:
        size_ = 4;
        r_shift_ = 0;
        g_shift_ = 8;
        b_shift_ = 16;
        a_shift_ = 24;
        r_mask_ = 0xFF;
        g_mask_ = 0xFF;
        b_mask_ = 0xFF;
        a_mask_ = 0xFF;
        break;

      // NOTE: the low order bits are first in this naming convention.
      case IMAGE_FORMAT_BGRA8888:
        size_ = 4;
        r_shift_ = 16;
        g_shift_ = 8;
        b_shift_ = 0;
        a_shift_ = 24;
        r_mask_ = 0xFF;
        g_mask_ = 0xFF;
        b_mask_ = 0xFF;
        a_mask_ = 0xFF;
        break;

      case IMAGE_FORMAT_BGRX8888:
        size_ = 4;
        r_shift_ = 16;
        g_shift_ = 8;
        b_shift_ = 0;
        a_shift_ = 24;
        r_mask_ = 0xFF;
        g_mask_ = 0xFF;
        b_mask_ = 0xFF;
        a_mask_ = 0x00;
        break;

      case IMAGE_FORMAT_BGRA4444:
        size_ = 2;
        r_shift_ = 4;
        g_shift_ = 0;
        b_shift_ = -4;
        a_shift_ = 8;
        r_mask_ = 0xF0;
        g_mask_ = 0xF0;
        b_mask_ = 0xF0;
        a_mask_ = 0xF0;
        break;

      case IMAGE_FORMAT_BGR888:
        size_ = 3;
        r_shift_ = 16;
        g_shift_ = 8;
        b_shift_ = 0;
        a_shift_ = 0;
        r_mask_ = 0xFF;
        g_mask_ = 0xFF;
        b_mask_ = 0xFF;
        a_mask_ = 0x00;
        break;

      case IMAGE_FORMAT_BGR565:
        size_ = 2;
        r_shift_ = 8;
        g_shift_ = 3;
        b_shift_ = -3;
        a_shift_ = 0;
        r_mask_ = 0xF8;
        g_mask_ = 0xFC;
        b_mask_ = 0xF8;
        a_mask_ = 0x00;
        break;

      case IMAGE_FORMAT_BGRA5551:
      case IMAGE_FORMAT_BGRX5551:
        size_ = 2;
        r_shift_ = 7;
        g_shift_ = 2;
        b_shift_ = -3;
        a_shift_ = 8;
        r_mask_ = 0xF8;
        g_mask_ = 0xF8;
        b_mask_ = 0xF8;
        a_mask_ = 0x80;
        break;

      // GR - alpha format for HDR support.
      case IMAGE_FORMAT_A8:
        size_ = 1;
        r_shift_ = 0;
        g_shift_ = 0;
        b_shift_ = 0;
        a_shift_ = 0;
        r_mask_ = 0x00;
        g_mask_ = 0x00;
        b_mask_ = 0x00;
        a_mask_ = 0xFF;
        break;

      case IMAGE_FORMAT_UVWQ8888:
        size_ = 4;
        r_shift_ = 0;
        g_shift_ = 8;
        b_shift_ = 16;
        a_shift_ = 24;
        r_mask_ = 0xFF;
        g_mask_ = 0xFF;
        b_mask_ = 0xFF;
        a_mask_ = 0xFF;
        break;

      case IMAGE_FORMAT_RGBA16161616:
        size_ = 8;
        r_shift_ = 0;
        g_shift_ = 16;
        b_shift_ = 32;
        a_shift_ = 48;
        r_mask_ = 0xFFFF;
        g_mask_ = 0xFFFF;
        b_mask_ = 0xFFFF;
        a_mask_ = 0xFFFF;
        break;

      case IMAGE_FORMAT_I8:
        // whatever goes into R is considered the intensity.
        size_ = 1;
        r_shift_ = 0;
        g_shift_ = 0;
        b_shift_ = 0;
        a_shift_ = 0;
        r_mask_ = 0xFF;
        g_mask_ = 0x00;
        b_mask_ = 0x00;
        a_mask_ = 0x00;
        break;
      // TODO(d.rattman): Add more color formats as need arises.
      default: {
        static bool is_format_error_printed[NUM_IMAGE_FORMATS];
        if (!is_format_error_printed[format]) {
          Assert(0);
          Warning(
              "CPixelWriter::SetPixelMemory:  Unsupported image format %i\n",
              format);
          is_format_error_printed[format] = true;
        }
        // set to zero so that we don't stomp memory for formats that
        // we don't understand.
        size_ = 0;
      } break;
    }
  }
  SOURCE_FORCEINLINE void *GetPixelMemory() const { return base_; }

  // Sets where we're writing to.
  SOURCE_FORCEINLINE void Seek(i32 x, i32 y) {
    bits_ = base_ + y * bytes_per_row_ + x * size_;
  }
  // Skips n bytes:
  SOURCE_FORCEINLINE void *SkipBytes(i32 n) SOURCE_RESTRICT {
    bits_ += n;
    return bits_;
  }
  // Skips n pixels:
  SOURCE_FORCEINLINE void SkipPixels(i32 n) { SkipBytes(n * size_); }
  // Writes a pixel, advances the write index.
  SOURCE_FORCEINLINE void WritePixel(i32 r, i32 g, i32 b, i32 a = 255) {
    WritePixelNoAdvance(r, g, b, a);
    bits_ += size_;
  }
  // Writes a pixel without advancing the index.
  SOURCE_FORCEINLINE void WritePixelNoAdvance(i32 r, i32 g, i32 b,
                                              i32 a = 255) {
    Assert(!IsUsingFloatFormat());

    if (size_ <= 0) return;

    if (size_ < 5) {
      u32 val = (r & r_mask_) << r_shift_;
      val |= (g & g_mask_) << g_shift_;
      val |= (b_shift_ > 0) ? ((b & b_mask_) << b_shift_)
                            : ((b & b_mask_) >> -b_shift_);
      val |= (a & a_mask_) << a_shift_;

      switch (size_) {
        default:
          Assert(0);
          return;
        case 1: {
          bits_[0] = (u8)((val & 0xff));
          return;
        }
        case 2: {
          ((u16 *)bits_)[0] = (u16)((val & 0xffff));
          return;
        }
        case 3: {
          ((u16 *)bits_)[0] = (u16)((val & 0xffff));
          bits_[2] = (u8)((val >> 16) & 0xff);
          return;
        }
        case 4: {
          ((u32 *)bits_)[0] = val;
          return;
        }
      }
    } else  // RGBA32323232 or RGBA16161616 -- PC only.
    {
      i64 val = ((i64)(r & r_mask_)) << r_shift_;
      val |= ((i64)(g & g_mask_)) << g_shift_;
      val |= (b_shift_ > 0) ? (((i64)(b & b_mask_)) << b_shift_)
                            : (((i64)(b & b_mask_)) >> -b_shift_);
      val |= ((i64)(a & a_mask_)) << a_shift_;

      switch (size_) {
        case 6: {
          ((u32 *)bits_)[0] = val & 0xffffffff;
          ((u16 *)bits_)[2] = (u16)((val >> 32) & 0xffff);
          return;
        }
        case 8: {
          ((u32 *)bits_)[0] = val & 0xffffffff;
          ((u32 *)bits_)[1] = (val >> 32) & 0xffffffff;
          return;
        }
        default:
          Assert(0);
          return;
      }
    }
  }
  // Writes a pixel, advances the write index.
  SOURCE_FORCEINLINE void WritePixelSigned(i32 r, i32 g, i32 b, i32 a = 255) {
    WritePixelNoAdvanceSigned(r, g, b, a);
    bits_ += size_;
  }
  // Writes a signed pixel without advancing the index.
  SOURCE_FORCEINLINE void WritePixelNoAdvanceSigned(i32 r, i32 g, i32 b,
                                                    i32 a = 255) {
    Assert(!IsUsingFloatFormat());

    if (size_ <= 0) return;

    if (size_ < 5) {
      i32 val = (r & r_mask_) << r_shift_;
      val |= (g & g_mask_) << g_shift_;
      val |= (b_shift_ > 0) ? ((b & b_mask_) << b_shift_)
                            : ((b & b_mask_) >> -b_shift_);
      val |= (a & a_mask_) << a_shift_;
      i8 *pSignedBits = (i8 *)bits_;

      switch (size_) {
        case 4:
          pSignedBits[3] = (i8)((val >> 24) & 0xff);
          // fall through intentionally.
        case 3:
          pSignedBits[2] = (i8)((val >> 16) & 0xff);
          // fall through intentionally.
        case 2:
          pSignedBits[1] = (i8)((val >> 8) & 0xff);
          // fall through intentionally.
        case 1:
          pSignedBits[0] = (i8)((val & 0xff));
          // fall through intentionally.
          return;
      }
    } else {
      i64 val = ((i64)(r & r_mask_)) << r_shift_;
      val |= ((i64)(g & g_mask_)) << g_shift_;
      val |= (b_shift_ > 0) ? (((i64)(b & b_mask_)) << b_shift_)
                            : (((i64)(b & b_mask_)) >> -b_shift_);
      val |= ((i64)(a & a_mask_)) << a_shift_;
      i8 *pSignedBits = (i8 *)bits_;

      switch (size_) {
        case 8:
          pSignedBits[7] = (i8)((val >> 56) & 0xff);
          pSignedBits[6] = (i8)((val >> 48) & 0xff);
          // fall through intentionally.
        case 6:
          pSignedBits[5] = (i8)((val >> 40) & 0xff);
          pSignedBits[4] = (i8)((val >> 32) & 0xff);
          // fall through intentionally.
        case 4:
          pSignedBits[3] = (i8)((val >> 24) & 0xff);
          // fall through intentionally.
        case 3:
          pSignedBits[2] = (i8)((val >> 16) & 0xff);
          // fall through intentionally.
        case 2:
          pSignedBits[1] = (i8)((val >> 8) & 0xff);
          // fall through intentionally.
        case 1:
          pSignedBits[0] = (i8)((val & 0xff));
          break;
        default:
          Assert(0);
          return;
      }
    }
  }
  SOURCE_FORCEINLINE void ReadPixelNoAdvance(i32 &r, i32 &g, i32 &b,
                                             i32 &a) const {
    Assert(!IsUsingFloatFormat());

    i32 val = bits_[0];
    if (size_ > 1) {
      val |= (i32)bits_[1] << 8;
      if (size_ > 2) {
        val |= (i32)bits_[2] << 16;
        if (size_ > 3) {
          val |= (i32)bits_[3] << 24;
        }
      }
    }

    r = (val >> r_shift_) & r_mask_;
    g = (val >> g_shift_) & g_mask_;
    b = (val >> b_shift_) & b_mask_;
    a = (val >> a_shift_) & a_mask_;
  }

  // Floating point formats
  // Writes a pixel without advancing the index.
  SOURCE_FORCEINLINE void WritePixelNoAdvanceF(f32 r, f32 g, f32 b,
                                               f32 a = 1.0f) {
    Assert(IsUsingFloatFormat());

    if (PIXELWRITER_USING_16BIT_FLOAT_FORMAT & flags_) {
      float16 fp16[4];
      fp16[0].SetFloat(r);
      fp16[1].SetFloat(g);
      fp16[2].SetFloat(b);
      fp16[3].SetFloat(a);
      // fp16
      u16 pBuf[4] = {0, 0, 0, 0};
      pBuf[r_shift_ >> 4] |= (fp16[0].GetBits() & r_mask_) << (r_shift_ & 0xF);
      pBuf[g_shift_ >> 4] |= (fp16[1].GetBits() & g_mask_) << (g_shift_ & 0xF);
      pBuf[b_shift_ >> 4] |= (fp16[2].GetBits() & b_mask_) << (b_shift_ & 0xF);
      pBuf[a_shift_ >> 4] |= (fp16[3].GetBits() & a_mask_) << (a_shift_ & 0xF);
      memcpy(bits_, pBuf, size_);
    } else {
      // fp32
      i32 pBuf[4] = {0, 0, 0, 0};
      pBuf[r_shift_ >> 5] |= (FloatBits(r) & r_mask_) << (r_shift_ & 0x1F);
      pBuf[g_shift_ >> 5] |= (FloatBits(g) & g_mask_) << (g_shift_ & 0x1F);
      pBuf[b_shift_ >> 5] |= (FloatBits(b) & b_mask_) << (b_shift_ & 0x1F);
      pBuf[a_shift_ >> 5] |= (FloatBits(a) & a_mask_) << (a_shift_ & 0x1F);
      memcpy(bits_, pBuf, size_);
    }
  }
  // Writes a pixel, advances the write index.
  SOURCE_FORCEINLINE void WritePixelF(f32 r, f32 g, f32 b, f32 a = 1.0f) {
    WritePixelNoAdvanceF(r, g, b, a);
    bits_ += size_;
  }

  // SIMD formats
  SOURCE_FORCEINLINE void WritePixel(FLTX4 rgba);
  SOURCE_FORCEINLINE void WritePixelNoAdvance(FLTX4 rgba);

  SOURCE_FORCEINLINE u8 GetPixelSize() const { return size_; }

  SOURCE_FORCEINLINE bool IsUsingFloatFormat() const {
    return (flags_ & PIXELWRITER_USING_FLOAT_FORMAT) != 0;
  }
  SOURCE_FORCEINLINE u8 *GetCurrentPixel() const { return bits_; }

 private:
  enum : u8 {
    PIXELWRITER_USING_FLOAT_FORMAT = 0x01,
    PIXELWRITER_USING_16BIT_FLOAT_FORMAT = 0x02,
    PIXELWRITER_SWAPBYTES = 0x04,
  };

  u8 *base_;
  u8 *bits_;
  u16 bytes_per_row_;
  u8 size_;
  u8 flags_;

  i16 r_shift_, g_shift_, b_shift_, a_shift_;
  u32 r_mask_, g_mask_, b_mask_, a_mask_;
};

#endif  // PIXELWRITER_H
