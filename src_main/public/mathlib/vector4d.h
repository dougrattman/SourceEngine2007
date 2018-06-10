// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_VECTOR4D_H_
#define SOURCE_MATHLIB_VECTOR4D_H_

#include <cfloat>
#include <cmath>
#include <cstdlib>  // std::rand.

#include "build/include/build_config.h"

#if defined(COMPILER_MSVC) && defined(ARCH_CPU_X86)
#include <xmmintrin.h>  // SSE* intrinsics.
#endif

#include "base/include/base_types.h"
#include "tier0/include/dbg.h"
#include "tier0/include/floattypes.h"

#include "mathlib/math_pfns.h"

class Vector;
class Vector2D;
class Vector4D;

inline f32 Vector4DLength(Vector4D const& v);
inline void Vector4DSubtract(Vector4D const& a, Vector4D const& b, Vector4D& c);
inline f32 DotProduct4D(Vector4D const& a, Vector4D const& b);

// 4D vector.
class Vector4D {
 public:
  // Members
  f32 x, y, z, w;

  // Construction/destruction
  Vector4D() {
#ifndef NDEBUG
    // Initialize to NAN to catch errors
    x = y = z = w = FLOAT32_NAN;
#endif
  }
  Vector4D(f32 X, f32 Y, f32 Z, f32 W) {
    x = X;
    y = Y;
    z = Z;
    w = W;
    Assert(IsValid());
  }
  Vector4D(const f32* pFloat) {
    Assert(pFloat);
    x = pFloat[0];
    y = pFloat[1];
    z = pFloat[2];
    w = pFloat[3];
    Assert(IsValid());
  }

  // Initialization
  void Init(f32 ix = 0.0f, f32 iy = 0.0f, f32 iz = 0.0f, f32 iw = 0.0f) {
    x = ix;
    y = iy;
    z = iz;
    w = iw;
    Assert(IsValid());
  }

  // Got any nasty NAN's?
  bool IsValid() const {
    return IsFinite(x) && IsFinite(y) && IsFinite(z) && IsFinite(w);
  }

  f32 operator[](int i) const {
    Assert((i >= 0) && (i < 4));
    return ((f32*)this)[i];
  }

  f32& operator[](int i) {
    Assert((i >= 0) && (i < 4));
    return ((f32*)this)[i];
  }

  // Base address...
  inline f32* Base() { return (f32*)this; }
  inline f32 const* Base() const { return (f32 const*)this; }

  // Cast to Vector and Vector2D...
  Vector& AsVector3D() { return *(Vector*)this; }
  Vector const& AsVector3D() const { return *(Vector const*)this; }

  Vector2D& AsVector2D() { return *(Vector2D*)this; }
  Vector2D const& AsVector2D() const { return *(Vector2D const*)this; }

  // Initialization methods
  void Random(f32 minVal, f32 maxVal) {
    x = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
    y = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
    z = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
    w = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  }

  // equality
  bool operator==(const Vector4D& v) const;
  bool operator!=(const Vector4D& v) const;

  // arithmetic operations
  Vector4D& operator+=(const Vector4D& v);
  Vector4D& operator-=(const Vector4D& v);
  Vector4D& operator*=(const Vector4D& v);
  Vector4D& operator*=(f32 s);
  Vector4D& operator/=(const Vector4D& v);
  Vector4D& operator/=(f32 s);

  // negate the Vector4D components
  // standard math operations
  void Negate() {
    Assert(IsValid());
    x = -x;
    y = -y;
    z = -z;
    w = -w;
  }

  // Get the Vector4D's magnitude.
  f32 Length() const { return Vector4DLength(*this); }

  // Get the Vector4D's magnitude squared.
  f32 LengthSqr() const {
    Assert(IsValid());
    return (x * x + y * y + z * z + w * w);
  }

  // return true if this vector is (0,0,0,0) within tolerance
  bool IsZero(f32 tolerance = 0.01f) const {
    return (x > -tolerance && x < tolerance && y > -tolerance &&
            y < tolerance && z > -tolerance && z < tolerance &&
            w > -tolerance && w < tolerance);
  }

  // Get the distance from this Vector4D to the other one.
  f32 DistTo(const Vector4D& vOther) const {
    Vector4D delta;
    Vector4DSubtract(*this, vOther, delta);
    return delta.Length();
  }

  // Get the distance from this Vector4D to the other one squared.
  f32 DistToSqr(const Vector4D& vOther) const {
    Vector4D delta;
    Vector4DSubtract(*this, vOther, delta);
    return delta.LengthSqr();
  }

  // Copy
  void CopyToArray(f32* rgfl) const {
    Assert(IsValid());
    Assert(rgfl);
    rgfl[0] = x;
    rgfl[1] = y;
    rgfl[2] = z;
    rgfl[3] = w;
  }

  // Multiply, add, and assign to this (ie: *this = a + b * scalar). This
  // is about 12% faster than the actual Vector4D equation (because it's done
  // per-component rather than per-Vector4D).
  // TODO(d.rattman): Remove
  // For backwards compatability
  void MulAdd(Vector4D const& a, Vector4D const& b, f32 scalar) {
    x = a.x + b.x * scalar;
    y = a.y + b.y * scalar;
    z = a.z + b.z * scalar;
    w = a.w + b.w * scalar;
  }

  // Dot product.
  // for backwards compatability
  f32 Dot(Vector4D const& vOther) const { return DotProduct4D(*this, vOther); }

    // No copy constructors allowed if we're in optimal mode
#ifdef VECTOR_NO_SLOW_OPERATIONS
 private:
#else
 public:
#endif
  Vector4D(Vector4D const& vOther) {
    Assert(vOther.IsValid());
    x = vOther.x;
    y = vOther.y;
    z = vOther.z;
    w = vOther.w;
  }

  // No assignment operators either...
  Vector4D& operator=(Vector4D const& vOther) {
    Assert(vOther.IsValid());
    x = vOther.x;
    y = vOther.y;
    z = vOther.z;
    w = vOther.w;
    return *this;
  }
};

const Vector4D vec4_origin(0.0f, 0.0f, 0.0f, 0.0f);
const Vector4D vec4_invalid(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);

// SSE optimized routines
class alignas(16) Vector4DAligned : public Vector4D {
 public:
  Vector4DAligned() {}
  Vector4DAligned(f32 X, f32 Y, f32 Z, f32 W) {
    x = X;
    y = Y;
    z = Z;
    w = W;
    Assert(IsValid());
  }

  inline void Set(f32 X, f32 Y, f32 Z, f32 W) {
    x = X;
    y = Y;
    z = Z;
    w = W;
    Assert(IsValid());
  }
  inline void InitZero() {
    this->AsM128() = _mm_set1_ps(0.0f);

    Assert(IsValid());
  }

  inline __m128& AsM128() { return *(__m128*)&x; }
  inline const __m128& AsM128() const { return *(const __m128*)&x; }

 private:
  // No copy constructors allowed if we're in optimal mode
  Vector4DAligned(Vector4DAligned const& vOther);

  // No assignment operators either...
  Vector4DAligned& operator=(Vector4DAligned const& src);
};

// Vector4D related operations.

// Vector4D clear.
inline void Vector4DClear(Vector4D& a) { a.x = a.y = a.z = a.w = 0.0f; }

// Copy.
inline void Vector4DCopy(Vector4D const& src, Vector4D& dst) {
  Assert(src.IsValid());
  dst.x = src.x;
  dst.y = src.y;
  dst.z = src.z;
  dst.w = src.w;
}

// Vector4D arithmetic.
inline void Vector4DAdd(Vector4D const& a, Vector4D const& b, Vector4D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x + b.x;
  c.y = a.y + b.y;
  c.z = a.z + b.z;
  c.w = a.w + b.w;
}
inline void Vector4DSubtract(Vector4D const& a, Vector4D const& b,
                             Vector4D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x - b.x;
  c.y = a.y - b.y;
  c.z = a.z - b.z;
  c.w = a.w - b.w;
}
inline void Vector4DMultiply(Vector4D const& a, f32 b, Vector4D& c) {
  Assert(a.IsValid() && IsFinite(b));
  c.x = a.x * b;
  c.y = a.y * b;
  c.z = a.z * b;
  c.w = a.w * b;
}
inline void Vector4DMultiply(Vector4D const& a, Vector4D const& b,
                             Vector4D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x * b.x;
  c.y = a.y * b.y;
  c.z = a.z * b.z;
  c.w = a.w * b.w;
}
inline void Vector4DDivide(Vector4D const& a, f32 b, Vector4D& c) {
  Assert(a.IsValid());
  Assert(b != 0.0f);
  f32 oob = 1.0f / b;
  c.x = a.x * oob;
  c.y = a.y * oob;
  c.z = a.z * oob;
  c.w = a.w * oob;
}
inline void Vector4DDivide(Vector4D const& a, Vector4D const& b, Vector4D& c) {
  Assert(a.IsValid());
  Assert((b.x != 0.0f) && (b.y != 0.0f) && (b.z != 0.0f) && (b.w != 0.0f));
  c.x = a.x / b.x;
  c.y = a.y / b.y;
  c.z = a.z / b.z;
  c.w = a.w / b.w;
}
inline void Vector4DMA(Vector4D const& start, f32 s, Vector4D const& dir,
                       Vector4D& result) {
  Assert(start.IsValid() && IsFinite(s) && dir.IsValid());
  result.x = start.x + s * dir.x;
  result.y = start.y + s * dir.y;
  result.z = start.z + s * dir.z;
  result.w = start.w + s * dir.w;
}

// Vector4DAligned arithmetic.
inline void Vector4DMultiplyAligned(Vector4DAligned const& a,
                                    Vector4DAligned const& b,
                                    Vector4DAligned& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x * b.x;
  c.y = a.y * b.y;
  c.z = a.z * b.z;
  c.w = a.w * b.w;
}

#define Vector4DExpand(v) (v).x, (v).y, (v).z, (v).w

// Normalization.
// TODO(d.rattman): Can't use until we're un-macroed in mathlib.h
inline f32 Vector4DNormalize(Vector4D& v) {
  Assert(v.IsValid());
  f32 l = v.Length();
  if (l != 0.0f) {
    v /= l;
  } else {
    v.x = v.y = v.z = v.w = 0.0f;
  }
  return l;
}

// Length.
inline f32 Vector4DLength(Vector4D const& v) {
  Assert(v.IsValid());
  return (f32)FastSqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

// Dot Product.
inline f32 DotProduct4D(Vector4D const& a, Vector4D const& b) {
  Assert(a.IsValid() && b.IsValid());
  return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

// Linearly interpolate between two vectors.
inline void Vector4DLerp(Vector4D const& src1, Vector4D const& src2, f32 t,
                         Vector4D& dest) {
  dest[0] = src1[0] + (src2[0] - src1[0]) * t;
  dest[1] = src1[1] + (src2[1] - src1[1]) * t;
  dest[2] = src1[2] + (src2[2] - src1[2]) * t;
  dest[3] = src1[3] + (src2[3] - src1[3]) * t;
}

inline bool Vector4D::operator==(Vector4D const& src) const {
  Assert(src.IsValid() && IsValid());
  return (src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);
}

inline bool Vector4D::operator!=(Vector4D const& src) const {
  Assert(src.IsValid() && IsValid());
  return (src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);
}

inline Vector4D& Vector4D::operator+=(const Vector4D& v) {
  Assert(IsValid() && v.IsValid());
  x += v.x;
  y += v.y;
  z += v.z;
  w += v.w;
  return *this;
}

inline Vector4D& Vector4D::operator-=(const Vector4D& v) {
  Assert(IsValid() && v.IsValid());
  x -= v.x;
  y -= v.y;
  z -= v.z;
  w -= v.w;
  return *this;
}

inline Vector4D& Vector4D::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  z *= fl;
  w *= fl;
  Assert(IsValid());
  return *this;
}

inline Vector4D& Vector4D::operator*=(Vector4D const& v) {
  x *= v.x;
  y *= v.y;
  z *= v.z;
  w *= v.w;
  Assert(IsValid());
  return *this;
}

inline Vector4D& Vector4D::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  z *= oofl;
  w *= oofl;
  Assert(IsValid());
  return *this;
}

inline Vector4D& Vector4D::operator/=(Vector4D const& v) {
  Assert(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f && v.w != 0.0f);
  x /= v.x;
  y /= v.y;
  z /= v.z;
  w /= v.w;
  Assert(IsValid());
  return *this;
}

inline void Vector4DWeightMAD(f32 w, Vector4DAligned const& vInA,
                              Vector4DAligned& vOutA,
                              Vector4DAligned const& vInB,
                              Vector4DAligned& vOutB) {
  Assert(vInA.IsValid() && vInB.IsValid() && IsFinite(w));

  vOutA.x += vInA.x * w;
  vOutA.y += vInA.y * w;
  vOutA.z += vInA.z * w;
  vOutA.w += vInA.w * w;

  vOutB.x += vInB.x * w;
  vOutB.y += vInB.y * w;
  vOutB.z += vInB.z * w;
  vOutB.w += vInB.w * w;
}

inline void Vector4DWeightMADSSE(f32 w, Vector4DAligned const& vInA,
                                 Vector4DAligned& vOutA,
                                 Vector4DAligned const& vInB,
                                 Vector4DAligned& vOutB) {
  Assert(vInA.IsValid() && vInB.IsValid() && IsFinite(w));

  // Replicate scalar f32 out to 4 components
  __m128 packed = _mm_set1_ps(w);

  // 4D SSE Vector MAD
  vOutA.AsM128() =
      _mm_add_ps(vOutA.AsM128(), _mm_mul_ps(vInA.AsM128(), packed));
  vOutB.AsM128() =
      _mm_add_ps(vOutB.AsM128(), _mm_mul_ps(vInB.AsM128(), packed));
}

#endif  // SOURCE_MATHLIB_VECTOR4D_H_
