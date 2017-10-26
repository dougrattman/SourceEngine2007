// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef TGALOADER_H
#define TGALOADER_H

#include "bitmap/imageformat.h"
#include "tier1/utlmemory.h"

class CUtlBuffer;

namespace TGALoader {
int TGAHeaderSize();

bool GetInfo(const char *fileName, int *width, int *height,
             ImageFormat *imageFormat, float *sourceGamma);
bool GetInfo(CUtlBuffer &buf, int *width, int *height, ImageFormat *imageFormat,
             float *sourceGamma);

bool Load(unsigned char *imageData, const char *fileName, int width, int height,
          ImageFormat imageFormat, float targetGamma, bool mipmap);
bool Load(unsigned char *imageData, CUtlBuffer &buf, int width, int height,
          ImageFormat imageFormat, float targetGamma, bool mipmap);

bool LoadRGBA8888(const char *pFileName, CUtlMemory<unsigned char> &outputData,
                  int &outWidth, int &outHeight);
bool LoadRGBA8888(CUtlBuffer &buf, CUtlMemory<unsigned char> &outputData,
                  int &outWidth, int &outHeight);

}  // namespace TGALoader

#endif  // TGALOADER_H
