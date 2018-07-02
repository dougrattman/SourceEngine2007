// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_VPLANE_H_
#define SOURCE_MATHLIB_VPLANE_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "mathlib/vector.h"

using SideType = int;

// Used to represent sides of things like planes.
#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2

#define VP_EPSILON 0.01f

class VPlane {
 public:
  VPlane() {}
  VPlane(const Vector &vNormal, f32 dist) : m_Normal{vNormal}, m_Dist{dist} {}

  void Init(const Vector &vNormal, f32 dist) {
    m_Normal = vNormal;
    m_Dist = dist;
  }

  // Return the distance from the point to the plane.
  f32 DistTo(const Vector &vVec) const { return vVec.Dot(m_Normal) - m_Dist; }

  // Copy.
  VPlane &operator=(const VPlane &thePlane) {
    m_Normal = thePlane.m_Normal;
    m_Dist = thePlane.m_Dist;
    return *this;
  }

  // Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK.
  // The epsilon for SIDE_ON can be passed in.
  SideType GetPointSide(const Vector &vPoint,
                        f32 sideEpsilon = VP_EPSILON) const {
    const f32 fDist = DistTo(vPoint);

    if (fDist >= sideEpsilon) return SIDE_FRONT;
    if (fDist <= -sideEpsilon) return SIDE_BACK;

    return SIDE_ON;
  }

  // Returns SIDE_FRONT or SIDE_BACK.
  SideType GetPointSideExact(const Vector &vPoint) const {
    return DistTo(vPoint) > 0.0f ? SIDE_FRONT : SIDE_BACK;
  }

  // Classify the box with respect to the plane.
  // Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK
  SideType BoxOnPlaneSideBoxOnPlaneSide(const Vector &vMin,
                                        const Vector &vMax) const {
    int i, firstSide, side;
    TableVector vPoints[8] = {
        {vMin.x, vMin.y, vMin.z}, {vMin.x, vMin.y, vMax.z},
        {vMin.x, vMax.y, vMax.z}, {vMin.x, vMax.y, vMin.z},

        {vMax.x, vMin.y, vMin.z}, {vMax.x, vMin.y, vMax.z},
        {vMax.x, vMax.y, vMax.z}, {vMax.x, vMax.y, vMin.z},
    };

    firstSide = GetPointSideExact(vPoints[0]);
    for (i = 1; i < 8; i++) {
      side = GetPointSideExact(vPoints[i]);

      // Does the box cross the plane?
      if (side != firstSide) return SIDE_ON;
    }

    // Ok, they're all on the same side, return that.
    return firstSide;
  }

#ifndef VECTOR_NO_SLOW_OPERATIONS
  // Flip the plane.
  VPlane Flip() { return VPlane{-m_Normal, -m_Dist}; }

  // Get a point on the plane (normal*dist).
  Vector GetPointOnPlane() const { return m_Normal * m_Dist; }

  // Snap the specified point to the plane (along the plane's normal).
  Vector SnapPointToPlane(const Vector &vPoint) const {
    return vPoint - m_Normal * DistTo(vPoint);
  }
#endif

 public:
  Vector m_Normal;
  f32 m_Dist;

#ifdef VECTOR_NO_SLOW_OPERATIONS
 private:
  // No copy constructors allowed if we're in optimal mode
  VPlane(const VPlane &vOther);
#else
  VPlane(const VPlane &vOther)
      : m_Normal{vOther.m_Normal}, m_Dist{vOther.m_Dist} {}
#endif
};

#endif  // SOURCE_MATHLIB_VPLANE_H_
