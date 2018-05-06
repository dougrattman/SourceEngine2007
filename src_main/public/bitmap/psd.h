// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Methods relating to saving + loading PSD files (photoshop).

#ifndef PSD_H
#define PSD_H

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"  // ImageFormat.

class CUtlBuffer;
struct Bitmap_t;

class PSDImageResources {
 public:
  enum Resource { eResFileInfo = 0x0404 };

  struct ResElement {
    Resource m_eType;
    u16 m_numBytes;
    u8 const *m_pvData;
  };

 public:
  explicit PSDImageResources(u32 numBytes, u8 const *pvBuffer)
      : m_numBytes(numBytes), m_pvBuffer(pvBuffer) {}

 public:
  ResElement FindElement(Resource eType) const;

 protected:
  u32 m_numBytes;
  u8 const *m_pvBuffer;
};

class PSDResFileInfo {
 public:
  enum ResFileInfo {
    eTitle = 0x05,
    eAuthor = 0x50,
    eAuthorTitle = 0x55,
    eDescription = 0x78,
    eDescriptionWriter = 0x7A,
    eKeywords = 0x19,
    eCopyrightNotice = 0x74
  };

  struct ResFileInfoElement {
    ResFileInfo m_eType;
    u16 m_numBytes;
    u8 const *m_pvData;
  };

 public:
  explicit PSDResFileInfo(PSDImageResources::ResElement res) : m_res(res) {}

 public:
  ResFileInfoElement FindElement(ResFileInfo eType) const;

 protected:
  PSDImageResources::ResElement m_res;
};

// Is a file a PSD file?
bool IsPSDFile(const ch *pFileName, const ch *pPathID);
bool IsPSDFile(CUtlBuffer &buf);

// Returns information about the PSD file.
bool PSDGetInfo(const ch *pFileName, const ch *pPathID, int *pWidth,
                int *pHeight, ImageFormat *pImageFormat, f32 *pSourceGamma);
bool PSDGetInfo(CUtlBuffer &buf, int *pWidth, int *pHeight,
                ImageFormat *pImageFormat, f32 *pSourceGamma);

// Get PSD file image resources, pointers refer into the utlbuffer.
PSDImageResources PSDGetImageResources(CUtlBuffer &buf);

// Reads the PSD file into the specified buffer.
bool PSDReadFileRGBA8888(CUtlBuffer &buf, Bitmap_t &bitmap);
bool PSDReadFileRGBA8888(const ch *pFileName, const ch *pPathID,
                         Bitmap_t &bitmap);

#endif  // PSD_H
