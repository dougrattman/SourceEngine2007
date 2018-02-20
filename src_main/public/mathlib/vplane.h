// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_VPLANE_H_
#define SOURCE_MATHLIB_VPLANE_H_

#include "base/include/base_types.h"
#include "mathlib/vector.h"

typedef int SideType;

// Used to represent sides of things like planes.
#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2

#define VP_EPSILON 0.01f

class VPlane {
 public:
  VPlane();
  VPlane(const Vector &vNormal, f32 dist);

  void Init(const Vector &vNormal, f32 dist);

  // Return the distance from the point to the plane.
  f32 DistTo(const Vector &vVec) const;

  // Copy.
  VPlane &operator=(const VPlane &thePlane);

  // Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK.
  // The epsilon for SIDE_ON can be passed in.
  SideType GetPointSide(const Vector &vPoint,
                        f32 sideEpsilon = VP_EPSILON) const;

  // Returns SIDE_FRONT or SIDE_BACK.
  SideType GetPointSideExact(const Vector &vPoint) const;

  // Classify the box with respect to the plane.
  // Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK
  SideType BoxOnPlaneSide(const Vector &vMin, const Vector &vMax) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
  // Flip the plane.
  VPlane Flip();

  // Get a point on the plane (normal*dist).
  Vector GetPointOnPlane() const;

  // Snap the specified point to the plane (along the plane's normal).
  Vector SnapPointToPlane(const Vector &vPoint) const;
#endif

 public:
  Vector m_Normal;
  f32 m_Dist;

#ifdef VECTOR_NO_SLOW_OPERATIONS
 private:
  // No copy constructors allowed if we're in optimal mode
  VPlane(const VPlane &vOther);
#endif
};

//-----------------------------------------------------------------------------
// Inlines.
//-----------------------------------------------------------------------------
inline VPlane::VPlane() {}

inline VPlane::VPlane(const Vector &vNormal, f32 dist)
    : m_Normal{vNormal}, m_Dist{dist} {}

inline void VPlane::Init(const Vector &vNormal, f32 dist) {
  m_Normal = vNormal;
  m_Dist = dist;
}

inline f32 VPlane::DistTo(const Vector &vVec) const {
  return vVec.Dot(m_Normal) - m_Dist;
}

inline VPlane &VPlane::operator=(const VPlane &thePlane) {
  m_Normal = thePlane.m_Normal;
  m_Dist = thePlane.m_Dist;
  return *this;
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VPlane VPlane::Flip() { return VPlane(-m_Normal, -m_Dist); }

inline Vector VPlane::GetPointOnPlane() const { return m_Normal * m_Dist; }

inline Vector VPlane::SnapPointToPlane(const Vector &vPoint) const {
  return vPoint - m_Normal * DistTo(vPoint);
}

#endif

inline SideType VPlane::GetPointSide(const Vector &vPoint,
                                     f32 sideEpsilon) const {
  f32 fDist;

  fDist = DistTo(vPoint);
  if (fDist >= sideEpsilon)
    return SIDE_FRONT;
  else if (fDist <= -sideEpsilon)
    return SIDE_BACK;
  else
    return SIDE_ON;
}

inline SideType VPlane::GetPointSideExact(const Vector &vPoint) const {
  return DistTo(vPoint) > 0.0f ? SIDE_FRONT : SIDE_BACK;
}

// BUGBUG: This should either simply use the implementation in mathlib or cease
// to exist. mathlib implementation is much more efficient.  Check to see that
// VPlane isn't used in performance critical code.
inline SideType VPlane::BoxOnPlaneSide(const Vector &vMin,
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

#endif  // SOURCE_MATHLIB_VPLANE_H_
