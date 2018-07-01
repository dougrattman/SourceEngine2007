// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef TGALOADER_H
#define TGALOADER_H

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"  // ImageFormat.
#include "tier1/utlmemory.h"

class CUtlBuffer;

namespace TGALoader {
int TGAHeaderSize();

bool GetInfo(const ch *fileName, int *width, int *height,
             ImageFormat *imageFormat, f32 *sourceGamma);
bool GetInfo(CUtlBuffer &buf, int *width, int *height, ImageFormat *imageFormat,
             f32 *sourceGamma);

bool Load(u8 *imageData, const ch *fileName, int width, int height,
          ImageFormat imageFormat, f32 targetGamma, bool mipmap);
bool Load(u8 *imageData, CUtlBuffer &buf, int width, int height,
          ImageFormat imageFormat, f32 targetGamma, bool mipmap);

bool LoadRGBA8888(const ch *pFileName, CUtlMemory<u8> &outputData,
                  int &outWidth, int &outHeight);
bool LoadRGBA8888(CUtlBuffer &buf, CUtlMemory<u8> &outputData, int &outWidth,
                  int &outHeight);
}  // namespace TGALoader

#endif  // TGALOADER_H
