// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_POLYHEDRON_H_
#define SOURCE_MATHLIB_POLYHEDRON_H_

#include "base/include/base_types.h"
#include "mathlib/mathlib.h"

struct Polyhedron_IndexedLine_t {
  u16 iPointIndices[2];
};

struct Polyhedron_IndexedLineReference_t {
  u16 iLineIndex;
  u8 iEndPointIndex;  // since two polygons reference any one line,
                      // one needs to traverse the line backwards,
                      // this flags that behavior
};

struct Polyhedron_IndexedPolygon_t {
  u16 iFirstIndex;
  u16 iIndexCount;
  Vector polyNormal;
};

// made into a class because it's going virtual to support
// distinctions between temp and permanent versions
class CPolyhedron {
 public:
  Vector *pVertices;
  Polyhedron_IndexedLine_t *pLines;
  Polyhedron_IndexedLineReference_t *pIndices;
  Polyhedron_IndexedPolygon_t *pPolygons;

  u16 iVertexCount;
  u16 iLineCount;
  u16 iIndexCount;
  u16 iPolygonCount;

  virtual ~CPolyhedron(void){};
  virtual void Release(void) = 0;
  Vector Center(void);
};

class CPolyhedron_AllocByNew : public CPolyhedron {
 public:
  virtual void Release(void);
  static CPolyhedron_AllocByNew *Allocate(
      u16 iVertices, u16 iLines, u16 iIndices,
      u16 iPolygons);  // creates the polyhedron along with enough
                       // memory to hold all it's data in a single
                       // allocation

 private:
  CPolyhedron_AllocByNew(void){};  // CPolyhedron_AllocByNew::Allocate() is the
                                   // only way to create one of these.
};

CPolyhedron *GeneratePolyhedronFromPlanes(
    const f32 *pOutwardFacingPlanes, int iPlaneCount, f32 fOnPlaneEpsilon,
    bool bUseTemporaryMemory = false);  // be sure to polyhedron->Release()
CPolyhedron *ClipPolyhedron(
    const CPolyhedron *pExistingPolyhedron, const f32 *pOutwardFacingPlanes,
    int iPlaneCount, f32 fOnPlaneEpsilon,
    bool bUseTemporaryMemory =
        false);  // this does NOT modify/delete the existing polyhedron

// grab the temporary polyhedron. Avoids new/delete for quick work. Can only be
// in use by one chunk of code at a time
CPolyhedron *GetTempPolyhedron(u16 iVertices, u16 iLines, u16 iIndices,
                               u16 iPolygons);

#endif  // SOURCE_MATHLIB_POLYHEDRON_H_
