// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_MATHLIB_H_
#define SOURCE_MATHLIB_MATHLIB_H_

#include <algorithm>
#include <cmath>

#include "base/include/base_types.h"
#include "mathlib/math_pfns.h"
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
// FIXME: does the asm code even exist anymore?
// FIXME: this should move to a different file
struct cplane_t {
  Vector normal;
  f32 dist;
  u8 type;      // for fast side tests
  u8 signbits;  // signx + (signy<<1) + (signz<<1)
  u8 pad[2];

#ifdef VECTOR_NO_SLOW_OPERATIONS
  cplane_t() {}

 private:
  // No copy constructors allowed if we're in optimal mode
  cplane_t(const cplane_t &vOther);
#endif
};

// structure offset for asm code
#define CPLANE_NORMAL_X 0
#define CPLANE_NORMAL_Y 4
#define CPLANE_NORMAL_Z 8
#define CPLANE_DIST 12
#define CPLANE_TYPE 16
#define CPLANE_SIGNBITS 17
#define CPLANE_PAD0 18
#define CPLANE_PAD1 19

// 0-2 are axial planes
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2

// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX 3
#define PLANE_ANYY 4
#define PLANE_ANYZ 5

// Frustum plane indices.
// WARNING: there is code that depends on these values

enum {
  FRUSTUM_RIGHT = 0,
  FRUSTUM_LEFT = 1,
  FRUSTUM_TOP = 2,
  FRUSTUM_BOTTOM = 3,
  FRUSTUM_NEARZ = 4,
  FRUSTUM_FARZ = 5,
  FRUSTUM_NUMPLANES = 6
};

extern int SignbitsForPlane(cplane_t *out);

class Frustum_t {
 public:
  void SetPlane(int i, int nType, const Vector &vecNormal, f32 dist) {
    m_Plane[i].normal = vecNormal;
    m_Plane[i].dist = dist;
    m_Plane[i].type = nType;
    m_Plane[i].signbits = SignbitsForPlane(&m_Plane[i]);
    m_AbsNormal[i].Init(fabs(vecNormal.x), fabs(vecNormal.y),
                        fabs(vecNormal.z));
  }

  inline const cplane_t *GetPlane(int i) const { return &m_Plane[i]; }
  inline const Vector &GetAbsNormal(int i) const { return m_AbsNormal[i]; }

 private:
  cplane_t m_Plane[FRUSTUM_NUMPLANES];
  Vector m_AbsNormal[FRUSTUM_NUMPLANES];
};

// Computes Y fov from an X fov and a screen aspect ratio + X from Y
f32 CalcFovY(f32 flFovX, f32 flScreenAspect);
f32 CalcFovX(f32 flFovY, f32 flScreenAspect);

// Generate a frustum based on perspective view parameters
// NOTE: FOV is specified in degrees, as the *full* view angle (not half-angle)
void GeneratePerspectiveFrustum(const Vector &origin, const QAngle &angles,
                                f32 flZNear, f32 flZFar, f32 flFovX,
                                f32 flAspectRatio, Frustum_t &frustum);
void GeneratePerspectiveFrustum(const Vector &origin, const Vector &forward,
                                const Vector &right, const Vector &up,
                                f32 flZNear, f32 flZFar, f32 flFovX, f32 flFovY,
                                Frustum_t &frustum);

// Cull the world-space bounding box to the specified frustum.
bool R_CullBox(const Vector &mins, const Vector &maxs,
               const Frustum_t &frustum);
bool R_CullBoxSkipNear(const Vector &mins, const Vector &maxs,
                       const Frustum_t &frustum);

struct matrix3x4_t {
  matrix3x4_t() {}  //-V730
  matrix3x4_t(f32 m00, f32 m01, f32 m02, f32 m03, f32 m10, f32 m11, f32 m12,
              f32 m13, f32 m20, f32 m21, f32 m22, f32 m23) {
    m_flMatVal[0][0] = m00;
    m_flMatVal[0][1] = m01;
    m_flMatVal[0][2] = m02;
    m_flMatVal[0][3] = m03;
    m_flMatVal[1][0] = m10;
    m_flMatVal[1][1] = m11;
    m_flMatVal[1][2] = m12;
    m_flMatVal[1][3] = m13;
    m_flMatVal[2][0] = m20;
    m_flMatVal[2][1] = m21;
    m_flMatVal[2][2] = m22;
    m_flMatVal[2][3] = m23;
  }

  // Creates a matrix where the X axis = forward
  // the Y axis = left, and the Z axis = up

  void Init(const Vector &xAxis, const Vector &yAxis, const Vector &zAxis,
            const Vector &vecOrigin) {
    m_flMatVal[0][0] = xAxis.x;
    m_flMatVal[0][1] = yAxis.x;
    m_flMatVal[0][2] = zAxis.x;
    m_flMatVal[0][3] = vecOrigin.x;
    m_flMatVal[1][0] = xAxis.y;
    m_flMatVal[1][1] = yAxis.y;
    m_flMatVal[1][2] = zAxis.y;
    m_flMatVal[1][3] = vecOrigin.y;
    m_flMatVal[2][0] = xAxis.z;
    m_flMatVal[2][1] = yAxis.z;
    m_flMatVal[2][2] = zAxis.z;
    m_flMatVal[2][3] = vecOrigin.z;
  }

  // Creates a matrix where the X axis = forward
  // the Y axis = left, and the Z axis = up

  matrix3x4_t(const Vector &xAxis, const Vector &yAxis, const Vector &zAxis,
              const Vector &vecOrigin) {
    Init(xAxis, yAxis, zAxis, vecOrigin);
  }

  inline void Invalidate(void) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 4; j++) {
        m_flMatVal[i][j] = VEC_T_NAN;
      }
    }
  }

  f32 *operator[](int i) {
    Assert((i >= 0) && (i < 3));
    return m_flMatVal[i];
  }
  const f32 *operator[](int i) const {
    Assert((i >= 0) && (i < 3));
    return m_flMatVal[i];
  }
  f32 *Base() { return &m_flMatVal[0][0]; }
  const f32 *Base() const { return &m_flMatVal[0][0]; }

  f32 m_flMatVal[3][4];
};

#ifndef M_PI
#define M_PI 3.14159265358979323846  // matches value in gcc v2 math.h
#endif

#define M_PI_F ((f32)(M_PI))  // Shouldn't collide with anything.

// NJS: Inlined to prevent floats from being autopromoted to doubles, as with
// the old system.
#ifndef RAD2DEG
#define RAD2DEG(x) ((f32)(x) * (f32)(180.f / M_PI_F))
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) ((f32)(x) * (f32)(M_PI_F / 180.f))
#endif

// Used to represent sides of things like planes.
#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS -2  // necessary for polylib.c

#define ON_VIS_EPSILON \
  0.01  // necessary for vvis (flow.c) -- again look into moving later!
#define EQUAL_EPSILON \
  0.001  // necessary for vbsp (faces.c) -- should look into moving it there?

extern bool s_bMathlibInitialized;

extern const Vector vec3_origin;
extern const QAngle vec3_angle;
extern const Vector vec3_invalid;
extern const int nanmask;

#define IS_NAN(x) (((*(int *)&x) & nanmask) == nanmask)

FORCEINLINE f32 DotProduct(const f32 *v1, const f32 *v2) {
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}
FORCEINLINE void VectorSubtract(const f32 *a, const f32 *b, f32 *c) {
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  c[2] = a[2] - b[2];
}
FORCEINLINE void VectorAdd(const f32 *a, const f32 *b, f32 *c) {
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  c[2] = a[2] + b[2];
}
FORCEINLINE void VectorCopy(const f32 *a, f32 *b) {
  b[0] = a[0];
  b[1] = a[1];
  b[2] = a[2];
}
FORCEINLINE void VectorClear(f32 *a) { a[0] = a[1] = a[2] = 0; }

FORCEINLINE f32 VectorMaximum(const f32 *v) {
  return std::max(v[0], std::max(v[1], v[2]));
}

FORCEINLINE f32 VectorMaximum(const Vector &v) {
  return std::max(v.x, std::max(v.y, v.z));
}

FORCEINLINE void VectorScale(const f32 *in, f32 scale, f32 *out) {
  out[0] = in[0] * scale;
  out[1] = in[1] * scale;
  out[2] = in[2] * scale;
}

// Cannot be forceinline as they have overloads:
inline void VectorFill(f32 *a, f32 b) { a[0] = a[1] = a[2] = b; }

inline void VectorNegate(f32 *a) {
  a[0] = -a[0];
  a[1] = -a[1];
  a[2] = -a[2];
}

//#define VectorMaximum(a)		( std::max( (a)[0], std::max( (a)[1], (a)[2] ) ) )
#define Vector2Clear(x) \
  { (x)[0] = (x)[1] = 0; }
#define Vector2Negate(x) \
  {                      \
    (x)[0] = -((x)[0]);  \
    (x)[1] = -((x)[1]);  \
  }
#define Vector2Copy(a, b) \
  {                       \
    (b)[0] = (a)[0];      \
    (b)[1] = (a)[1];      \
  }
#define Vector2Subtract(a, b, c) \
  {                              \
    (c)[0] = (a)[0] - (b)[0];    \
    (c)[1] = (a)[1] - (b)[1];    \
  }
#define Vector2Add(a, b, c)   \
  {                           \
    (c)[0] = (a)[0] + (b)[0]; \
    (c)[1] = (a)[1] + (b)[1]; \
  }
#define Vector2Scale(a, b, c) \
  {                           \
    (c)[0] = (b) * (a)[0];    \
    (c)[1] = (b) * (a)[1];    \
  }

// NJS: Some functions in VBSP still need to use these for dealing with mixing
// vec4's and shorts with f32's. remove when no longer needed.
#define VECTOR_COPY(A, B) \
  do {                    \
    (B)[0] = (A)[0];      \
    (B)[1] = (A)[1];      \
    (B)[2] = (A)[2];      \
  } while (0)
#define DOT_PRODUCT(A, B) ((A)[0] * (B)[0] + (A)[1] * (B)[1] + (A)[2] * (B)[2])

FORCEINLINE void VectorMAInline(const f32 *start, f32 scale,
                                const f32 *direction, f32 *dest) {
  dest[0] = start[0] + direction[0] * scale;
  dest[1] = start[1] + direction[1] * scale;
  dest[2] = start[2] + direction[2] * scale;
}

FORCEINLINE void VectorMAInline(const Vector &start, f32 scale,
                                const Vector &direction, Vector &dest) {
  dest.x = start.x + direction.x * scale;
  dest.y = start.y + direction.y * scale;
  dest.z = start.z + direction.z * scale;
}

FORCEINLINE void VectorMA(const Vector &start, f32 scale,
                          const Vector &direction, Vector &dest) {
  VectorMAInline(start, scale, direction, dest);
}

FORCEINLINE void VectorMA(const f32 *start, f32 scale, const f32 *direction,
                          f32 *dest) {
  VectorMAInline(start, scale, direction, dest);
}

int VectorCompare(const f32 *v1, const f32 *v2);

inline f32 VectorLength(const f32 *v) {
  return FastSqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + FLT_EPSILON);
}

void CrossProduct(const f32 *v1, const f32 *v2, f32 *cross);

bool VectorsEqual(const f32 *v1, const f32 *v2);

inline f32 RoundInt(f32 in) { return floor(in + 0.5f); }

int Q_log2(int val);

// Math routines done in optimized assembly math package routines
void inline SinCos(f32 radians, f32 *sine, f32 *cosine) {
#if defined(_WIN32)
  *sine = sin(radians);
  *cosine = cos(radians);
#elif defined(_LINUX)
  register f64 __cosr, __sinr;
  __asm __volatile__("fsincos" : "=t"(__cosr), "=u"(__sinr) : "0"(radians));

  *sine = __sinr;
  *cosine = __cosr;
#endif
}

#define SIN_TABLE_SIZE 256
#define FTOIBIAS 12582912.f
extern f32 SinCosTable[SIN_TABLE_SIZE];

inline f32 TableCos(f32 theta) {
  union {
    int i;
    f32 f;
  } ftmp;

  // ideally, the following should compile down to: theta * constant + constant,
  // changing any of these constants from defines sometimes fubars this.
  ftmp.f = theta * (f32)(SIN_TABLE_SIZE / (2.0f * M_PI)) +
           (FTOIBIAS + (SIN_TABLE_SIZE / 4));
  return SinCosTable[ftmp.i & (SIN_TABLE_SIZE - 1)];
}

inline f32 TableSin(f32 theta) {
  union {
    int i;
    f32 f;
  } ftmp;

  // ideally, the following should compile down to: theta * constant + constant
  ftmp.f = theta * (f32)(SIN_TABLE_SIZE / (2.0f * M_PI)) + FTOIBIAS;
  return SinCosTable[ftmp.i & (SIN_TABLE_SIZE - 1)];
}

template <class T>
FORCEINLINE T Square(T const &a) {
  return a * a;
}

FORCEINLINE bool IsPowerOfTwo(uint32_t x) { return (x & (x - 1)) == 0; }

// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x80000000 (ie numbers that would be negative if x
// was signed) NOTE: the old code took an int, and if you pass in an int of
// 0x80000000 casted to a u32,
//       you'll get 0x80000000, which is correct for uints, instead of 0, which
//       was correct for ints
FORCEINLINE uint32_t SmallestPowerOfTwoGreaterOrEqual(uint32_t x) {
  x -= 1;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
FORCEINLINE uint32_t LargestPowerOfTwoLessThanOrEqual(uint32_t x) {
  if (x >= 0x80000000) return 0x80000000;

  return SmallestPowerOfTwoGreaterOrEqual(x + 1) >> 1;
}

// Math routines for optimizing division
void FloorDivMod(f64 numer, f64 denom, int *quotient, int *rem);
int GreatestCommonDivisor(int i1, int i2);

// Test for FPU denormal mode
bool IsDenormal(const f32 &val);

// MOVEMENT INFO
enum {
  PITCH = 0,  // up / down
  YAW,        // left / right
  ROLL        // fall over
};

void MatrixAngles(const matrix3x4_t &matrix, f32 *angles);  // !!!!
void MatrixVectors(const matrix3x4_t &matrix, Vector *pForward, Vector *pRight,
                   Vector *pUp);
void VectorTransform(const f32 *in1, const matrix3x4_t &in2, f32 *out);
void VectorITransform(const f32 *in1, const matrix3x4_t &in2, f32 *out);
void VectorRotate(const f32 *in1, const matrix3x4_t &in2, f32 *out);
void VectorRotate(const Vector &in1, const QAngle &in2, Vector &out);
void VectorRotate(const Vector &in1, const Quaternion &in2, Vector &out);
void VectorIRotate(const f32 *in1, const matrix3x4_t &in2, f32 *out);

#ifndef VECTOR_NO_SLOW_OPERATIONS

QAngle TransformAnglesToLocalSpace(const QAngle &angles,
                                   const matrix3x4_t &parentMatrix);
QAngle TransformAnglesToWorldSpace(const QAngle &angles,
                                   const matrix3x4_t &parentMatrix);

#endif

void MatrixInitialize(matrix3x4_t &mat, const Vector &vecOrigin,
                      const Vector &vecXAxis, const Vector &vecYAxis,
                      const Vector &vecZAxis);
void MatrixCopy(const matrix3x4_t &in, matrix3x4_t &out);
void MatrixInvert(const matrix3x4_t &in, matrix3x4_t &out);

// Matrix equality test
bool MatricesAreEqual(const matrix3x4_t &src1, const matrix3x4_t &src2,
                      f32 flTolerance = 1e-5);

void MatrixGetColumn(const matrix3x4_t &in, int column, Vector &out);
void MatrixSetColumn(const Vector &in, int column, matrix3x4_t &out);

// void DecomposeRotation( const matrix3x4_t &mat, f32 *out );
void ConcatRotations(const matrix3x4_t &in1, const matrix3x4_t &in2,
                     matrix3x4_t &out);
void ConcatTransforms(const matrix3x4_t &in1, const matrix3x4_t &in2,
                      matrix3x4_t &out);

// For identical interface w/ VMatrix
inline void MatrixMultiply(const matrix3x4_t &in1, const matrix3x4_t &in2,
                           matrix3x4_t &out) {
  ConcatTransforms(in1, in2, out);
}

void QuaternionSlerp(const Quaternion &p, const Quaternion &q, f32 t,
                     Quaternion &qt);
void QuaternionSlerpNoAlign(const Quaternion &p, const Quaternion &q, f32 t,
                            Quaternion &qt);
void QuaternionBlend(const Quaternion &p, const Quaternion &q, f32 t,
                     Quaternion &qt);
void QuaternionBlendNoAlign(const Quaternion &p, const Quaternion &q, f32 t,
                            Quaternion &qt);
void QuaternionIdentityBlend(const Quaternion &p, f32 t, Quaternion &qt);
f32 QuaternionAngleDiff(const Quaternion &p, const Quaternion &q);
void QuaternionScale(const Quaternion &p, f32 t, Quaternion &q);
void QuaternionAlign(const Quaternion &p, const Quaternion &q, Quaternion &qt);
f32 QuaternionDotProduct(const Quaternion &p, const Quaternion &q);
void QuaternionConjugate(const Quaternion &p, Quaternion &q);
void QuaternionInvert(const Quaternion &p, Quaternion &q);
f32 QuaternionNormalize(Quaternion &q);
void QuaternionAdd(const Quaternion &p, const Quaternion &q, Quaternion &qt);
void QuaternionMult(const Quaternion &p, const Quaternion &q, Quaternion &qt);
void QuaternionMatrix(const Quaternion &q, matrix3x4_t &matrix);
void QuaternionMatrix(const Quaternion &q, const Vector &pos,
                      matrix3x4_t &matrix);
void QuaternionAngles(const Quaternion &q, QAngle &angles);
void AngleQuaternion(const QAngle &angles, Quaternion &qt);
void QuaternionAngles(const Quaternion &q, RadianEuler &angles);
void AngleQuaternion(RadianEuler const &angles, Quaternion &qt);
void QuaternionAxisAngle(const Quaternion &q, Vector &axis, f32 &angle);
void AxisAngleQuaternion(const Vector &axis, f32 angle, Quaternion &q);
void BasisToQuaternion(const Vector &vecForward, const Vector &vecRight,
                       const Vector &vecUp, Quaternion &q);
void MatrixQuaternion(const matrix3x4_t &mat, Quaternion &q);

// A couple methods to find the dot product of a vector with a matrix row or
// column...
inline f32 MatrixRowDotProduct(const matrix3x4_t &in1, int row,
                               const Vector &in2) {
  Assert((row >= 0) && (row < 3));
  return DotProduct(in1[row], in2.Base());
}

inline f32 MatrixColumnDotProduct(const matrix3x4_t &in1, int col,
                                  const Vector &in2) {
  Assert((col >= 0) && (col < 4));
  return in1[0][col] * in2[0] + in1[1][col] * in2[1] + in1[2][col] * in2[2];
}

int __cdecl BoxOnPlaneSide(const f32 *emins, const f32 *emaxs,
                           const cplane_t *plane);

inline f32 anglemod(f32 a) {
  a = (360.f / 65536) * ((int)(a * (65536.f / 360.0f)) & 65535);
  return a;
}

// Remap a value in the range [A,B] to [C,D].
inline f32 RemapVal(f32 val, f32 A, f32 B, f32 C, f32 D) {
  if (A == B) return val >= B ? D : C;
  return C + (D - C) * (val - A) / (B - A);
}

inline f32 RemapValClamped(f32 val, f32 A, f32 B, f32 C, f32 D) {
  if (A == B) return val >= B ? D : C;
  f32 cVal = (val - A) / (B - A);
  cVal = std::clamp(cVal, 0.0f, 1.0f);

  return C + (D - C) * cVal;
}

// Returns A + (B-A)*flPercent.
// f32 Lerp( f32 flPercent, f32 A, f32 B );
template <class T>
FORCEINLINE T Lerp(f32 flPercent, T const &A, T const &B) {
  return A + (B - A) * flPercent;
}

// 5-argument floating point linear interpolation.
// FLerp(f1,f2,i1,i2,x)=
//    f1 at x=i1
//    f2 at x=i2
//   smooth lerp between f1 and f2 at x>i1 and x<i2
//   extrapolation for x<i1 or x>i2
//
//   If you know a function f(x)'s value (f1) at position i1, and its value (f2)
//   at position i2, the function can be linearly interpolated with
//   FLerp(f1,f2,i1,i2,x)
//    i2=i1 will cause a divide by zero.
static inline f32 FLerp(f32 f1, f32 f2, f32 i1, f32 i2, f32 x) {
  return f1 + (f2 - f1) * (x - i1) / (i2 - i1);
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

// YWB:  Specialization for interpolating euler angles via quaternions...
template <>
FORCEINLINE QAngle Lerp<QAngle>(f32 flPercent, const QAngle &q1,
                                const QAngle &q2) {
  // Avoid precision errors
  if (q1 == q2) return q1;

  Quaternion src, dest;

  // Convert to quaternions
  AngleQuaternion(q1, src);
  AngleQuaternion(q2, dest);

  Quaternion result;

  // Slerp
  QuaternionSlerp(src, dest, flPercent, result);

  // Convert to euler
  QAngle output;
  QuaternionAngles(result, output);
  return output;
}

#else

#pragma error

// NOTE NOTE: I haven't tested this!! It may not work! Check out
// interpolatedvar.cpp in the client dll to try it
template <>
FORCEINLINE QAngleByValue Lerp<QAngleByValue>(f32 flPercent,
                                              const QAngleByValue &q1,
                                              const QAngleByValue &q2) {
  // Avoid precision errors
  if (q1 == q2) return q1;

  Quaternion src, dest;

  // Convert to quaternions
  AngleQuaternion(q1, src);
  AngleQuaternion(q2, dest);

  Quaternion result;

  // Slerp
  QuaternionSlerp(src, dest, flPercent, result);

  // Convert to euler
  QAngleByValue output;
  QuaternionAngles(result, output);
  return output;
}

#endif  // VECTOR_NO_SLOW_OPERATIONS

// Swap two of anything.
template <class T>
FORCEINLINE void swap(T &x, T &y) {
  T temp = x;
  x = y;
  y = temp;
}

template <class T>
FORCEINLINE T AVG(T a, T b) {
  return (a + b) / 2;
}

// number of elements in an array of static size
#define NELEMS(x) ((sizeof(x)) / sizeof(x[0]))

// XYZ macro, for printf type functions - ex printf("%f %f %f",XYZ(myvector));
#define XYZ(v) (v).x, (v).y, (v).z

inline f32 Sign(f32 x) { return (x < 0.0f) ? -1.0f : 1.0f; }

//
// Clamps the input integer to the given array bounds.
// Equivalent to the following, but without using any branches:
//
// if( n < 0 ) return 0;
// else if ( n > maxindex ) return maxindex;
// else return n;
//
// This is not always a clear performance win, but when you have situations
// where a clamped value is thrashing against a boundary this is a big win. (ie,
// valid, invalid, valid, invalid, ...)
//
// Note: This code has been run against all possible integers.
//
inline int ClampArrayBounds(int n, u32 maxindex) {
  // mask is 0 if less than 4096, 0xFFFFFFFF if greater than
  u32 inrangemask = 0xFFFFFFFF + (((u32)n) > maxindex);
  u32 lessthan0mask = 0xFFFFFFFF + (n >= 0);

  // If the result was valid, set the result, (otherwise sets zero)
  int result = (inrangemask & n);

  // if the result was out of range or zero.
  result |= ((~inrangemask) & (~lessthan0mask)) & maxindex;

  return result;
}

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)                               \
  (((p)->type < 3) ? (((p)->dist <= (emins)[(p)->type])                  \
                          ? 1                                            \
                          : (((p)->dist >= (emaxs)[(p)->type]) ? 2 : 3)) \
                   : BoxOnPlaneSide((emins), (emaxs), (p)))

// FIXME: Vector versions.... the f32 versions will go away hopefully soon!

void AngleVectors(const QAngle &angles, Vector *forward);
void AngleVectors(const QAngle &angles, Vector *forward, Vector *right,
                  Vector *up);
void AngleVectorsTranspose(const QAngle &angles, Vector *forward, Vector *right,
                           Vector *up);
void AngleMatrix(const QAngle &angles, matrix3x4_t &mat);
void AngleMatrix(const QAngle &angles, const Vector &position,
                 matrix3x4_t &mat);
void AngleMatrix(const RadianEuler &angles, matrix3x4_t &mat);
void AngleMatrix(RadianEuler const &angles, const Vector &position,
                 matrix3x4_t &mat);
void AngleIMatrix(const QAngle &angles, matrix3x4_t &mat);
void AngleIMatrix(const QAngle &angles, const Vector &position,
                  matrix3x4_t &mat);
void AngleIMatrix(const RadianEuler &angles, matrix3x4_t &mat);
void VectorAngles(const Vector &forward, QAngle &angles);
void VectorAngles(const Vector &forward, const Vector &pseudoup,
                  QAngle &angles);
void VectorMatrix(const Vector &forward, matrix3x4_t &mat);
void VectorVectors(const Vector &forward, Vector &right, Vector &up);
void SetIdentityMatrix(matrix3x4_t &mat);
void SetScaleMatrix(f32 x, f32 y, f32 z, matrix3x4_t &dst);
void MatrixBuildRotationAboutAxis(const Vector &vAxisOfRot, f32 angleDegrees,
                                  matrix3x4_t &dst);

inline void SetScaleMatrix(f32 flScale, matrix3x4_t &dst) {
  SetScaleMatrix(flScale, flScale, flScale, dst);
}

inline void SetScaleMatrix(const Vector &scale, matrix3x4_t &dst) {
  SetScaleMatrix(scale.x, scale.y, scale.z, dst);
}

// Computes the inverse transpose
void MatrixTranspose(matrix3x4_t &mat);
void MatrixTranspose(const matrix3x4_t &src, matrix3x4_t &dst);
void MatrixInverseTranspose(const matrix3x4_t &src, matrix3x4_t &dst);

inline void PositionMatrix(const Vector &position, matrix3x4_t &mat) {
  MatrixSetColumn(position, 3, mat);
}

inline void MatrixPosition(const matrix3x4_t &matrix, Vector &position) {
  MatrixGetColumn(matrix, 3, position);
}

inline void VectorRotate(const Vector &in1, const matrix3x4_t &in2,
                         Vector &out) {
  VectorRotate(&in1.x, in2, &out.x);
}

inline void VectorIRotate(const Vector &in1, const matrix3x4_t &in2,
                          Vector &out) {
  VectorIRotate(&in1.x, in2, &out.x);
}

inline void MatrixAngles(const matrix3x4_t &matrix, QAngle &angles) {
  MatrixAngles(matrix, &angles.x);
}

inline void MatrixAngles(const matrix3x4_t &matrix, QAngle &angles,
                         Vector &position) {
  MatrixAngles(matrix, angles);
  MatrixPosition(matrix, position);
}

inline void MatrixAngles(const matrix3x4_t &matrix, RadianEuler &angles) {
  MatrixAngles(matrix, &angles.x);

  angles.Init(DEG2RAD(angles.z), DEG2RAD(angles.x), DEG2RAD(angles.y));
}

void MatrixAngles(const matrix3x4_t &mat, RadianEuler &angles,
                  Vector &position);

void MatrixAngles(const matrix3x4_t &mat, Quaternion &q, Vector &position);

inline int VectorCompare(const Vector &v1, const Vector &v2) {
  return v1 == v2;
}

inline void VectorTransform(const Vector &in1, const matrix3x4_t &in2,
                            Vector &out) {
  VectorTransform(&in1.x, in2, &out.x);
}

inline void VectorITransform(const Vector &in1, const matrix3x4_t &in2,
                             Vector &out) {
  VectorITransform(&in1.x, in2, &out.x);
}

/*
inline void DecomposeRotation( const matrix3x4_t &mat, Vector &out )
{
        DecomposeRotation( mat, &out.x );
}
*/

inline int BoxOnPlaneSide(const Vector &emins, const Vector &emaxs,
                          const cplane_t *plane) {
  return BoxOnPlaneSide(&emins.x, &emaxs.x, plane);
}

inline void VectorFill(Vector &a, f32 b) { a[0] = a[1] = a[2] = b; }

inline void VectorNegate(Vector &a) {
  a[0] = -a[0];
  a[1] = -a[1];
  a[2] = -a[2];
}

inline f32 VectorAvg(Vector &a) { return (a[0] + a[1] + a[2]) / 3; }

// Box/plane test (slow version)

inline int FASTCALL BoxOnPlaneSide2(const Vector &emins, const Vector &emaxs,
                                    const cplane_t *p, f32 tolerance = 0.f) {
  Vector corners[2];

  if (p->normal[0] < 0) {
    corners[0][0] = emins[0];
    corners[1][0] = emaxs[0];
  } else {
    corners[1][0] = emins[0];
    corners[0][0] = emaxs[0];
  }

  if (p->normal[1] < 0) {
    corners[0][1] = emins[1];
    corners[1][1] = emaxs[1];
  } else {
    corners[1][1] = emins[1];
    corners[0][1] = emaxs[1];
  }

  if (p->normal[2] < 0) {
    corners[0][2] = emins[2];
    corners[1][2] = emaxs[2];
  } else {
    corners[1][2] = emins[2];
    corners[0][2] = emaxs[2];
  }

  int sides = 0;

  f32 dist1 = DotProduct(p->normal, corners[0]) - p->dist;
  if (dist1 >= tolerance) sides = 1;

  f32 dist2 = DotProduct(p->normal, corners[1]) - p->dist;
  if (dist2 < -tolerance) sides |= 2;

  return sides;
}

// Helpers for bounding box construction

void ClearBounds(Vector &mins, Vector &maxs);
void AddPointToBounds(const Vector &v, Vector &mins, Vector &maxs);

//
// COLORSPACE/GAMMA CONVERSION STUFF
//
void BuildGammaTable(f32 gamma, f32 texGamma, f32 brightness, int overbright);

// convert texture to linear 0..1 value
inline f32 TexLightToLinear(int c, int exponent) {
  extern f32 power2_n[256];
  Assert(exponent >= -128 && exponent <= 127);
  return (f32)c * power2_n[exponent + 128];
}

// convert texture to linear 0..1 value
u8 LinearToTexture(f32 f);
// converts 0..1 linear value to screen gamma (0..255)
u8 LinearToScreenGamma(f32 f);
f32 TextureToLinear(int c);

// compressed color format
struct ColorRGBExp32 {
  u8 r, g, b;
  signed char exponent;
};

void ColorRGBExp32ToVector(const ColorRGBExp32 &in, Vector &out);
void VectorToColorRGBExp32(const Vector &v, ColorRGBExp32 &c);

// solve for "x" where "a x^2 + b x + c = 0", return true if solution exists
bool SolveQuadratic(f32 a, f32 b, f32 c, f32 &root1, f32 &root2);

// solves for "a, b, c" where "a x^2 + b x + c = y", return true if solution
// exists
bool SolveInverseQuadratic(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3,
                           f32 &a, f32 &b, f32 &c);

// solves for a,b,c specified as above, except that it always creates a
// monotonically increasing or decreasing curve if the data is monotonically
// increasing or decreasing. In order to enforce the monoticity condition, it is
// possible that the resulting quadratic will only approximate the data instead
// of interpolating it. This code is not especially fast.
bool SolveInverseQuadraticMonotonic(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3,
                                    f32 y3, f32 &a, f32 &b, f32 &c);

// solves for "a, b, c" where "1/(a x^2 + b x + c ) = y", return true if
// solution exists
bool SolveInverseReciprocalQuadratic(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3,
                                     f32 y3, f32 &a, f32 &b, f32 &c);

// rotate a vector around the Z axis (YAW)
void VectorYawRotate(const Vector &in, f32 flYaw, Vector &out);

// Bias takes an X value between 0 and 1 and returns another value between 0 and
// 1 The curve is biased towards 0 or 1 based on biasAmt, which is between 0
// and 1. Lower values of biasAmt bias the curve towards 0 and higher values
// bias it towards 1.
//
// For example, with biasAmt = 0.2, the curve looks like this:
//
// 1
// | 	  *
// | 	  *
// |      *
// |    **
// |  **
// |	  	 ****
// |*********
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// | 	**************
// |  **
// | *
// | *
// |*
// |*
// |*
// |___________________
// 0                   1
//
// With a biasAmt of 0.5, Bias returns X.
f32 Bias(f32 x, f32 biasAmt);

// Gain is similar to Bias, but biasAmt biases towards or away from 0.5.
// Lower bias values bias towards 0.5 and higher bias values bias away from it.
//
// For example, with biasAmt = 0.2, the curve looks like this:
//
// 1
// |  	  *
// |  	 *
// |  	**
// |  ***************
// | **
// | *
// |*
// |___________________
// 0                   1
//
//
// With biasAmt = 0.8, the curve looks like this:
//
// 1
// |  		    *****
// |  		 ***
// |  		*
// | 		*
// | 		*
// |   	 ***
// |*****
// |___________________
// 0                   1
f32 Gain(f32 x, f32 biasAmt);

// SmoothCurve maps a 0-1 value into another 0-1 value based on a cosine wave
// where the derivatives of the function at 0 and 1 (and 0.5) are 0. This is
// useful for any fadein/fadeout effect where it should start and end smoothly.
//
// The curve looks like this:
//
// 1
// |  		**
// | 	   *  *
// | 	  *	   *
// | 	  *	   *
// | 	 *		*
// |   **		 **
// |***    ***
// |___________________
// 0                   1
//
f32 SmoothCurve(f32 x);

// This works like SmoothCurve, with two changes:
//
// 1. Instead of the curve peaking at 0.5, it will peak at flPeakPos.
//    (So if you specify flPeakPos=0.2, then the peak will slide to the left).
//
// 2. flPeakSharpness is a 0-1 value controlling the sharpness of the peak.
//    Low values blunt the peak and high values sharpen the peak.
f32 SmoothCurve_Tweak(f32 x, f32 flPeakPos = 0.5, f32 flPeakSharpness = 0.5);

// f32 ExponentialDecay( f32 halflife, f32 dt );
// f32 ExponentialDecay( f32 decayTo, f32 decayTime, f32 dt );

// halflife is time for value to reach 50%
inline f32 ExponentialDecay(f32 halflife, f32 dt) {
  // log(0.5) == -0.69314718055994530941723212145818
  return expf(-0.69314718f / halflife * dt);
}

// decayTo is factor the value should decay to in decayTime
inline f32 ExponentialDecay(f32 decayTo, f32 decayTime, f32 dt) {
  return expf(logf(decayTo) / decayTime * dt);
}

// Get the integrated distanced traveled
// decayTo is factor the value should decay to in decayTime
// dt is the time relative to the last velocity update
inline f32 ExponentialDecayIntegral(f32 decayTo, f32 decayTime, f32 dt) {
  return (powf(decayTo, dt / decayTime) * decayTime - decayTime) /
         logf(decayTo);
}

// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
inline f32 SimpleSpline(f32 value) {
  f32 valueSquared = value * value;

  // Nice little ease-in, ease-out spline-like curve
  return (3 * valueSquared - 2 * valueSquared * value);
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline f32 SimpleSplineRemapVal(f32 val, f32 A, f32 B, f32 C, f32 D) {
  if (A == B) return val >= B ? D : C;
  f32 cVal = (val - A) / (B - A);
  return C + (D - C) * SimpleSpline(cVal);
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline f32 SimpleSplineRemapValClamped(f32 val, f32 A, f32 B, f32 C, f32 D) {
  if (A == B) return val >= B ? D : C;
  f32 cVal = (val - A) / (B - A);
  cVal = std::clamp(cVal, 0.0f, 1.0f);
  return C + (D - C) * SimpleSpline(cVal);
}

FORCEINLINE int RoundFloatToInt(f32 f) {
#if defined(__i386__) || defined(_M_IX86) || defined(_WIN64)
  return _mm_cvtss_si32(_mm_load_ss(&f));
#elif _LINUX
  __asm __volatile__("fistpl %0;" : "=m"(nResult) : "t"(f) : "st");
#endif
}

FORCEINLINE u8 RoundFloatToByte(f32 f) {
  int nResult = RoundFloatToInt(f);
#ifdef Assert
  Assert((nResult & ~0xFF) == 0);
#endif
  return (u8)nResult;
}

FORCEINLINE unsigned long RoundFloatToUnsignedLong(f32 f) {
#if defined(_WIN64)
  uint32_t nRet = (uint32_t)f;
  if (nRet & 1) {
    if ((f - floor(f) >= 0.5)) {
      nRet++;
    }
  } else {
    if ((f - floor(f) > 0.5)) {
      nRet++;
    }
  }
  return nRet;
#else
  u8 nResult[8];

#if defined(_WIN32)
  __asm
  {
      fld f
      fistp       qword ptr nResult
  }
#elif POSIX
  __asm __volatile__("fistpl %0;" : "=m"(nResult) : "t"(f) : "st");
#endif

  return *((unsigned long *)nResult);
#endif
}

// Fast, accurate ftol:
FORCEINLINE int Float2Int(f32 a) {
  // Rely on compiler to generate CVTTSS2SI on x86
  return static_cast<int>(a);
}

// Over 15x faster than: (int)floor(value)
inline int Floor2Int(f32 a) {
  int RetVal;
#if defined(__i386__)
  // Convert to int and back, compare, subtract one if too big
  __m128 a128 = _mm_set_ss(a);
  RetVal = _mm_cvtss_si32(a128);
  __m128 rounded128 = _mm_cvt_si2ss(_mm_setzero_ps(), RetVal);
  RetVal -= _mm_comigt_ss(rounded128, a128);
#else
  RetVal = static_cast<int>(floor(a));
#endif

  return RetVal;
}

// Fast color conversion from f32 to u8

FORCEINLINE u8 FastFToC(f32 c) {
  // ieee trick
  volatile f32 dc = c * 255.0f + (f32)(1 << 23);
  // return the lsb
  return *(u8 *)&dc;
}

// Purpose: Bound input f32 to .001 (millisecond) boundary
// Input  : in -
// Output : inline f32

inline f32 ClampToMsec(f32 in) {
  int msec = Floor2Int(in * 1000.0f + 0.5f);
  return msec / 1000.0f;
}

// Over 15x faster than: (int)ceil(value)
inline int Ceil2Int(f32 a) {
  int RetVal;
#if defined(__i386__)
  // Convert to int and back, compare, add one if too small
  __m128 a128 = _mm_load_ss(&a);
  RetVal = _mm_cvtss_si32(a128);
  __m128 rounded128 = _mm_cvt_si2ss(_mm_setzero_ps(), RetVal);
  RetVal += _mm_comilt_ss(rounded128, a128);
#else
  RetVal = static_cast<int>(ceil(a));
#endif

  return RetVal;
}

// Regular signed area of triangle
#define TriArea2D(A, B, C) \
  (0.5f * ((B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x)))

// This version doesn't premultiply by 0.5f, so it's the area of the rectangle
// instead
#define TriArea2DTimesTwo(A, B, C) \
  (((B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x)))

// Get the barycentric coordinates of "pt" in triangle [A,B,C].
inline void GetBarycentricCoords2D(Vector2D const &A, Vector2D const &B,
                                   Vector2D const &C, Vector2D const &pt,
                                   f32 bcCoords[3]) {
  // Note, because to top and bottom are both x2, the issue washes out in the
  // composite
  f32 invTriArea = 1.0f / TriArea2DTimesTwo(A, B, C);

  // NOTE: We assume here that the lightmap coordinate vertices go
  // counterclockwise. If not, TriArea2D() is negated so this works out right.
  bcCoords[0] = TriArea2DTimesTwo(B, C, pt) * invTriArea;
  bcCoords[1] = TriArea2DTimesTwo(C, A, pt) * invTriArea;
  bcCoords[2] = TriArea2DTimesTwo(A, B, pt) * invTriArea;
}

// Return true of the sphere might touch the box (the sphere is actually
// treated like a box itself, so this may return true if the sphere's bounding
// box touches a corner of the box but the sphere itself doesn't).
inline bool QuickBoxSphereTest(const Vector &vOrigin, f32 flRadius,
                               const Vector &bbMin, const Vector &bbMax) {
  return vOrigin.x - flRadius < bbMax.x && vOrigin.x + flRadius > bbMin.x &&
         vOrigin.y - flRadius < bbMax.y && vOrigin.y + flRadius > bbMin.y &&
         vOrigin.z - flRadius < bbMax.z && vOrigin.z + flRadius > bbMin.z;
}

// Return true of the boxes intersect (but not if they just touch).
inline bool QuickBoxIntersectTest(const Vector &vBox1Min,
                                  const Vector &vBox1Max,
                                  const Vector &vBox2Min,
                                  const Vector &vBox2Max) {
  return vBox1Min.x < vBox2Max.x && vBox1Max.x > vBox2Min.x &&
         vBox1Min.y < vBox2Max.y && vBox1Max.y > vBox2Min.y &&
         vBox1Min.z < vBox2Max.z && vBox1Max.z > vBox2Min.z;
}

extern f32 GammaToLinearFullRange(f32 gamma);
extern f32 LinearToGammaFullRange(f32 linear);
extern f32 GammaToLinear(f32 gamma);
extern f32 LinearToGamma(f32 linear);

extern f32 SrgbGammaToLinear(f32 flSrgbGammaValue);
extern f32 SrgbLinearToGamma(f32 flLinearValue);
extern f32 X360GammaToLinear(f32 fl360GammaValue);
extern f32 X360LinearToGamma(f32 flLinearValue);
extern f32 SrgbGammaTo360Gamma(f32 flSrgbGammaValue);

// linear (0..4) to screen corrected vertex space (0..1?)
FORCEINLINE f32 LinearToVertexLight(f32 f) {
  extern f32 lineartovertex[4096];

  // Gotta clamp before the multiply; could overflow...
  // assume 0..4 range
  int i = RoundFloatToInt(f * 1024.f);

  // Presumably the comman case will be not to clamp, so check that first:
  if ((u32)i > 4095) {
    if (i < 0)
      i = 0;  // Compare to zero instead of 4095 to save 4 bytes in the
              // instruction stream
    else
      i = 4095;
  }

  return lineartovertex[i];
}

FORCEINLINE u8 LinearToLightmap(f32 f) {
  extern u8 lineartolightmap[4096];

  // Gotta clamp before the multiply; could overflow...
  int i = RoundFloatToInt(f * 1024.f);  // assume 0..4 range

  // Presumably the comman case will be not to clamp, so check that first:
  if ((u32)i > 4095) {
    if (i < 0)
      i = 0;  // Compare to zero instead of 4095 to save 4 bytes in the
              // instruction stream
    else
      i = 4095;
  }

  return lineartolightmap[i];
}

FORCEINLINE void ColorClamp(Vector &color) {
  f32 maxc = std::max(color.x, std::max(color.y, color.z));
  if (maxc > 1.0f) {
    f32 ooMax = 1.0f / maxc;
    color.x *= ooMax;
    color.y *= ooMax;
    color.z *= ooMax;
  }

  if (color[0] < 0.f) color[0] = 0.f;
  if (color[1] < 0.f) color[1] = 0.f;
  if (color[2] < 0.f) color[2] = 0.f;
}

inline void ColorClampTruncate(Vector &color) {
  if (color[0] > 1.0f)
    color[0] = 1.0f;
  else if (color[0] < 0.0f)
    color[0] = 0.0f;
  if (color[1] > 1.0f)
    color[1] = 1.0f;
  else if (color[1] < 0.0f)
    color[1] = 0.0f;
  if (color[2] > 1.0f)
    color[2] = 1.0f;
  else if (color[2] < 0.0f)
    color[2] = 0.0f;
}

// Interpolate a Catmull-Rom spline.
// t is a [0,1] value and interpolates a curve between p2 and p3.
void Catmull_Rom_Spline(const Vector &p1, const Vector &p2, const Vector &p3,
                        const Vector &p4, f32 t, Vector &output);

// Interpolate a Catmull-Rom spline.
// Returns the tangent of the point at t of the spline
void Catmull_Rom_Spline_Tangent(const Vector &p1, const Vector &p2,
                                const Vector &p3, const Vector &p4, f32 t,
                                Vector &output);

// area under the curve [0..t]
void Catmull_Rom_Spline_Integral(const Vector &p1, const Vector &p2,
                                 const Vector &p3, const Vector &p4, f32 t,
                                 Vector &output);

// area under the curve [0..1]
void Catmull_Rom_Spline_Integral(const Vector &p1, const Vector &p2,
                                 const Vector &p3, const Vector &p4,
                                 Vector &output);

// Interpolate a Catmull-Rom spline.
// Normalize p2->p1 and p3->p4 to be the same length as p2->p3
void Catmull_Rom_Spline_Normalize(const Vector &p1, const Vector &p2,
                                  const Vector &p3, const Vector &p4, f32 t,
                                  Vector &output);

// area under the curve [0..t]
// Normalize p2->p1 and p3->p4 to be the same length as p2->p3
void Catmull_Rom_Spline_Integral_Normalize(const Vector &p1, const Vector &p2,
                                           const Vector &p3, const Vector &p4,
                                           f32 t, Vector &output);

// Interpolate a Catmull-Rom spline.
// Normalize p2.x->p1.x and p3.x->p4.x to be the same length as p2.x->p3.x
void Catmull_Rom_Spline_NormalizeX(const Vector &p1, const Vector &p2,
                                   const Vector &p3, const Vector &p4, f32 t,
                                   Vector &output);

// area under the curve [0..t]
void Catmull_Rom_Spline_NormalizeX(const Vector &p1, const Vector &p2,
                                   const Vector &p3, const Vector &p4, f32 t,
                                   Vector &output);

// Interpolate a Hermite spline.
// t is a [0,1] value and interpolates a curve between p1 and p2 with the
// deltas d1 and d2.
void Hermite_Spline(const Vector &p1, const Vector &p2, const Vector &d1,
                    const Vector &d2, f32 t, Vector &output);

f32 Hermite_Spline(f32 p1, f32 p2, f32 d1, f32 d2, f32 t);

// t is a [0,1] value and interpolates a curve between p1 and p2 with the
// slopes p0->p1 and p1->p2
void Hermite_Spline(const Vector &p0, const Vector &p1, const Vector &p2, f32 t,
                    Vector &output);

f32 Hermite_Spline(f32 p0, f32 p1, f32 p2, f32 t);

void Hermite_SplineBasis(f32 t, f32 basis[]);

void Hermite_Spline(const Quaternion &q0, const Quaternion &q1,
                    const Quaternion &q2, f32 t, Quaternion &output);

// See http://en.wikipedia.org/wiki/Kochanek-Bartels_curves
//
// Tension:  -1 = Round -> 1 = Tight
// Bias:     -1 = Pre-shoot (bias left) -> 1 = Post-shoot (bias right)
// Continuity: -1 = Box corners -> 1 = Inverted corners
//
// If T=B=C=0 it's the same matrix as Catmull-Rom.
// If T=1 & B=C=0 it's the same as Cubic.
// If T=B=0 & C=-1 it's just linear interpolation
//
// See
// http://news.povray.org/povray.binaries.tutorials/attachment/%3CXns91B880592482seed7@povray.org%3E/Splines.bas.txt
// for example code and descriptions of various spline types...
//
void Kochanek_Bartels_Spline(f32 tension, f32 bias, f32 continuity,
                             const Vector &p1, const Vector &p2,
                             const Vector &p3, const Vector &p4, f32 t,
                             Vector &output);

void Kochanek_Bartels_Spline_NormalizeX(f32 tension, f32 bias, f32 continuity,
                                        const Vector &p1, const Vector &p2,
                                        const Vector &p3, const Vector &p4,
                                        f32 t, Vector &output);

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void Cubic_Spline(const Vector &p1, const Vector &p2, const Vector &p3,
                  const Vector &p4, f32 t, Vector &output);

void Cubic_Spline_NormalizeX(const Vector &p1, const Vector &p2,
                             const Vector &p3, const Vector &p4, f32 t,
                             Vector &output);

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void BSpline(const Vector &p1, const Vector &p2, const Vector &p3,
             const Vector &p4, f32 t, Vector &output);

void BSpline_NormalizeX(const Vector &p1, const Vector &p2, const Vector &p3,
                        const Vector &p4, f32 t, Vector &output);

// See link at Kochanek_Bartels_Spline for info on the basis matrix used
void Parabolic_Spline(const Vector &p1, const Vector &p2, const Vector &p3,
                      const Vector &p4, f32 t, Vector &output);

void Parabolic_Spline_NormalizeX(const Vector &p1, const Vector &p2,
                                 const Vector &p3, const Vector &p4, f32 t,
                                 Vector &output);

// quintic interpolating polynomial from Perlin.
// 0->0, 1->1, smooth-in between with smooth tangents
FORCEINLINE f32 QuinticInterpolatingPolynomial(f32 t) {
  // 6t^5-15t^4+10t^3
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// given a table of sorted tabulated positions, return the two indices and
// blendfactor to linear interpolate. Does a search. Can be used to find the
// blend value to interpolate between keyframes.
void GetInterpolationData(f32 const *pKnotPositions, f32 const *pKnotValues,
                          int nNumValuesinList, int nInterpolationRange,
                          f32 flPositionToInterpolateAt, bool bWrap,
                          f32 *pValueA, f32 *pValueB, f32 *pInterpolationValue);

f32 RangeCompressor(f32 flValue, f32 flMin, f32 flMax, f32 flBase);

// Get the minimum distance from vOrigin to the bounding box defined by
// [mins,maxs] using voronoi regions. 0 is returned if the origin is inside
// the box.
f32 CalcSqrDistanceToAABB(const Vector &mins, const Vector &maxs,
                          const Vector &point);
void CalcClosestPointOnAABB(const Vector &mins, const Vector &maxs,
                            const Vector &point, Vector &closestOut);
void CalcSqrDistAndClosestPointOnAABB(const Vector &mins, const Vector &maxs,
                                      const Vector &point, Vector &closestOut,
                                      f32 &distSqrOut);

inline f32 CalcDistanceToAABB(const Vector &mins, const Vector &maxs,
                              const Vector &point) {
  f32 flDistSqr = CalcSqrDistanceToAABB(mins, maxs, point);
  return sqrt(flDistSqr);
}

// Get the closest point from P to the (infinite) line through vLineA and
// vLineB and calculate the shortest distance from P to the line. If you pass
// in a value for t, it will tell you the t for (A + (B-A)t) to get the
// closest point. If the closest point lies on the segment between A and B,
// then 0 <= t
// <= 1.
void CalcClosestPointOnLine(const Vector &P, const Vector &vLineA,
                            const Vector &vLineB, Vector &vClosest, f32 *t = 0);
f32 CalcDistanceToLine(const Vector &P, const Vector &vLineA,
                       const Vector &vLineB, f32 *t = 0);
f32 CalcDistanceSqrToLine(const Vector &P, const Vector &vLineA,
                          const Vector &vLineB, f32 *t = 0);

// The same three functions as above, except now the line is closed between A
// and B.
void CalcClosestPointOnLineSegment(const Vector &P, const Vector &vLineA,
                                   const Vector &vLineB, Vector &vClosest,
                                   f32 *t = 0);
f32 CalcDistanceToLineSegment(const Vector &P, const Vector &vLineA,
                              const Vector &vLineB, f32 *t = 0);
f32 CalcDistanceSqrToLineSegment(const Vector &P, const Vector &vLineA,
                                 const Vector &vLineB, f32 *t = 0);

// A function to compute the closes line segment connnection two lines (or
// false if the lines are parallel, etc.)
bool CalcLineToLineIntersectionSegment(const Vector &p1, const Vector &p2,
                                       const Vector &p3, const Vector &p4,
                                       Vector *s1, Vector *s2, f32 *t1,
                                       f32 *t2);

// The above functions in 2D
void CalcClosestPointOnLine2D(Vector2D const &P, Vector2D const &vLineA,
                              Vector2D const &vLineB, Vector2D &vClosest,
                              f32 *t = 0);
f32 CalcDistanceToLine2D(Vector2D const &P, Vector2D const &vLineA,
                         Vector2D const &vLineB, f32 *t = 0);
f32 CalcDistanceSqrToLine2D(Vector2D const &P, Vector2D const &vLineA,
                            Vector2D const &vLineB, f32 *t = 0);
void CalcClosestPointOnLineSegment2D(Vector2D const &P, Vector2D const &vLineA,
                                     Vector2D const &vLineB, Vector2D &vClosest,
                                     f32 *t = 0);
f32 CalcDistanceToLineSegment2D(Vector2D const &P, Vector2D const &vLineA,
                                Vector2D const &vLineB, f32 *t = 0);
f32 CalcDistanceSqrToLineSegment2D(Vector2D const &P, Vector2D const &vLineA,
                                   Vector2D const &vLineB, f32 *t = 0);

// Init the mathlib
void MathLib_Init(f32 gamma = 2.2f, f32 texGamma = 2.2f, f32 brightness = 0.0f,
                  int overbright = 2.0f, bool bAllow3DNow = true,
                  bool bAllowSSE = true, bool bAllowSSE2 = true,
                  bool bAllowMMX = true);
bool MathLib_3DNowEnabled(void);
bool MathLib_MMXEnabled(void);
bool MathLib_SSEEnabled(void);
bool MathLib_SSE2Enabled(void);

f32 Approach(f32 target, f32 value, f32 speed);
f32 ApproachAngle(f32 target, f32 value, f32 speed);
f32 AngleDiff(f32 destAngle, f32 srcAngle);
f32 AngleDistance(f32 next, f32 cur);
f32 AngleNormalize(f32 angle);

// ensure that 0 <= angle <= 360
f32 AngleNormalizePositive(f32 angle);

bool AnglesAreEqual(f32 a, f32 b, f32 tolerance = 0.0f);

void RotationDeltaAxisAngle(const QAngle &srcAngles, const QAngle &destAngles,
                            Vector &deltaAxis, f32 &deltaAngle);
void RotationDelta(const QAngle &srcAngles, const QAngle &destAngles,
                   QAngle *out);

void ComputeTrianglePlane(const Vector &v1, const Vector &v2, const Vector &v3,
                          Vector &normal, f32 &intercept);
int PolyFromPlane(Vector *outVerts, const Vector &normal, f32 dist,
                  f32 fHalfScale = 9000.0f);
int ClipPolyToPlane(Vector *inVerts, int vertCount, Vector *outVerts,
                    const Vector &normal, f32 dist, f32 fOnPlaneEpsilon = 0.1f);
int ClipPolyToPlane_Precise(f64 *inVerts, int vertCount, f64 *outVerts,
                            const f64 *normal, f64 dist,
                            f64 fOnPlaneEpsilon = 0.1);

// Computes a reasonable tangent space for a triangle

void CalcTriangleTangentSpace(const Vector &p0, const Vector &p1,
                              const Vector &p2, const Vector2D &t0,
                              const Vector2D &t1, const Vector2D &t2,
                              Vector &sVect, Vector &tVect);

// Transforms a AABB into another space; which will inherently grow the box.

void TransformAABB(const matrix3x4_t &in1, const Vector &vecMinsIn,
                   const Vector &vecMaxsIn, Vector &vecMinsOut,
                   Vector &vecMaxsOut);

// Uses the inverse transform of in1

void ITransformAABB(const matrix3x4_t &in1, const Vector &vecMinsIn,
                    const Vector &vecMaxsIn, Vector &vecMinsOut,
                    Vector &vecMaxsOut);

// Rotates a AABB into another space; which will inherently grow the box.
// (same as TransformAABB, but doesn't take the translation into account)

void RotateAABB(const matrix3x4_t &in1, const Vector &vecMinsIn,
                const Vector &vecMaxsIn, Vector &vecMinsOut,
                Vector &vecMaxsOut);

// Uses the inverse transform of in1

void IRotateAABB(const matrix3x4_t &in1, const Vector &vecMinsIn,
                 const Vector &vecMaxsIn, Vector &vecMinsOut,
                 Vector &vecMaxsOut);

// Transform a plane

inline void MatrixTransformPlane(const matrix3x4_t &src,
                                 const cplane_t &inPlane, cplane_t &outPlane) {
  // What we want to do is the following:
  // 1) transform the normal into the new space.
  // 2) Determine a point on the old plane given by plane dist * plane normal
  // 3) Transform that point into the new space
  // 4) Plane dist = DotProduct( new normal, new point )

  // An optimized version, which works if the plane is orthogonal.
  // 1) Transform the normal into the new space
  // 2) Realize that transforming the old plane point into the new space
  // is given by [ d * n'x + Tx, d * n'y + Ty, d * n'z + Tz ]
  // where d = old plane dist, n' = transformed normal, Tn = translational
  // component of transform 3) Compute the new plane dist using the dot
  // product of the normal result of #2

  // For a correct result, this should be an inverse-transpose matrix
  // but that only matters if there are nonuniform scale or skew factors in
  // this matrix.
  VectorRotate(inPlane.normal, src, outPlane.normal);
  outPlane.dist = inPlane.dist * DotProduct(outPlane.normal, outPlane.normal);
  outPlane.dist += outPlane.normal.x * src[0][3] +
                   outPlane.normal.y * src[1][3] +
                   outPlane.normal.z * src[2][3];
}

inline void MatrixITransformPlane(const matrix3x4_t &src,
                                  const cplane_t &inPlane, cplane_t &outPlane) {
  // The trick here is that Tn = translational component of transform,
  // but for an inverse transform, Tn = - R^-1 * T
  Vector vecTranslation;
  MatrixGetColumn(src, 3, vecTranslation);

  Vector vecInvTranslation;
  VectorIRotate(vecTranslation, src, vecInvTranslation);

  VectorIRotate(inPlane.normal, src, outPlane.normal);
  outPlane.dist = inPlane.dist * DotProduct(outPlane.normal, outPlane.normal);
  outPlane.dist -= outPlane.normal.x * vecInvTranslation[0] +
                   outPlane.normal.y * vecInvTranslation[1] +
                   outPlane.normal.z * vecInvTranslation[2];
}

int CeilPow2(int in);
int FloorPow2(int in);

FORCEINLINE f32 *UnpackNormal_HEND3N(const u32 *pPackedNormal, f32 *pNormal) {
  int temp[3];
  temp[0] = ((*pPackedNormal >> 0L) & 0x7ff);
  if (temp[0] & 0x400) {
    temp[0] = 2048 - temp[0];
  }
  temp[1] = ((*pPackedNormal >> 11L) & 0x7ff);
  if (temp[1] & 0x400) {
    temp[1] = 2048 - temp[1];
  }
  temp[2] = ((*pPackedNormal >> 22L) & 0x3ff);
  if (temp[2] & 0x200) {
    temp[2] = 1024 - temp[2];
  }
  pNormal[0] = (f32)temp[0] * 1.0f / 1023.0f;
  pNormal[1] = (f32)temp[1] * 1.0f / 1023.0f;
  pNormal[2] = (f32)temp[2] * 1.0f / 511.0f;
  return pNormal;
}

FORCEINLINE u32 *PackNormal_HEND3N(const f32 *pNormal, u32 *pPackedNormal) {
  int temp[3];

  temp[0] = Float2Int(pNormal[0] * 1023.0f);
  temp[1] = Float2Int(pNormal[1] * 1023.0f);
  temp[2] = Float2Int(pNormal[2] * 511.0f);

  // the normal is out of bounds, determine the source and fix
  // clamping would be even more of a slowdown here
  Assert(temp[0] >= -1023 && temp[0] <= 1023);
  Assert(temp[1] >= -1023 && temp[1] <= 1023);
  Assert(temp[2] >= -511 && temp[2] <= 511);

  *pPackedNormal = ((temp[2] & 0x3ff) << 22L) | ((temp[1] & 0x7ff) << 11L) |
                   ((temp[0] & 0x7ff) << 0L);
  return pPackedNormal;
}

FORCEINLINE u32 *PackNormal_HEND3N(f32 nx, f32 ny, f32 nz, u32 *pPackedNormal) {
  int temp[3];

  temp[0] = Float2Int(nx * 1023.0f);
  temp[1] = Float2Int(ny * 1023.0f);
  temp[2] = Float2Int(nz * 511.0f);

  // the normal is out of bounds, determine the source and fix
  // clamping would be even more of a slowdown here
  Assert(temp[0] >= -1023 && temp[0] <= 1023);
  Assert(temp[1] >= -1023 && temp[1] <= 1023);
  Assert(temp[2] >= -511 && temp[2] <= 511);

  *pPackedNormal = ((temp[2] & 0x3ff) << 22L) | ((temp[1] & 0x7ff) << 11L) |
                   ((temp[0] & 0x7ff) << 0L);
  return pPackedNormal;
}

FORCEINLINE f32 *UnpackNormal_SHORT2(const u32 *pPackedNormal, f32 *pNormal,
                                     bool bIsTangent = FALSE) {
  // Unpacks from Jason's 2-i16 format (fills in a 4th binormal-sign (+1/-1)
  // value, if this is a tangent vector)

  // FIXME: i16 math is slow on 360 - use ints here instead (bit-twiddle to
  // deal w/ the sign bits)
  i16 iX = (*pPackedNormal & 0x0000FFFF);
  i16 iY = (*pPackedNormal & 0xFFFF0000) >> 16;

  f32 zSign = +1;
  if (iX < 0) {
    zSign = -1;
    iX = -iX;
  }
  f32 tSign = +1;
  if (iY < 0) {
    tSign = -1;
    iY = -iY;
  }

  pNormal[0] = (iX - 16384.0f) / 16384.0f;
  pNormal[1] = (iY - 16384.0f) / 16384.0f;
  pNormal[2] =
      zSign * sqrtf(1.0f - (pNormal[0] * pNormal[0] + pNormal[1] * pNormal[1]));
  if (bIsTangent) {
    pNormal[3] = tSign;
  }

  return pNormal;
}

FORCEINLINE u32 *PackNormal_SHORT2(f32 nx, f32 ny, f32 nz, u32 *pPackedNormal,
                                   f32 binormalSign = +1.0f) {
  // Pack a vector (ASSUMED TO BE NORMALIZED) into Jason's 4-u8 (SHORT2)
  // format. This simply reconstructs Z from X & Y. It uses the sign bits of
  // the X & Y coords to reconstruct the sign of Z and, if this is a tangent
  // vector, the sign of the binormal (this is needed because tangent/binormal
  // vectors are supposed to follow UV gradients, but shaders reconstruct the
  // binormal from the tangent and normal assuming that they form a
  // right-handed basis).

  nx += 1;  // [-1,+1] -> [0,2]
  ny += 1;
  nx *= 16384.0f;  // [ 0, 2] -> [0,32768]
  ny *= 16384.0f;

  // '0' and '32768' values are invalid encodings
  nx = std::max(nx, 1.0f);  // Make sure there are no zero values
  ny = std::max(ny, 1.0f);
  nx = std::min(nx, 32767.0f);  // Make sure there are no 32768 values
  ny = std::min(ny, 32767.0f);

  if (nz < 0.0f) nx = -nx;  // Set the sign bit for z

  ny *= binormalSign;  // Set the sign bit for the binormal (use when encoding a
                       // tangent vector)

  // FIXME: i16 math is slow on 360 - use ints here instead (bit-twiddle to
  // deal w/ the sign bits), also use Float2Int()
  i16 sX = (i16)nx;  // signed i16 [1,32767]
  i16 sY = (i16)ny;

  *pPackedNormal =
      (sX & 0x0000FFFF) | (sY << 16);  // NOTE: The mask is necessary (if sX is
                                       // negative and cast to an int...)

  return pPackedNormal;
}

FORCEINLINE u32 *PackNormal_SHORT2(const f32 *pNormal, u32 *pPackedNormal,
                                   f32 binormalSign = +1.0f) {
  return PackNormal_SHORT2(pNormal[0], pNormal[1], pNormal[2], pPackedNormal,
                           binormalSign);
}

// Unpacks a UBYTE4 normal (for a tangent, the result's fourth component
// receives the binormal 'sign')
FORCEINLINE f32 *UnpackNormal_UBYTE4(const u32 *pPackedNormal, f32 *pNormal,
                                     bool bIsTangent = FALSE) {
  u8 cX, cY;
  if (bIsTangent) {
    cX = *pPackedNormal >> 16;  // Unpack Z
    cY = *pPackedNormal >> 24;  // Unpack W
  } else {
    cX = *pPackedNormal >> 0;  // Unpack X
    cY = *pPackedNormal >> 8;  // Unpack Y
  }

  f32 x = cX - 128.0f;
  f32 y = cY - 128.0f;
  f32 z;

  f32 zSignBit =
      x < 0 ? 1.0f : 0.0f;  // z and t negative bits (like slt asm instruction)
  f32 tSignBit = y < 0 ? 1.0f : 0.0f;
  f32 zSign = -(2 * zSignBit - 1);  // z and t signs
  f32 tSign = -(2 * tSignBit - 1);

  x = x * zSign - zSignBit;  // 0..127
  y = y * tSign - tSignBit;
  x = x - 64;  // -64..63
  y = y - 64;

  f32 xSignBit =
      x < 0 ? 1.0f : 0.0f;  // x and y negative bits (like slt asm instruction)
  f32 ySignBit = y < 0 ? 1.0f : 0.0f;
  f32 xSign = -(2 * xSignBit - 1);  // x and y signs
  f32 ySign = -(2 * ySignBit - 1);

  x = (x * xSign - xSignBit) / 63.0f;  // 0..1 range
  y = (y * ySign - ySignBit) / 63.0f;
  z = 1.0f - x - y;

  f32 oolen = 1.0f / sqrt(x * x + y * y + z * z);  // Normalize and
  x *= oolen * xSign;                              // Recover signs
  y *= oolen * ySign;
  z *= oolen * zSign;

  pNormal[0] = x;
  pNormal[1] = y;
  pNormal[2] = z;
  if (bIsTangent) {
    pNormal[3] = tSign;
  }

  return pNormal;
}

//////////////////////////////////////////////////////////////////////////////
// See:
// http://www.oroboro.com/rafael/docserv.php/index/programming/article/unitv2
//
// UBYTE4 encoding, using per-octant projection onto x+y+z=1
// Assume input vector is already unit length
//
// binormalSign specifies 'sign' of binormal, stored in t sign bit of tangent
// (lets the shader know whether norm/tan/bin form a right-handed basis)
//
// bIsTangent is used to specify which WORD of the output to store the data
// The expected usage is to call once with the normal and once with
// the tangent and binormal sign flag, bitwise OR'ing the returned DWORDs
FORCEINLINE u32 *PackNormal_UBYTE4(f32 nx, f32 ny, f32 nz, u32 *pPackedNormal,
                                   bool bIsTangent = false,
                                   f32 binormalSign = +1.0f) {
  f32 xSign = nx < 0.0f ? -1.0f : 1.0f;  // -1 or 1 sign
  f32 ySign = ny < 0.0f ? -1.0f : 1.0f;
  f32 zSign = nz < 0.0f ? -1.0f : 1.0f;
  f32 tSign = binormalSign;
  Assert((binormalSign == +1.0f) || (binormalSign == -1.0f));

  f32 xSignBit = 0.5f * (1 - xSign);  // [-1,+1] -> [1,0]
  f32 ySignBit =
      0.5f * (1 - ySign);  // 1 is negative bit (like slt instruction)
  f32 zSignBit = 0.5f * (1 - zSign);
  f32 tSignBit = 0.5f * (1 - binormalSign);

  f32 absX = xSign * nx;  // 0..1 range (abs)
  f32 absY = ySign * ny;
  f32 absZ = zSign * nz;

  f32 xbits = absX / (absX + absY + absZ);  // Project onto x+y+z=1 plane
  f32 ybits = absY / (absX + absY + absZ);

  xbits *= 63;  // 0..63
  ybits *= 63;

  xbits = xbits * xSign - xSignBit;  // -64..63 range
  ybits = ybits * ySign - ySignBit;
  xbits += 64.0f;  // 0..127 range
  ybits += 64.0f;

  xbits = xbits * zSign - zSignBit;  // Negate based on z and t
  ybits = ybits * tSign - tSignBit;  // -128..127 range

  xbits += 128.0f;  // 0..255 range
  ybits += 128.0f;

  u8 cX = (u8)xbits;
  u8 cY = (u8)ybits;

  if (!bIsTangent)
    *pPackedNormal = (cX << 0) | (cY << 8);  // xy for normal
  else
    *pPackedNormal = (cX << 16) | (cY << 24);  // zw for tangent

  return pPackedNormal;
}

FORCEINLINE u32 *PackNormal_UBYTE4(const f32 *pNormal, u32 *pPackedNormal,
                                   bool bIsTangent = false,
                                   f32 binormalSign = +1.0f) {
  return PackNormal_UBYTE4(pNormal[0], pNormal[1], pNormal[2], pPackedNormal,
                           bIsTangent, binormalSign);
}

// Convert RGB to HSV

void RGBtoHSV(const Vector &rgb, Vector &hsv);

// Convert HSV to RGB

void HSVtoRGB(const Vector &hsv, Vector &rgb);

#endif  // SOURCE_MATHLIB_MATHLIB_H_
