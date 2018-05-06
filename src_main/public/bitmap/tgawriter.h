// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef TGAWRITER_H
#define TGAWRITER_H

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"  // ImageFormat.
#include "tier1/interface.h"

class CUtlBuffer;

namespace TGAWriter {
bool WriteToBuffer(u8 *pImageData, CUtlBuffer &buffer, int width, int height,
                   ImageFormat srcFormat, ImageFormat dstFormat);

// write out a simple tga file from a memory buffer.
bool WriteTGAFile(const ch *fileName, int width, int height,
                  enum ImageFormat srcFormat, u8 const *srcData, int nStride);

// A pair of routines for writing to files without allocating any memory in the
// TGA writer Useful for very large files such as posters, which are rendered as
// sub-rects anyway
bool WriteDummyFileNoAlloc(const ch *fileName, int width, int height,
                           ImageFormat dstFormat);
bool WriteRectNoAlloc(u8 *pImageData, const ch *fileName, int nXOrigin,
                      int nYOrigin, int width, int height, int nStride,
                      ImageFormat srcFormat);

}  // namespace TGAWriter

#endif  // TGAWRITER_H
