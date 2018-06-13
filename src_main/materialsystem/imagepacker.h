// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATERIALSYSTEM_IMAGEPACKER_H_
#define SOURCE_MATERIALSYSTEM_IMAGEPACKER_H_

#include "tier1/utlrbtree.h"
#include "tier1/utlvector.h"

#define MAX_MAX_LIGHTMAP_WIDTH 2048

// This packs a single lightmap
class CImagePacker {
 public:
  bool Reset(int nSortId, int maxLightmapWidth, int maxLightmapHeight);
  bool AddBlock(int width, int height, int *returnX, int *returnY);
  void GetMinimumDimensions(int *returnWidth, int *returnHeight);
  float GetEfficiency();

  int GetSortId() const { return m_nSortID; }
  void IncrementSortId() { ++m_nSortID; }

 protected:
  int GetMaxYIndex(int firstX, int width);

  int m_MaxLightmapWidth;
  int m_MaxLightmapHeight;
  int m_pLightmapWavefront[MAX_MAX_LIGHTMAP_WIDTH];
  int m_AreaUsed;
  int m_MinimumHeight;

  // For optimization purposes:
  // These store the width + height of the first image
  // that was unable to be stored in this image
  int m_MaxBlockWidth;
  int m_MaxBlockHeight;
  int m_nSortID;
};

#endif  // SOURCE_MATERIALSYSTEM_IMAGEPACKER_H_
