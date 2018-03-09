// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A Scene Image file aggregates all the compiled binary VCD files into
// a single file.

#ifndef SCENE_IMAGE_FILE_H
#define SCENE_IMAGE_FILE_H

#include "base/include/macros.h"
#include "tier1/checksum_crc.h"

#define SCENE_IMAGE_ID MAKEID<int32_t>('V', 'S', 'I', 'F')
#define SCENE_IMAGE_VERSION 2

// scene summary: cached calcs for commmon startup queries, variable sized
struct SceneImageSummary_t {
  unsigned int msecs;
  int numSounds;
  int soundStrings[1];  // has numSounds
};

// stored sorted by crc filename for binary search
struct SceneImageEntry_t {
  CRC32_t crcFilename;  // expected to be normalized as scenes\???.vcd
  int nDataOffset;      // offset to dword aligned data from start
  int nDataLength;
  int nSceneSummaryOffset;  // offset to summary
};

struct SceneImageHeader_t {
  int nId;
  int nVersion;
  int nNumScenes;   // number of scene files
  int nNumStrings;  // number of unique strings in table
  int nSceneEntryOffset;

  inline const char *String(short iString) {
    if (iString < 0 || iString >= nNumStrings) {
      Assert(0);
      return NULL;
    }

    // access string table (after header) to access pool
    unsigned int *pTable =
        (unsigned int *)((uint8_t *)this + sizeof(SceneImageHeader_t));
    return (char *)this + pTable[iString];
  }
};

#endif  // SCENE_IMAGE_FILE_H
