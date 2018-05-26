// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A set of utilities to help with generating meshes.

#ifndef SOURCE_TIER2_MESHUTILS_H_
#define SOURCE_TIER2_MESHUTILS_H_

#include "base/include/base_types.h"

// Helper methods to create various standard index buffer types
void GenerateSequentialIndexBuffer(u16* pIndexMemory, int nIndexCount,
                                   int nFirstVertex);
void GenerateQuadIndexBuffer(u16* pIndexMemory, int nIndexCount,
                             int nFirstVertex);
void GeneratePolygonIndexBuffer(u16* pIndexMemory, int nIndexCount,
                                int nFirstVertex);
void GenerateLineStripIndexBuffer(u16* pIndexMemory, int nIndexCount,
                                  int nFirstVertex);
void GenerateLineLoopIndexBuffer(u16* pIndexMemory, int nIndexCount,
                                 int nFirstVertex);

#endif  // SOURCE_TIER2_MESHUTILS_H_
