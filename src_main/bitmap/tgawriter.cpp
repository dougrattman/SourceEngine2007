// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bitmap/tgawriter.h"

#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include "bitmap/imageformat.h"
#include "filesystem.h"
#include "tier0/include/dbg.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"
#include "tier2/tier2.h"

#include "tier0/include/memdbgon.h"

namespace TGAWriter {

#pragma pack(1)
struct TGAHeader_t {
  u8 id_length;
  u8 colormap_type;
  u8 image_type;
  u16 colormap_index;
  u16 colormap_length;
  u8 colormap_size;
  u16 x_origin;
  u16 y_origin;
  u16 width;
  u16 height;
  u8 pixel_size;
  u8 attributes;
};
#pragma pack()

#define fputc myfputc
#define fwrite myfwrite

static void fputLittleShort(u16 s, CUtlBuffer &buffer) {
  buffer.PutChar(s & 0xff);
  buffer.PutChar(s >> 8);
}

static inline void myfputc(u8 c, FileHandle_t fileHandle) {
  g_pFullFileSystem->Write(&c, 1, fileHandle);
}

static inline void myfwrite(void const *data, int size1, int size2,
                            FileHandle_t fileHandle) {
  g_pFullFileSystem->Write(data, size1 * size2, fileHandle);
}

// TODO(d.rattman): assumes that we don't need to do gamma correction.

bool WriteToBuffer(u8 *pImageData, CUtlBuffer &buffer, int width, int height,
                   ImageFormat srcFormat, ImageFormat dstFormat) {
  TGAHeader_t header;

  // Fix the dstFormat to match what actually is going to go into the file
  switch (dstFormat) {
    case IMAGE_FORMAT_RGB888:
      dstFormat = IMAGE_FORMAT_BGR888;
      break;
    case IMAGE_FORMAT_RGBA8888:
      dstFormat = IMAGE_FORMAT_BGRA8888;
      break;
  }

  header.id_length = 0;      // comment length
  header.colormap_type = 0;  // ???

  switch (dstFormat) {
    case IMAGE_FORMAT_BGR888:
      header.image_type = 2;  // 24/32 bit uncompressed TGA
      header.pixel_size = 24;
      break;
    case IMAGE_FORMAT_BGRA8888:
      header.image_type = 2;  // 24/32 bit uncompressed TGA
      header.pixel_size = 32;
      break;
    case IMAGE_FORMAT_I8:
      header.image_type = 1;  // 8 bit uncompressed TGA
      header.pixel_size = 8;
      break;
    default:
      return false;
      break;
  }

  header.colormap_index = 0;
  header.colormap_length = 0;
  header.colormap_size = 0;
  header.x_origin = 0;
  header.y_origin = 0;
  header.width = (u16)width;
  header.height = (u16)height;
  // Makes it so we don't have to vertically flip the image
  header.attributes = 0x20;

  buffer.PutChar(header.id_length);
  buffer.PutChar(header.colormap_type);
  buffer.PutChar(header.image_type);
  fputLittleShort(header.colormap_index, buffer);
  fputLittleShort(header.colormap_length, buffer);
  buffer.PutChar(header.colormap_size);
  fputLittleShort(header.x_origin, buffer);
  fputLittleShort(header.y_origin, buffer);
  fputLittleShort(header.width, buffer);
  fputLittleShort(header.height, buffer);
  buffer.PutChar(header.pixel_size);
  buffer.PutChar(header.attributes);

  int nSizeInBytes = width * height * ImageLoader::SizeInBytes(dstFormat);
  buffer.EnsureCapacity(buffer.TellPut() + nSizeInBytes);
  u8 *pDst = (u8 *)buffer.PeekPut();

  if (!ImageLoader::ConvertImageFormat(pImageData, srcFormat, pDst, dstFormat,
                                       width, height))
    return false;

  buffer.SeekPut(CUtlBuffer::SEEK_CURRENT, nSizeInBytes);
  return true;
}

bool WriteDummyFileNoAlloc(const char *fileName, int width, int height,
                           enum ImageFormat dstFormat) {
  TGAHeader_t tgaHeader;

  Assert(g_pFullFileSystem);
  if (!g_pFullFileSystem) {
    return false;
  }
  COutputFile fp(fileName);

  int nBytesPerPixel, nImageType, nPixelSize;

  switch (dstFormat) {
    case IMAGE_FORMAT_BGR888:
      nBytesPerPixel = 3;  // 24/32 bit uncompressed TGA
      nPixelSize = 24;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_BGRA8888:
      nBytesPerPixel = 4;  // 24/32 bit uncompressed TGA
      nPixelSize = 32;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_I8:
      nBytesPerPixel = 1;  // 8 bit uncompressed TGA
      nPixelSize = 8;
      nImageType = 1;
      break;
    default:
      return false;
      break;
  }

  memset(&tgaHeader, 0, sizeof(tgaHeader));
  tgaHeader.id_length = 0;
  tgaHeader.image_type = (u8)nImageType;
  tgaHeader.width = (u16)width;
  tgaHeader.height = (u16)height;
  tgaHeader.pixel_size = (u8)nPixelSize;
  tgaHeader.attributes = 0x20;

  // Write the Targa header
  fp.Write(&tgaHeader, sizeof(TGAHeader_t));

  // Write out width * height black pixels
  u8 black[4] = {0x1E, 0x9A, 0xFF, 0x00};
  for (int i = 0; i < width * height; i++) {
    fp.Write(black, nBytesPerPixel);
  }

  return true;
}

bool WriteTGAFile(const char *fileName, int width, int height,
                  enum ImageFormat srcFormat, u8 const *srcData, int nStride) {
  TGAHeader_t tgaHeader;

  COutputFile fp(fileName);

  int nBytesPerPixel, nImageType, nPixelSize;

  bool bMustConvert = false;
  ImageFormat dstFormat = srcFormat;

  switch (srcFormat) {
    case IMAGE_FORMAT_BGR888:
      nBytesPerPixel = 3;  // 24/32 bit uncompressed TGA
      nPixelSize = 24;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_BGRA8888:
      nBytesPerPixel = 4;  // 24/32 bit uncompressed TGA
      nPixelSize = 32;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_RGBA8888:
      bMustConvert = true;
      dstFormat = IMAGE_FORMAT_BGRA8888;
      nBytesPerPixel = 4;  // 24/32 bit uncompressed TGA
      nPixelSize = 32;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_I8:
      nBytesPerPixel = 1;  // 8 bit uncompressed TGA
      nPixelSize = 8;
      nImageType = 1;
      break;
    default:
      return false;
      break;
  }

  memset(&tgaHeader, 0, sizeof(tgaHeader));
  tgaHeader.id_length = 0;
  tgaHeader.image_type = (u8)nImageType;
  tgaHeader.width = (u16)width;
  tgaHeader.height = (u16)height;
  tgaHeader.pixel_size = (u8)nPixelSize;
  tgaHeader.attributes = 0x20;

  // Write the Targa header
  fp.Write(&tgaHeader, sizeof(TGAHeader_t));

  // Write out image data
  if (bMustConvert) {
    u8 *pLineBuf = new u8[nBytesPerPixel * width];
    while (height--) {
      ImageLoader::ConvertImageFormat(srcData, srcFormat, pLineBuf, dstFormat,
                                      width, 1);
      fp.Write(pLineBuf, nBytesPerPixel * width);
      srcData += nStride;
    }
    delete[] pLineBuf;
  } else {
    while (height--) {
      fp.Write(srcData, nBytesPerPixel * width);
      srcData += nStride;
    }
  }
  return true;
}

bool WriteRectNoAlloc(u8 *pImageData, const char *fileName, int nXOrigin,
                      int nYOrigin, int width, int height, int nStride,
                      enum ImageFormat srcFormat) {
  Assert(g_pFullFileSystem);
  if (!g_pFullFileSystem) {
    return false;
  }
  FileHandle_t fp;
  fp = g_pFullFileSystem->Open(fileName, "r+b");

  //
  // Read in the targa header
  //
  TGAHeader_t tgaHeader;
  g_pFullFileSystem->Read(&tgaHeader, sizeof(tgaHeader), fp);

  int nBytesPerPixel, nImageType, nPixelSize;

  switch (srcFormat) {
    case IMAGE_FORMAT_BGR888:
      nBytesPerPixel = 3;  // 24/32 bit uncompressed TGA
      nPixelSize = 24;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_BGRA8888:
      nBytesPerPixel = 4;  // 24/32 bit uncompressed TGA
      nPixelSize = 32;
      nImageType = 2;
      break;
    case IMAGE_FORMAT_I8:
      nBytesPerPixel = 1;  // 8 bit uncompressed TGA
      nPixelSize = 8;
      nImageType = 1;
      break;
    default:
      return false;
      break;
  }

  // Verify src data matches the targa we're going to write into
  if (nPixelSize != tgaHeader.pixel_size) {
    Warning("TGA doesn't match source data.\n");
    return false;
  }

  // Seek to the origin of the target subrect from the beginning of the file
  g_pFullFileSystem->Seek(
      fp, nBytesPerPixel * (tgaHeader.width * nYOrigin + nXOrigin),
      FILESYSTEM_SEEK_CURRENT);

  u8 *pSrc = pImageData;

  // Run through each scanline of the incoming rect
  for (int row = 0; row < height; row++) {
    g_pFullFileSystem->Write(pSrc, nBytesPerPixel * width, fp);

    // Advance src pointer to next scanline
    pSrc += nBytesPerPixel * nStride;

    // Seek ahead in the file
    g_pFullFileSystem->Seek(fp, nBytesPerPixel * (tgaHeader.width - width),
                            FILESYSTEM_SEEK_CURRENT);
  }

  g_pFullFileSystem->Close(fp);
  return true;
}

}  // namespace TGAWriter
