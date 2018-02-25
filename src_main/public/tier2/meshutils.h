// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A set of utilities to help with generating meshes.

#ifndef SOURCE_TIER2_MESHUTILS_H_
#define SOURCE_TIER2_MESHUTILS_H_


// Helper methods to create various standard index buffer types

void GenerateSequentialIndexBuffer(unsigned short* pIndexMemory,
                                   int nIndexCount, int nFirstVertex);
void GenerateQuadIndexBuffer(unsigned short* pIndexMemory, int nIndexCount,
                             int nFirstVertex);
void GeneratePolygonIndexBuffer(unsigned short* pIndexMemory, int nIndexCount,
                                int nFirstVertex);
void GenerateLineStripIndexBuffer(unsigned short* pIndexMemory, int nIndexCount,
                                  int nFirstVertex);
void GenerateLineLoopIndexBuffer(unsigned short* pIndexMemory, int nIndexCount,
                                 int nFirstVertex);

#endif  // SOURCE_TIER2_MESHUTILS_H_
