// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Image Byte Swapping. Isolate routines to own module to allow
// librarian to ignore xbox 360 dependenices in non-applicable win32 projects.

#if defined(_WIN32)
#include "base/include/windows/windows_light.h"
#endif

#include "bitmap/imageformat.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

// Should be last include
#include "tier0/include/memdbgon.h"

namespace ImageLoader {

// Known formats that can be converted.  Used as a trap for 360 formats
// that may occur but have not been validated yet.

bool IsFormatValidForConversion(ImageFormat fmt) {
  switch (fmt) {
    case IMAGE_FORMAT_RGBA8888:
    case IMAGE_FORMAT_ABGR8888:
    case IMAGE_FORMAT_RGB888:
    case IMAGE_FORMAT_BGR888:
    case IMAGE_FORMAT_ARGB8888:
    case IMAGE_FORMAT_BGRA8888:
    case IMAGE_FORMAT_BGRX8888:
    case IMAGE_FORMAT_UVWQ8888:
    case IMAGE_FORMAT_RGBA16161616F:
    case IMAGE_FORMAT_RGBA16161616:
    case IMAGE_FORMAT_UVLX8888:
    case IMAGE_FORMAT_DXT1:
    case IMAGE_FORMAT_DXT1_ONEBITALPHA:
    case IMAGE_FORMAT_DXT3:
    case IMAGE_FORMAT_DXT5:
    case IMAGE_FORMAT_UV88:
      return true;

      // untested formats
    default:
    case IMAGE_FORMAT_RGB565:
    case IMAGE_FORMAT_I8:
    case IMAGE_FORMAT_IA88:
    case IMAGE_FORMAT_A8:
    case IMAGE_FORMAT_RGB888_BLUESCREEN:
    case IMAGE_FORMAT_BGR888_BLUESCREEN:
    case IMAGE_FORMAT_BGR565:
    case IMAGE_FORMAT_BGRX5551:
    case IMAGE_FORMAT_BGRA4444:
    case IMAGE_FORMAT_BGRA5551:
    case IMAGE_FORMAT_ATI1N:
    case IMAGE_FORMAT_ATI2N:
      break;
  }

  return false;
}


// Swaps the image element type within the format.
// This is to ensure that >8 bit channels are in the correct endian format
// as expected by the conversion process, which varies according to format,
// input, and output.

void PreConvertSwapImageData(unsigned char *pImageData, int nImageSize,
                             ImageFormat imageFormat, int width, int stride) {
  Assert(IsFormatValidForConversion(imageFormat));

  // running as a win32 tool, data is in expected order
  // for conversion code
}


// Swaps image bytes for use on a big endian platform. This is used after the
// conversion process to match the 360 d3dformats.

void PostConvertSwapImageData(unsigned char *pImageData, int nImageSize,
                              ImageFormat imageFormat, int width, int stride) {
  Assert(IsFormatValidForConversion(imageFormat));
  AssertMsg(false, "PostConvertSwapImageData is not implemented for PC.");
}


// Swaps image bytes.

void ByteSwapImageData(unsigned char *pImageData, int nImageSize,
                       ImageFormat imageFormat, int width, int stride) {
  Assert(IsFormatValidForConversion(imageFormat));
  AssertMsg(false, "ByteSwapImageData is not implemented for PC.");
}

}  // namespace ImageLoader
