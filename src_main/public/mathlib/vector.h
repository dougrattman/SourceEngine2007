// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_VECTOR_H_
#define SOURCE_MATHLIB_VECTOR_H_

#ifdef _WIN32
#pragma once
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>  // std::rand.

#include "build/include/build_config.h"

#if defined(COMPILER_MSVC) && defined(ARCH_CPU_X86)
#include <xmmintrin.h>  // SSE* intrinsics.
#endif

#include "base/include/base_types.h"
#include "mathlib/math_pfns.h"
#include "mathlib/vector2d.h"
#include "tier0/include/dbg.h"
#include "tier0/include/floattypes.h"
#include "tier0/include/threadtools.h"

// Uncomment this to add extra Asserts to check for NANs, uninitialized vecs,
// etc.
//#define VECTOR_PARANOIA	1

// Uncomment this to make sure we don't do anything slow with our vectors
//#define VECTOR_NO_SLOW_OPERATIONS 1

// Used to make certain code easier to read.
#define X_INDEX 0
#define Y_INDEX 1
#define Z_INDEX 2

#ifdef VECTOR_PARANOIA
#define CHECK_VALID(_v) Assert((_v).IsValid())
#else
#define CHECK_VALID(_v) 0
#endif

#define VecToString(v)              \
  (static_cast<const ch*>(CFmtStr(  \
      "(%f, %f, %f)", (v).x, (v).y, \
      (v).z)))  // ** Note: this generates a temporary, don't hold reference!

class VectorByValue;

// 3D Vector
class Vector {
 public:
  // Members
  f32 x, y, z;

  // Construction/destruction:
  inline Vector() {
#ifdef _DEBUG
#ifdef VECTOR_PARANOIA
    // Initialize to NAN to catch errors
    x = y = z = VEC_T_NAN;
#endif
#endif
  }

  constexpr inline Vector(f32 X, f32 Y, f32 Z) : x{X}, y{Y}, z{Z} {
    CHECK_VALID(*this);
  }

  // Initialization
  void Init(f32 ix = 0.0f, f32 iy = 0.0f, f32 iz = 0.0f);

  // Got any nasty NAN's?
  bool IsValid() const;
  void Invalidate();

  // array access...
  f32 operator[](int i) const;
  f32& operator[](int i);

  // Base address...
  f32* Base();
  f32 const* Base() const;

  // Cast to Vector2D...
  Vector2D& AsVector2D();
  const Vector2D& AsVector2D() const;

  // Initialization methods
  void Random(f32 minVal, f32 maxVal);
  inline void Zero();  ///< zero out a vector

  // equality
  bool operator==(const Vector& v) const;
  bool operator!=(const Vector& v) const;

  // arithmetic operations
  SOURCE_FORCEINLINE Vector& operator+=(const Vector& v);
  SOURCE_FORCEINLINE Vector& operator-=(const Vector& v);
  SOURCE_FORCEINLINE Vector& operator*=(const Vector& v);
  SOURCE_FORCEINLINE Vector& operator*=(f32 s);
  SOURCE_FORCEINLINE Vector& operator/=(const Vector& v);
  SOURCE_FORCEINLINE Vector& operator/=(f32 s);
  SOURCE_FORCEINLINE Vector& operator+=(f32 fl);  ///< broadcast add
  SOURCE_FORCEINLINE Vector& operator-=(f32 fl);  ///< broadcast sub

  // negate the vector components
  void Negate();

  // Get the vector's magnitude.
  inline f32 Length() const;

  // Get the vector's magnitude squared.
  SOURCE_FORCEINLINE f32 LengthSqr() const {
    CHECK_VALID(*this);
    return (x * x + y * y + z * z);
  }

  // return true if this vector is (0,0,0) within tolerance
  bool IsZero(f32 tolerance = 0.01f) const {
    return (x > -tolerance && x < tolerance && y > -tolerance &&
            y < tolerance && z > -tolerance && z < tolerance);
  }

  f32 NormalizeInPlace();
  bool IsLengthGreaterThan(f32 val) const;
  bool IsLengthLessThan(f32 val) const;

  // check if a vector is within the box defined by two other vectors
  SOURCE_FORCEINLINE bool WithinAABox(Vector const& boxmin,
                                      Vector const& boxmax);

  // Get the distance from this vector to the other one.
  f32 DistTo(const Vector& vOther) const;

  // Get the distance from this vector to the other one squared.
  // NJS: note, VC wasn't inlining it correctly in several deeply nested inlines
  // due to being an 'out of line' inline. may be able to tidy this up after
  // switching to VC7
  SOURCE_FORCEINLINE f32 DistToSqr(const Vector& vOther) const {
    const Vector delta{x - vOther.x, y - vOther.y, z - vOther.z};
    return delta.LengthSqr();
  }

  // Copy
  void CopyToArray(f32* rgfl) const;

  // Multiply, add, and assign to this (ie: *this = a + b * scalar). This
  // is about 12% faster than the actual vector equation (because it's done
  // per-component rather than per-vector).
  void MulAdd(const Vector& a, const Vector& b, f32 scalar);

  // Dot product.
  f32 Dot(const Vector& vOther) const;

  // assignment
  Vector& operator=(const Vector& vOther);

  // 2d
  f32 Length2D(void) const;
  f32 Length2DSqr(void) const;

  operator VectorByValue&() { return *((VectorByValue*)(this)); }
  operator const VectorByValue&() const {
    return *((const VectorByValue*)(this));
  }

#ifndef VECTOR_NO_SLOW_OPERATIONS
  // copy constructors
  //	Vector(const Vector &vOther);

  // arithmetic operations
  Vector operator-(void) const;

  Vector operator+(const Vector& v) const;
  Vector operator-(const Vector& v) const;
  Vector operator*(const Vector& v) const;
  Vector operator/(const Vector& v) const;
  Vector operator*(f32 fl) const;
  Vector operator/(f32 fl) const;

  // Cross product between two vectors.
  Vector Cross(const Vector& vOther) const;

  // Returns a vector with the min or max in X, Y, and Z.
  Vector Min(const Vector& vOther) const;
  Vector Max(const Vector& vOther) const;

#else

 private:
  // No copy constructors allowed if we're in optimal mode
  Vector(const Vector& vOther);
#endif
};

#define USE_M64S ((!defined(_X360)) && (!defined(OS_POSIX)))

//=========================================================
// 4D Short Vector (aligned on 8-u8 boundary)
//=========================================================
class alignas(8) ShortVector {
 public:
  i16 x, y, z, w;

  // Initialization
  void Init(i16 ix = 0, i16 iy = 0, i16 iz = 0, i16 iw = 0);

#if USE_M64S
  __m64& AsM64() { return *(__m64*)&x; }
  const __m64& AsM64() const { return *(const __m64*)&x; }
#endif

  // Setter
  void Set(const ShortVector& vOther);
  void Set(const i16 ix, const i16 iy, const i16 iz, const i16 iw);

  // array access...
  i16 operator[](int i) const;
  i16& operator[](int i);

  // Base address...
  i16* Base();
  i16 const* Base() const;

  // equality
  bool operator==(const ShortVector& v) const;
  bool operator!=(const ShortVector& v) const;

  // Arithmetic operations
  SOURCE_FORCEINLINE ShortVector& operator+=(const ShortVector& v);
  SOURCE_FORCEINLINE ShortVector& operator-=(const ShortVector& v);
  SOURCE_FORCEINLINE ShortVector& operator*=(const ShortVector& v);
  SOURCE_FORCEINLINE ShortVector& operator*=(f32 s);
  SOURCE_FORCEINLINE ShortVector& operator/=(const ShortVector& v);
  SOURCE_FORCEINLINE ShortVector& operator/=(f32 s);
  SOURCE_FORCEINLINE ShortVector operator*(f32 fl) const;

 private:
  // No copy constructors allowed if we're in optimal mode
  //	ShortVector(ShortVector const& vOther);

  // No assignment operators either...
  //	ShortVector& operator=( ShortVector const& src );
};

//=========================================================
// 4D Integer Vector
//=========================================================
class IntVector4D {
 public:
  int x, y, z, w;

  // Initialization
  void Init(int ix = 0, int iy = 0, int iz = 0, int iw = 0);

#if USE_M64S
  __m64& AsM64() { return *(__m64*)&x; }
  const __m64& AsM64() const { return *(const __m64*)&x; }
#endif

  // Setter
  void Set(const IntVector4D& vOther);
  void Set(const int ix, const int iy, const int iz, const int iw);

  // array access...
  int operator[](int i) const;
  int& operator[](int i);

  // Base address...
  int* Base();
  int const* Base() const;

  // equality
  bool operator==(const IntVector4D& v) const;
  bool operator!=(const IntVector4D& v) const;

  // Arithmetic operations
  SOURCE_FORCEINLINE IntVector4D& operator+=(const IntVector4D& v);
  SOURCE_FORCEINLINE IntVector4D& operator-=(const IntVector4D& v);
  SOURCE_FORCEINLINE IntVector4D& operator*=(const IntVector4D& v);
  SOURCE_FORCEINLINE IntVector4D& operator*=(f32 s);
  SOURCE_FORCEINLINE IntVector4D& operator/=(const IntVector4D& v);
  SOURCE_FORCEINLINE IntVector4D& operator/=(f32 s);
  SOURCE_FORCEINLINE IntVector4D operator*(f32 fl) const;

 private:
  // No copy constructors allowed if we're in optimal mode
  //	IntVector4D(IntVector4D const& vOther);

  // No assignment operators either...
  //	IntVector4D& operator=( IntVector4D const& src );
};

// Allows us to specifically pass the vector by value when we need to

class VectorByValue : public Vector {
 public:
  // Construction/destruction:
  VectorByValue(void) : Vector() {}
  VectorByValue(f32 X, f32 Y, f32 Z) : Vector(X, Y, Z) {}
  VectorByValue(const VectorByValue& vOther) { *this = vOther; }
};

// Utility to simplify table construction. No constructor means can use
// traditional C-style initialization

class TableVector {
 public:
  f32 x, y, z;

  operator Vector&() { return *((Vector*)(this)); }
  operator const Vector&() const { return *((const Vector*)(this)); }

  // array access...
  inline f32& operator[](int i) {
    Assert((i >= 0) && (i < 3));
    return ((f32*)this)[i];
  }

  inline f32 operator[](int i) const {
    Assert((i >= 0) && (i < 3));
    return ((f32*)this)[i];
  }
};

// Here's where we add all those lovely SSE optimized routines

class alignas(16) VectorAligned : public Vector {
 public:
  inline VectorAligned(){};
  inline VectorAligned(f32 X, f32 Y, f32 Z) { Init(X, Y, Z); }

#ifdef VECTOR_NO_SLOW_OPERATIONS

 private:
  // No copy constructors allowed if we're in optimal mode
  VectorAligned(const VectorAligned& vOther);
  VectorAligned(const Vector& vOther);

#else
 public:
  explicit VectorAligned(const Vector& vOther) {
    Init(vOther.x, vOther.y, vOther.z);
  }

  VectorAligned& operator=(const Vector& vOther) {
    Init(vOther.x, vOther.y, vOther.z);
    return *this;
  }

#endif
  f32 w;  // this space is used anyway
};

// Vector related operations

// Vector clear
SOURCE_FORCEINLINE void VectorClear(Vector& a);

// Copy
SOURCE_FORCEINLINE void VectorCopy(const Vector& src, Vector& dst);

// Vector arithmetic
SOURCE_FORCEINLINE void VectorAdd(const Vector& a, const Vector& b,
                                  Vector& result);
SOURCE_FORCEINLINE void VectorSubtract(const Vector& a, const Vector& b,
                                       Vector& result);
SOURCE_FORCEINLINE void VectorMultiply(const Vector& a, f32 b, Vector& result);
SOURCE_FORCEINLINE void VectorMultiply(const Vector& a, const Vector& b,
                                       Vector& result);
SOURCE_FORCEINLINE void VectorDivide(const Vector& a, f32 b, Vector& result);
SOURCE_FORCEINLINE void VectorDivide(const Vector& a, const Vector& b,
                                     Vector& result);
inline void VectorScale(const Vector& in, f32 scale, Vector& result);
inline void VectorMA(const Vector& start, f32 scale, const Vector& direction,
                     Vector& dest);

// Vector equality with tolerance
bool VectorsAreEqual(const Vector& src1, const Vector& src2,
                     f32 tolerance = 0.0f);

#define VectorExpand(v) (v).x, (v).y, (v).z

// Normalization
// TODO(d.rattman): Can't use quite yet
// f32 VectorNormalize( Vector& v );

// Length
inline f32 VectorLength(const Vector& v);

// Dot Product
SOURCE_FORCEINLINE f32 DotProduct(const Vector& a, const Vector& b);

// Cross product
void CrossProduct(const Vector& a, const Vector& b, Vector& result);

// Store the min or max of each of x, y, and z into the result.
void VectorMin(const Vector& a, const Vector& b, Vector& result);
void VectorMax(const Vector& a, const Vector& b, Vector& result);

// Linearly interpolate between two vectors
void VectorLerp(const Vector& src1, const Vector& src2, f32 t, Vector& dest);

#ifndef VECTOR_NO_SLOW_OPERATIONS

// Cross product
Vector CrossProduct(const Vector& a, const Vector& b);

// Random vector creation
Vector RandomVector(f32 minVal, f32 maxVal);

#endif

//
// Inlined Vector methods
//

// initialization

inline void Vector::Init(f32 ix, f32 iy, f32 iz) {
  x = ix;
  y = iy;
  z = iz;
  CHECK_VALID(*this);
}

inline void Vector::Random(f32 minVal, f32 maxVal) {
  x = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  y = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  z = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  CHECK_VALID(*this);
}

// This should really be a single opcode on the PowerPC (move r0 onto the vec
// reg)
inline void Vector::Zero() { x = y = z = 0.0f; }

inline void VectorClear(Vector& a) { a.x = a.y = a.z = 0.0f; }

// assignment

inline Vector& Vector::operator=(const Vector& vOther) {
  CHECK_VALID(vOther);
  x = vOther.x;
  y = vOther.y;
  z = vOther.z;
  return *this;
}

// Array access

inline f32& Vector::operator[](int i) {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

inline f32 Vector::operator[](int i) const {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

// Base address...

inline f32* Vector::Base() { return (f32*)this; }

inline f32 const* Vector::Base() const { return (f32 const*)this; }

// Cast to Vector2D...

inline Vector2D& Vector::AsVector2D() { return *(Vector2D*)this; }

inline const Vector2D& Vector::AsVector2D() const {
  return *(const Vector2D*)this;
}

// IsValid?

inline bool Vector::IsValid() const {
  return IsFinite(x) && IsFinite(y) && IsFinite(z);
}

// Invalidate

inline void Vector::Invalidate() {
  //#ifdef _DEBUG
  //#ifdef VECTOR_PARANOIA
  x = y = z = FLOAT32_NAN;
  //#endif
  //#endif
}

// comparison

inline bool Vector::operator==(const Vector& src) const {
  CHECK_VALID(src);
  CHECK_VALID(*this);
  return (src.x == x) && (src.y == y) && (src.z == z);
}

inline bool Vector::operator!=(const Vector& src) const {
  CHECK_VALID(src);
  CHECK_VALID(*this);
  return (src.x != x) || (src.y != y) || (src.z != z);
}

// Copy

SOURCE_FORCEINLINE void VectorCopy(const Vector& src, Vector& dst) {
  CHECK_VALID(src);
  dst.x = src.x;
  dst.y = src.y;
  dst.z = src.z;
}

inline void Vector::CopyToArray(f32* rgfl) const {
  Assert(rgfl);
  CHECK_VALID(*this);
  rgfl[0] = x, rgfl[1] = y, rgfl[2] = z;
}

// standard math operations

// #pragma message("TODO: these should be SSE")

inline void Vector::Negate() {
  CHECK_VALID(*this);
  x = -x;
  y = -y;
  z = -z;
}

SOURCE_FORCEINLINE Vector& Vector::operator+=(const Vector& v) {
  CHECK_VALID(*this);
  CHECK_VALID(v);
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator-=(const Vector& v) {
  CHECK_VALID(*this);
  CHECK_VALID(v);
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  z *= fl;
  CHECK_VALID(*this);
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator*=(const Vector& v) {
  CHECK_VALID(v);
  x *= v.x;
  y *= v.y;
  z *= v.z;
  CHECK_VALID(*this);
  return *this;
}

// this ought to be an opcode.
SOURCE_FORCEINLINE Vector& Vector::operator+=(f32 fl) {
  x += fl;
  y += fl;
  z += fl;
  CHECK_VALID(*this);
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator-=(f32 fl) {
  x -= fl;
  y -= fl;
  z -= fl;
  CHECK_VALID(*this);
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  z *= oofl;
  CHECK_VALID(*this);
  return *this;
}

SOURCE_FORCEINLINE Vector& Vector::operator/=(const Vector& v) {
  CHECK_VALID(v);
  Assert(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f);
  x /= v.x;
  y /= v.y;
  z /= v.z;
  CHECK_VALID(*this);
  return *this;
}

//
// Inlined Short Vector methods
//

inline void ShortVector::Init(i16 ix, i16 iy, i16 iz, i16 iw) {
  x = ix;
  y = iy;
  z = iz;
  w = iw;
}

SOURCE_FORCEINLINE void ShortVector::Set(const ShortVector& vOther) {
  x = vOther.x;
  y = vOther.y;
  z = vOther.z;
  w = vOther.w;
}

SOURCE_FORCEINLINE void ShortVector::Set(const i16 ix, const i16 iy,
                                         const i16 iz, const i16 iw) {
  x = ix;
  y = iy;
  z = iz;
  w = iw;
}

// Array access

inline i16 ShortVector::operator[](int i) const {
  Assert((i >= 0) && (i < 4));
  return ((i16*)this)[i];
}

inline i16& ShortVector::operator[](int i) {
  Assert((i >= 0) && (i < 4));
  return ((i16*)this)[i];
}

// Base address...

inline i16* ShortVector::Base() { return (i16*)this; }

inline i16 const* ShortVector::Base() const { return (i16 const*)this; }

// comparison

inline bool ShortVector::operator==(const ShortVector& src) const {
  return (src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);
}

inline bool ShortVector::operator!=(const ShortVector& src) const {
  return (src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);
}

// standard math operations

SOURCE_FORCEINLINE ShortVector& ShortVector::operator+=(const ShortVector& v) {
  x += v.x;
  y += v.y;
  z += v.z;
  w += v.w;
  return *this;
}

SOURCE_FORCEINLINE ShortVector& ShortVector::operator-=(const ShortVector& v) {
  x -= v.x;
  y -= v.y;
  z -= v.z;
  w -= v.w;
  return *this;
}

SOURCE_FORCEINLINE ShortVector& ShortVector::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  z *= fl;
  w *= fl;
  return *this;
}

SOURCE_FORCEINLINE ShortVector& ShortVector::operator*=(const ShortVector& v) {
  x *= v.x;
  y *= v.y;
  z *= v.z;
  w *= v.w;
  return *this;
}

SOURCE_FORCEINLINE ShortVector& ShortVector::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  z *= oofl;
  w *= oofl;
  return *this;
}

SOURCE_FORCEINLINE ShortVector& ShortVector::operator/=(const ShortVector& v) {
  Assert(v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0);
  x /= v.x;
  y /= v.y;
  z /= v.z;
  w /= v.w;
  return *this;
}

SOURCE_FORCEINLINE void ShortVectorMultiply(const ShortVector& src, f32 fl,
                                            ShortVector& res) {
  Assert(IsFinite(fl));
  res.x = src.x * fl;
  res.y = src.y * fl;
  res.z = src.z * fl;
  res.w = src.w * fl;
}

SOURCE_FORCEINLINE ShortVector ShortVector::operator*(f32 fl) const {
  ShortVector res;
  ShortVectorMultiply(*this, fl, res);
  return res;
}

//
// Inlined Integer Vector methods
//

inline void IntVector4D::Init(int ix, int iy, int iz, int iw) {
  x = ix;
  y = iy;
  z = iz;
  w = iw;
}

SOURCE_FORCEINLINE void IntVector4D::Set(const IntVector4D& vOther) {
  x = vOther.x;
  y = vOther.y;
  z = vOther.z;
  w = vOther.w;
}

SOURCE_FORCEINLINE void IntVector4D::Set(const int ix, const int iy,
                                         const int iz, const int iw) {
  x = ix;
  y = iy;
  z = iz;
  w = iw;
}

// Array access

inline int IntVector4D::operator[](int i) const {
  Assert((i >= 0) && (i < 4));
  return ((int*)this)[i];
}

inline int& IntVector4D::operator[](int i) {
  Assert((i >= 0) && (i < 4));
  return ((int*)this)[i];
}

// Base address...

inline int* IntVector4D::Base() { return (int*)this; }

inline int const* IntVector4D::Base() const { return (int const*)this; }

// comparison

inline bool IntVector4D::operator==(const IntVector4D& src) const {
  return (src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);
}

inline bool IntVector4D::operator!=(const IntVector4D& src) const {
  return (src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);
}

// standard math operations

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator+=(const IntVector4D& v) {
  x += v.x;
  y += v.y;
  z += v.z;
  w += v.w;
  return *this;
}

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator-=(const IntVector4D& v) {
  x -= v.x;
  y -= v.y;
  z -= v.z;
  w -= v.w;
  return *this;
}

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  z *= fl;
  w *= fl;
  return *this;
}

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator*=(const IntVector4D& v) {
  x *= v.x;
  y *= v.y;
  z *= v.z;
  w *= v.w;
  return *this;
}

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  z *= oofl;
  w *= oofl;
  return *this;
}

SOURCE_FORCEINLINE IntVector4D& IntVector4D::operator/=(const IntVector4D& v) {
  Assert(v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0);
  x /= v.x;
  y /= v.y;
  z /= v.z;
  w /= v.w;
  return *this;
}

SOURCE_FORCEINLINE void IntVector4DMultiply(const IntVector4D& src, f32 fl,
                                            IntVector4D& res) {
  Assert(IsFinite(fl));
  res.x = src.x * fl;
  res.y = src.y * fl;
  res.z = src.z * fl;
  res.w = src.w * fl;
}

SOURCE_FORCEINLINE IntVector4D IntVector4D::operator*(f32 fl) const {
  IntVector4D res;
  IntVector4DMultiply(*this, fl, res);
  return res;
}

// =======================

SOURCE_FORCEINLINE void VectorAdd(const Vector& a, const Vector& b, Vector& c) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  c.x = a.x + b.x;
  c.y = a.y + b.y;
  c.z = a.z + b.z;
}

SOURCE_FORCEINLINE void VectorSubtract(const Vector& a, const Vector& b,
                                       Vector& c) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  c.x = a.x - b.x;
  c.y = a.y - b.y;
  c.z = a.z - b.z;
}

SOURCE_FORCEINLINE void VectorMultiply(const Vector& a, f32 b, Vector& c) {
  CHECK_VALID(a);
  Assert(IsFinite(b));
  c.x = a.x * b;
  c.y = a.y * b;
  c.z = a.z * b;
}

SOURCE_FORCEINLINE void VectorMultiply(const Vector& a, const Vector& b,
                                       Vector& c) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  c.x = a.x * b.x;
  c.y = a.y * b.y;
  c.z = a.z * b.z;
}

// for backwards compatability
inline void VectorScale(const Vector& in, f32 scale, Vector& result) {
  VectorMultiply(in, scale, result);
}

SOURCE_FORCEINLINE void VectorDivide(const Vector& a, f32 b, Vector& c) {
  CHECK_VALID(a);
  Assert(b != 0.0f);
  f32 oob = 1.0f / b;
  c.x = a.x * oob;
  c.y = a.y * oob;
  c.z = a.z * oob;
}

SOURCE_FORCEINLINE void VectorDivide(const Vector& a, const Vector& b,
                                     Vector& c) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  Assert((b.x != 0.0f) && (b.y != 0.0f) && (b.z != 0.0f));
  c.x = a.x / b.x;
  c.y = a.y / b.y;
  c.z = a.z / b.z;
}

// TODO(d.rattman): Remove
// For backwards compatability
inline void Vector::MulAdd(const Vector& a, const Vector& b, f32 scalar) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  x = a.x + b.x * scalar;
  y = a.y + b.y * scalar;
  z = a.z + b.z * scalar;
}

inline void VectorLerp(const Vector& src1, const Vector& src2, f32 t,
                       Vector& dest) {
  CHECK_VALID(src1);
  CHECK_VALID(src2);
  dest.x = src1.x + (src2.x - src1.x) * t;
  dest.y = src1.y + (src2.y - src1.y) * t;
  dest.z = src1.z + (src2.z - src1.z) * t;
}

// Temporary storage for vector results so const Vector& results can be returned

inline Vector& AllocTempVector() {
  static Vector s_vecTemp[128];
  static CInterlockedInt s_nIndex;

  int nIndex;
  for (;;) {
    int nOldIndex = s_nIndex;
    nIndex = ((nOldIndex + 0x10001) & 0x7F);

    if (s_nIndex.AssignIf(nOldIndex, nIndex)) {
      break;
    }
    ThreadPause();
  }
  return s_vecTemp[nIndex & 0x7F];
}

// Dot product.
SOURCE_FORCEINLINE f32 DotProduct(const Vector& a, const Vector& b) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  return (a.x * b.x + a.y * b.y + a.z * b.z);
}

// Dot product for backwards compatability.
inline f32 Vector::Dot(const Vector& vOther) const {
  CHECK_VALID(vOther);
  return DotProduct(*this, vOther);
}

inline void CrossProduct(const Vector& a, const Vector& b, Vector& result) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  Assert(&a != &result);
  Assert(&b != &result);
  result.x = a.y * b.z - a.z * b.y;
  result.y = a.z * b.x - a.x * b.z;
  result.z = a.x * b.y - a.y * b.x;
}

inline f32 DotProductAbs(const Vector& v0, const Vector& v1) {
  CHECK_VALID(v0);
  CHECK_VALID(v1);
  return FloatMakePositive(v0.x * v1.x) + FloatMakePositive(v0.y * v1.y) +
         FloatMakePositive(v0.z * v1.z);
}

inline f32 DotProductAbs(const Vector& v0, const f32* v1) {
  return FloatMakePositive(v0.x * v1[0]) + FloatMakePositive(v0.y * v1[1]) +
         FloatMakePositive(v0.z * v1[2]);
}

// length

inline f32 VectorLength(const Vector& v) {
  CHECK_VALID(v);
  return (f32)FastSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline f32 Vector::Length() const {
  CHECK_VALID(*this);
  return VectorLength(*this);
}

// check a point against a box
bool Vector::WithinAABox(Vector const& boxmin, Vector const& boxmax) {
  return ((x >= boxmin.x) && (x <= boxmax.x) && (y >= boxmin.y) &&
          (y <= boxmax.y) && (z >= boxmin.z) && (z <= boxmax.z));
}

// Get the distance from this vector to the other one

inline f32 Vector::DistTo(const Vector& vOther) const {
  Vector delta;
  VectorSubtract(*this, vOther, delta);
  return delta.Length();
}

// Vector equality with tolerance

inline bool VectorsAreEqual(const Vector& src1, const Vector& src2,
                            f32 tolerance) {
  if (FloatMakePositive(src1.x - src2.x) > tolerance) return false;
  if (FloatMakePositive(src1.y - src2.y) > tolerance) return false;
  return (FloatMakePositive(src1.z - src2.z) <= tolerance);
}

// Computes the closest point to vecTarget no farther than flMaxDist from
// vecStart

inline void ComputeClosestPoint(const Vector& vecStart, f32 flMaxDist,
                                const Vector& vecTarget, Vector* pResult) {
  Vector vecDelta;
  VectorSubtract(vecTarget, vecStart, vecDelta);
  f32 flDistSqr = vecDelta.LengthSqr();
  if (flDistSqr <= flMaxDist * flMaxDist) {
    *pResult = vecTarget;
  } else {
    vecDelta /= FastSqrt(flDistSqr);
    VectorMA(vecStart, flMaxDist, vecDelta, *pResult);
  }
}

// Takes the absolute value of a vector

inline void VectorAbs(const Vector& src, Vector& dst) {
  dst.x = FloatMakePositive(src.x);
  dst.y = FloatMakePositive(src.y);
  dst.z = FloatMakePositive(src.z);
}

  //
  // Slow methods
  //

#ifndef VECTOR_NO_SLOW_OPERATIONS

// Returns a vector with the min or max in X, Y, and Z.

inline Vector Vector::Min(const Vector& vOther) const {
  return Vector(x < vOther.x ? x : vOther.x, y < vOther.y ? y : vOther.y,
                z < vOther.z ? z : vOther.z);
}

inline Vector Vector::Max(const Vector& vOther) const {
  return Vector(x > vOther.x ? x : vOther.x, y > vOther.y ? y : vOther.y,
                z > vOther.z ? z : vOther.z);
}

// arithmetic operations

inline Vector Vector::operator-() const { return Vector(-x, -y, -z); }

inline Vector Vector::operator+(const Vector& v) const {
  Vector res;
  VectorAdd(*this, v, res);
  return res;
}

inline Vector Vector::operator-(const Vector& v) const {
  Vector res;
  VectorSubtract(*this, v, res);
  return res;
}

inline Vector Vector::operator*(f32 fl) const {
  Vector res;
  VectorMultiply(*this, fl, res);
  return res;
}

inline Vector Vector::operator*(const Vector& v) const {
  Vector res;
  VectorMultiply(*this, v, res);
  return res;
}

inline Vector Vector::operator/(f32 fl) const {
  Vector res;
  VectorDivide(*this, fl, res);
  return res;
}

inline Vector Vector::operator/(const Vector& v) const {
  Vector res;
  VectorDivide(*this, v, res);
  return res;
}

inline Vector operator*(f32 fl, const Vector& v) { return v * fl; }

// cross product

inline Vector Vector::Cross(const Vector& vOther) const {
  Vector res;
  CrossProduct(*this, vOther, res);
  return res;
}

// 2D

inline f32 Vector::Length2D() const { return (f32)FastSqrt(x * x + y * y); }

inline f32 Vector::Length2DSqr() const { return (x * x + y * y); }

inline Vector CrossProduct(const Vector& a, const Vector& b) {
  return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

inline void VectorMin(const Vector& a, const Vector& b, Vector& result) {
  result.x = std::min(a.x, b.x);
  result.y = std::min(a.y, b.y);
  result.z = std::min(a.z, b.z);
}

inline void VectorMax(const Vector& a, const Vector& b, Vector& result) {
  result.x = std::max(a.x, b.x);
  result.y = std::max(a.y, b.y);
  result.z = std::max(a.z, b.z);
}

// Get a random vector.
inline Vector RandomVector(f32 minVal, f32 maxVal) {
  Vector r;
  r.Random(minVal, maxVal);
  return r;
}

#endif  // slow

// Helper debugging stuff....

inline bool operator==(f32 const*, const Vector&) {
  // AIIIEEEE!!!!
  Assert(0);
  return false;
}

inline bool operator==(const Vector&, f32 const*) {
  // AIIIEEEE!!!!
  Assert(0);
  return false;
}

inline bool operator!=(f32 const*, const Vector&) {
  // AIIIEEEE!!!!
  Assert(0);
  return false;
}

inline bool operator!=(const Vector&, f32 const*) {
  // AIIIEEEE!!!!
  Assert(0);
  return false;
}

// AngularImpulse

// AngularImpulse are exponetial maps (an axis scaled by a "twist" angle in
// degrees)
using AngularImpulse = Vector;

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline AngularImpulse RandomAngularImpulse(f32 minVal, f32 maxVal) {
  AngularImpulse angImp;
  angImp.Random(minVal, maxVal);
  return angImp;
}

#endif

// Quaternion

class RadianEuler;

class Quaternion  // same data-layout as engine's vec4_t,
{                 //		which is a f32[4]
 public:
  inline Quaternion() {
  // Initialize to NAN to catch errors
#ifdef _DEBUG
#ifdef VECTOR_PARANOIA
    x = y = z = w = VEC_T_NAN;
#endif
#endif
  }
  inline Quaternion(f32 ix, f32 iy, f32 iz, f32 iw)
      : x(ix), y(iy), z(iz), w(iw) {}
  inline Quaternion(RadianEuler const& angle);  // evil auto type promotion!!!

  inline void Init(f32 ix = 0.0f, f32 iy = 0.0f, f32 iz = 0.0f, f32 iw = 0.0f) {
    x = ix;
    y = iy;
    z = iz;
    w = iw;
  }

  bool IsValid() const;
  void Invalidate();

  bool operator==(const Quaternion& src) const;
  bool operator!=(const Quaternion& src) const;

  f32* Base() { return (f32*)this; }
  const f32* Base() const { return (f32*)this; }

  // array access...
  f32 operator[](int i) const;
  f32& operator[](int i);

  f32 x, y, z, w;
};

// Array access

inline f32& Quaternion::operator[](int i) {
  Assert((i >= 0) && (i < 4));
  return ((f32*)this)[i];
}

inline f32 Quaternion::operator[](int i) const {
  Assert((i >= 0) && (i < 4));
  return ((f32*)this)[i];
}

// Equality test

inline bool Quaternion::operator==(const Quaternion& src) const {
  return (x == src.x) && (y == src.y) && (z == src.z) && (w == src.w);
}

inline bool Quaternion::operator!=(const Quaternion& src) const {
  return !operator==(src);
}

// Quaternion equality with tolerance

inline bool QuaternionsAreEqual(const Quaternion& src1, const Quaternion& src2,
                                f32 tolerance) {
  if (FloatMakePositive(src1.x - src2.x) > tolerance) return false;
  if (FloatMakePositive(src1.y - src2.y) > tolerance) return false;
  if (FloatMakePositive(src1.z - src2.z) > tolerance) return false;
  return (FloatMakePositive(src1.w - src2.w) <= tolerance);
}

// Here's where we add all those lovely SSE optimized routines

class alignas(16) QuaternionAligned : public Quaternion {
 public:
  inline QuaternionAligned(void){};
  inline QuaternionAligned(f32 X, f32 Y, f32 Z, f32 W) { Init(X, Y, Z, W); }

#ifdef VECTOR_NO_SLOW_OPERATIONS

 private:
  // No copy constructors allowed if we're in optimal mode
  QuaternionAligned(const QuaternionAligned& vOther);
  QuaternionAligned(const Quaternion& vOther);

#else
 public:
  explicit QuaternionAligned(const Quaternion& vOther) {
    Init(vOther.x, vOther.y, vOther.z, vOther.w);
  }

  QuaternionAligned& operator=(const Quaternion& vOther) {
    Init(vOther.x, vOther.y, vOther.z, vOther.w);
    return *this;
  }

#endif
};

// Radian Euler angle aligned to axis (NOT ROLL/PITCH/YAW)

class QAngle;
class RadianEuler {
 public:
  inline RadianEuler() {}
  inline RadianEuler(f32 X, f32 Y, f32 Z) {
    x = X;
    y = Y;
    z = Z;
  }
  inline RadianEuler(Quaternion const& q);   // evil auto type promotion!!!
  inline RadianEuler(QAngle const& angles);  // evil auto type promotion!!!

  // Initialization
  inline void Init(f32 ix = 0.0f, f32 iy = 0.0f, f32 iz = 0.0f) {
    x = ix;
    y = iy;
    z = iz;
  }

  //	conversion to qangle
  QAngle ToQAngle(void) const;
  bool IsValid() const;
  void Invalidate();

  // array access...
  f32 operator[](int i) const;
  f32& operator[](int i);

  f32 x, y, z;
};

extern void AngleQuaternion(RadianEuler const& angles, Quaternion& qt);
extern void QuaternionAngles(Quaternion const& q, RadianEuler& angles);
inline Quaternion::Quaternion(RadianEuler const& angle) {
  AngleQuaternion(angle, *this);
}

inline bool Quaternion::IsValid() const {
  return IsFinite(x) && IsFinite(y) && IsFinite(z) && IsFinite(w);
}

inline void Quaternion::Invalidate() {
  //#ifdef _DEBUG
  //#ifdef VECTOR_PARANOIA
  x = y = z = w = FLOAT32_NAN;
  //#endif
  //#endif
}

inline RadianEuler::RadianEuler(Quaternion const& q) {
  QuaternionAngles(q, *this);
}

inline void VectorCopy(RadianEuler const& src, RadianEuler& dst) {
  CHECK_VALID(src);
  dst.x = src.x;
  dst.y = src.y;
  dst.z = src.z;
}

inline void VectorScale(RadianEuler const& src, f32 b, RadianEuler& dst) {
  CHECK_VALID(src);
  Assert(IsFinite(b));
  dst.x = src.x * b;
  dst.y = src.y * b;
  dst.z = src.z * b;
}

inline bool RadianEuler::IsValid() const {
  return IsFinite(x) && IsFinite(y) && IsFinite(z);
}

inline void RadianEuler::Invalidate() {
  //#ifdef _DEBUG
  //#ifdef VECTOR_PARANOIA
  x = y = z = FLOAT32_NAN;
  //#endif
  //#endif
}

// Array access

inline f32& RadianEuler::operator[](int i) {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

inline f32 RadianEuler::operator[](int i) const {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

// Degree Euler QAngle pitch, yaw, roll

class QAngleByValue;

class QAngle {
 public:
  // Members
  f32 x, y, z;

  // Construction/destruction
  QAngle();
  QAngle(f32 X, f32 Y, f32 Z);
  //	QAngle(RadianEuler const &angles);	// evil auto type promotion!!!

  // Allow pass-by-value
  operator QAngleByValue&() { return *((QAngleByValue*)(this)); }
  operator const QAngleByValue&() const {
    return *((const QAngleByValue*)(this));
  }

  // Initialization
  void Init(f32 ix = 0.0f, f32 iy = 0.0f, f32 iz = 0.0f);
  void Random(f32 minVal, f32 maxVal);

  // Got any nasty NAN's?
  bool IsValid() const;
  void Invalidate();

  // array access...
  f32 operator[](int i) const;
  f32& operator[](int i);

  // Base address...
  f32* Base();
  f32 const* Base() const;

  // equality
  bool operator==(const QAngle& v) const;
  bool operator!=(const QAngle& v) const;

  // arithmetic operations
  QAngle& operator+=(const QAngle& v);
  QAngle& operator-=(const QAngle& v);
  QAngle& operator*=(f32 s);
  QAngle& operator/=(f32 s);

  // Get the vector's magnitude.
  f32 Length() const;
  f32 LengthSqr() const;

  // negate the QAngle components
  // void	Negate();

  // No assignment operators either...
  QAngle& operator=(const QAngle& src);

#ifndef VECTOR_NO_SLOW_OPERATIONS
  // copy constructors

  // arithmetic operations
  QAngle operator-(void) const;

  QAngle operator+(const QAngle& v) const;
  QAngle operator-(const QAngle& v) const;
  QAngle operator*(f32 fl) const;
  QAngle operator/(f32 fl) const;
#else

 private:
  // No copy constructors allowed if we're in optimal mode
  QAngle(const QAngle& vOther);

#endif
};

// Allows us to specifically pass the vector by value when we need to

class QAngleByValue : public QAngle {
 public:
  // Construction/destruction:
  QAngleByValue(void) : QAngle() {}
  QAngleByValue(f32 X, f32 Y, f32 Z) : QAngle(X, Y, Z) {}
  QAngleByValue(const QAngleByValue& vOther) { *this = vOther; }
};

inline void VectorAdd(const QAngle& a, const QAngle& b, QAngle& result) {
  CHECK_VALID(a);
  CHECK_VALID(b);
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
}

inline void VectorMA(const QAngle& start, f32 scale, const QAngle& direction,
                     QAngle& dest) {
  CHECK_VALID(start);
  CHECK_VALID(direction);
  dest.x = start.x + scale * direction.x;
  dest.y = start.y + scale * direction.y;
  dest.z = start.z + scale * direction.z;
}

// constructors

inline QAngle::QAngle() {
#ifdef _DEBUG
#ifdef VECTOR_PARANOIA
  // Initialize to NAN to catch errors
  x = y = z = VEC_T_NAN;
#endif
#endif
}

inline QAngle::QAngle(f32 X, f32 Y, f32 Z) {
  x = X;
  y = Y;
  z = Z;
  CHECK_VALID(*this);
}

// initialization

inline void QAngle::Init(f32 ix, f32 iy, f32 iz) {
  x = ix;
  y = iy;
  z = iz;
  CHECK_VALID(*this);
}

inline void QAngle::Random(f32 minVal, f32 maxVal) {
  x = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  y = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  z = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  CHECK_VALID(*this);
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline QAngle RandomAngle(f32 minVal, f32 maxVal) {
  Vector r;
  r.Random(minVal, maxVal);
  QAngle ret(r.x, r.y, r.z);
  return ret;
}

#endif

inline RadianEuler::RadianEuler(QAngle const& angles) {
  Init(angles.z * 3.14159265358979323846f / 180.f,
       angles.x * 3.14159265358979323846f / 180.f,
       angles.y * 3.14159265358979323846f / 180.f);
}

inline QAngle RadianEuler::ToQAngle() const {
  return QAngle(y * 180.f / 3.14159265358979323846f,
                z * 180.f / 3.14159265358979323846f,
                x * 180.f / 3.14159265358979323846f);
}

// assignment

inline QAngle& QAngle::operator=(const QAngle& vOther) {
  CHECK_VALID(vOther);
  x = vOther.x;
  y = vOther.y;
  z = vOther.z;
  return *this;
}

// Array access

inline f32& QAngle::operator[](int i) {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

inline f32 QAngle::operator[](int i) const {
  Assert((i >= 0) && (i < 3));
  return ((f32*)this)[i];
}

// Base address...

inline f32* QAngle::Base() { return (f32*)this; }

inline f32 const* QAngle::Base() const { return (f32 const*)this; }

// IsValid?

inline bool QAngle::IsValid() const {
  return IsFinite(x) && IsFinite(y) && IsFinite(z);
}

// Invalidate

inline void QAngle::Invalidate() {
  //#ifdef _DEBUG
  //#ifdef VECTOR_PARANOIA
  x = y = z = FLOAT32_NAN;
  //#endif
  //#endif
}

// comparison

inline bool QAngle::operator==(const QAngle& src) const {
  CHECK_VALID(src);
  CHECK_VALID(*this);
  return (src.x == x) && (src.y == y) && (src.z == z);
}

inline bool QAngle::operator!=(const QAngle& src) const {
  CHECK_VALID(src);
  CHECK_VALID(*this);
  return (src.x != x) || (src.y != y) || (src.z != z);
}

// Copy

inline void VectorCopy(const QAngle& src, QAngle& dst) {
  CHECK_VALID(src);
  dst.x = src.x;
  dst.y = src.y;
  dst.z = src.z;
}

// standard math operations

inline QAngle& QAngle::operator+=(const QAngle& v) {
  CHECK_VALID(*this);
  CHECK_VALID(v);
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

inline QAngle& QAngle::operator-=(const QAngle& v) {
  CHECK_VALID(*this);
  CHECK_VALID(v);
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

inline QAngle& QAngle::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  z *= fl;
  CHECK_VALID(*this);
  return *this;
}

inline QAngle& QAngle::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  z *= oofl;
  CHECK_VALID(*this);
  return *this;
}

// length

inline f32 QAngle::Length() const {
  CHECK_VALID(*this);
  return (f32)FastSqrt(LengthSqr());
}

inline f32 QAngle::LengthSqr() const {
  CHECK_VALID(*this);
  return x * x + y * y + z * z;
}

// Vector equality with tolerance

inline bool QAnglesAreEqual(const QAngle& src1, const QAngle& src2,
                            f32 tolerance = 0.0f) {
  if (FloatMakePositive(src1.x - src2.x) > tolerance) return false;
  if (FloatMakePositive(src1.y - src2.y) > tolerance) return false;
  return (FloatMakePositive(src1.z - src2.z) <= tolerance);
}

  // arithmetic operations (SLOW!!)

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline QAngle QAngle::operator-() const {
  QAngle ret(-x, -y, -z);
  return ret;
}

inline QAngle QAngle::operator+(const QAngle& v) const {
  QAngle res;
  res.x = x + v.x;
  res.y = y + v.y;
  res.z = z + v.z;
  return res;
}

inline QAngle QAngle::operator-(const QAngle& v) const {
  QAngle res;
  res.x = x - v.x;
  res.y = y - v.y;
  res.z = z - v.z;
  return res;
}

inline QAngle QAngle::operator*(f32 fl) const {
  QAngle res;
  res.x = x * fl;
  res.y = y * fl;
  res.z = z * fl;
  return res;
}

inline QAngle QAngle::operator/(f32 fl) const {
  QAngle res;
  res.x = x / fl;
  res.y = y / fl;
  res.z = z / fl;
  return res;
}

inline QAngle operator*(f32 fl, const QAngle& v) {
  QAngle ret(v * fl);
  return ret;
}

#endif  // VECTOR_NO_SLOW_OPERATIONS

// NOTE: These are not completely correct.  The representations are not
// equivalent unless the QAngle represents a rotational impulse along a
// coordinate axis (x,y,z)
inline void QAngleToAngularImpulse(const QAngle& angles,
                                   AngularImpulse& impulse) {
  impulse.x = angles.z;
  impulse.y = angles.x;
  impulse.z = angles.y;
}

inline void AngularImpulseToQAngle(const AngularImpulse& impulse,
                                   QAngle& angles) {
  angles.x = impulse.y;
  angles.y = impulse.z;
  angles.z = impulse.x;
}

extern f32 (*pfInvRSquared)(const f32* v);

SOURCE_FORCEINLINE f32 InvRSquared(f32 const* v) { return (*pfInvRSquared)(v); }
SOURCE_FORCEINLINE f32 InvRSquared(const Vector& v) {
  return InvRSquared(&v.x);
}

extern f32(SOURCE_FASTCALL* pfVectorNormalize)(Vector& v);

// TODO(d.rattman): Change this back to a #define once we get rid of the f32
// version
SOURCE_FORCEINLINE f32 VectorNormalize(Vector& v) {
  return (*pfVectorNormalize)(v);
}
// TODO(d.rattman): Obsolete version of VectorNormalize, once we remove all the
// friggin f32*s
SOURCE_FORCEINLINE f32 VectorNormalize(f32* v) {
  return VectorNormalize(*(reinterpret_cast<Vector*>(v)));
}

extern void(SOURCE_FASTCALL* pfVectorNormalizeFast)(Vector& v);

SOURCE_FORCEINLINE void VectorNormalizeFast(Vector& v) {
  (*pfVectorNormalizeFast)(v);
}

inline f32 Vector::NormalizeInPlace() { return VectorNormalize(*this); }

inline bool Vector::IsLengthGreaterThan(f32 val) const {
  return LengthSqr() > val * val;
}

inline bool Vector::IsLengthLessThan(f32 val) const {
  return LengthSqr() < val * val;
}

#endif  // SOURCE_MATHLIB_VECTOR_H_
