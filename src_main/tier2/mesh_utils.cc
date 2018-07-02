// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A set of utilities to render standard shapes.

#include "tier2/meshutils.h"

// Helper methods to create various standard index buffer types

void GenerateSequentialIndexBuffer(u16* pIndices, usize nIndexCount,
                                   int nFirstVertex) {
  if (!pIndices) return;

  // Format the sequential buffer
  for (usize i = 0; i < nIndexCount; ++i) {
    pIndices[i] = (u16)(i + nFirstVertex);
  }
}

void GenerateQuadIndexBuffer(u16* pIndices, usize nIndexCount,
                             int nFirstVertex) {
  if (!pIndices) return;

  // Format the quad buffer
  usize numQuads = nIndexCount / 6;
  u16 baseVertex = nFirstVertex;
  for (usize i = 0; i < numQuads; ++i) {
    // Triangle 1
    pIndices[0] = baseVertex;
    pIndices[1] = (u16)(baseVertex + 1);
    pIndices[2] = (u16)(baseVertex + 2);

    // Triangle 2
    pIndices[3] = baseVertex;
    pIndices[4] = (u16)(baseVertex + 2);
    pIndices[5] = (u16)(baseVertex + 3);

    baseVertex += 4;
    pIndices += 6;
  }
}

void GeneratePolygonIndexBuffer(u16* pIndices, usize nIndexCount,
                                int nFirstVertex) {
  if (!pIndices) return;

  usize numPolygons = nIndexCount / 3;
  for (usize i = 0; i < numPolygons; ++i) {
    // Triangle 1
    pIndices[0] = (u16)(nFirstVertex);
    pIndices[1] = (u16)(nFirstVertex + i + 1);
    pIndices[2] = (u16)(nFirstVertex + i + 2);

    pIndices += 3;
  }
}

void GenerateLineStripIndexBuffer(u16* pIndices, usize nIndexCount,
                                  int nFirstVertex) {
  if (!pIndices) return;

  usize numLines = nIndexCount / 2;
  for (usize i = 0; i < numLines; ++i) {
    pIndices[0] = (u16)(nFirstVertex + i);
    pIndices[1] = (u16)(nFirstVertex + i + 1);

    pIndices += 2;
  }
}

void GenerateLineLoopIndexBuffer(u16* pIndices, usize nIndexCount,
                                 int nFirstVertex) {
  if (!pIndices) return;

  usize numLines = nIndexCount / 2;

  pIndices[0] = (u16)(nFirstVertex + numLines - 1);
  pIndices[1] = (u16)(nFirstVertex);
  pIndices += 2;

  for (usize i = 1; i < numLines; ++i) {
    pIndices[0] = (u16)(nFirstVertex + i - 1);
    pIndices[1] = (u16)(nFirstVertex + i);

    pIndices += 2;
  }
}
