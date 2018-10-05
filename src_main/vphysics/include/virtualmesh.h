// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VPHYSICS_INCLUDE_VIRTUALMESH_H_
#define SOURCE_VPHYSICS_INCLUDE_VIRTUALMESH_H_

#include "base/include/compiler_specific.h"
#include "base/include/base_types.h"
#include "mathlib/vector.h"

#ifdef _WIN32
#pragma once
#endif

// NOTE: These are fixed length to make it easy to fill these out without memory
// allocation or storage
constexpr inline int MAX_VIRTUAL_TRIANGLES{1024};

struct virtualmeshlist_t {
  Vector *pVerts;
  int indexCount;
  int triangleCount;
  int vertexCount;
  int surfacePropsIndex;
  u8 *pHull;
  u16 indices[MAX_VIRTUAL_TRIANGLES * 3];
};

struct virtualmeshtrianglelist_t {
  int triangleCount;
  u16 triangleIndices[MAX_VIRTUAL_TRIANGLES * 3];
};

src_interface IVirtualMeshEvent {
  virtual void GetVirtualMesh(void *userData, virtualmeshlist_t *pList) = 0;
  virtual void GetWorldspaceBounds(void *userData, Vector *pMins,
                                   Vector *pMaxs) = 0;
  virtual void GetTrianglesInSphere(void *userData, const Vector &center,
                                    float radius,
                                    virtualmeshtrianglelist_t *pList) = 0;
};

struct virtualmeshparams_t {
  IVirtualMeshEvent *pMeshEventHandler;
  void *userData;
  bool buildOuterHull;
};

#endif  // SOURCE_VPHYSICS_INCLUDE_VIRTUALMESH_H_
