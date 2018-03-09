// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines SIMD "structure of arrays" classes and functions.

#ifndef SOURCE_MATHLIB_SSEMATH_H_
#define SOURCE_MATHLIB_SSEMATH_H_

#include "build/include/build_config.h"

#if defined(COMPILER_MSVC) && defined(ARCH_CPU_X86)
#include <xmmintrin.h>  // SSE* intrinsics.
#endif

#include "base/include/base_types.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"

#if defined(OS_POSIX)
#define USE_STDC_FOR_SIMD 0
#else
#define USE_STDC_FOR_SIMD 0
#endif

// I thought about defining a class/union for the SIMD packed floats instead of
// using fltx4, but decided against it because (a) the nature of SIMD code which
// includes comparisons is to blur the relationship between packed floats and
// packed integer types and (b) not sure that the compiler would handle
// generating good code for the intrinsics.

#if USE_STDC_FOR_SIMD

typedef union {
  f32 m128_f32[4];
  u32 m128_u32[4];
} fltx4;

typedef fltx4 i32x4;
typedef fltx4 u32x4;

#elif (defined(_X360))

typedef union {
  // This union allows f32/int access (which generally shouldn't be done in
  // inner loops)
  __vector4 vmx;
  f32 m128_f32[4];
  u32 m128_u32[4];
} fltx4_union;

typedef __vector4 fltx4;
typedef __vector4 i32x4;  // a VMX register; just a way of making it explicit
                          // that we're doing integer ops.
typedef __vector4 u32x4;  // a VMX register; just a way of making it explicit
                          // that we're doing u32 integer ops.

#else

typedef __m128 fltx4;
typedef __m128 i32x4;
typedef __m128 u32x4;

#endif

// The FLTX4 type is a fltx4 used as a parameter to a function.
// On the 360, the best way to do this is pass-by-copy on the registers.
// On the PC, the best way is to pass by const reference.
// The compiler will sometimes, but not always, replace a pass-by-const-ref
// with a pass-in-reg on the 360; to avoid this confusion, you can
// explicitly use a FLTX4 as the parameter type.
typedef const fltx4 &FLTX4;

// A 16-byte aligned i32 datastructure
// (for use when writing out fltx4's as SIGNED
// ints).
struct alignas(16) intx4 {
  i32 m_i32[4];

  inline int &operator[](int which) { return m_i32[which]; }

  inline const int &operator[](int which) const { return m_i32[which]; }

  inline i32 *Base() { return m_i32; }

  inline const i32 *Base() const { return m_i32; }

  inline const bool operator==(const intx4 &other) const {
    return m_i32[0] == other.m_i32[0] && m_i32[1] == other.m_i32[1] &&
           m_i32[2] == other.m_i32[2] && m_i32[3] == other.m_i32[3];
  }
};

SOURCE_FORCEINLINE void TestVPUFlags() {}

// useful constants in SIMD packed f32 format:
// (note: some of these aren't stored on the 360,
// but are manufactured directly in one or two
// instructions, saving a load and possible L2
// miss.)
extern const fltx4 Four_Zeros;       // 0 0 0 0
extern const fltx4 Four_Ones;        // 1 1 1 1
extern const fltx4 Four_Twos;        // 2 2 2 2
extern const fltx4 Four_Threes;      // 3 3 3 3
extern const fltx4 Four_Fours;       // guess.
extern const fltx4 Four_Point225s;   // .225 .225 .225 .225
extern const fltx4 Four_PointFives;  // .5 .5 .5 .5
extern const fltx4
    Four_Epsilons;  // FLT_EPSILON FLT_EPSILON FLT_EPSILON FLT_EPSILON
extern const fltx4 Four_2ToThe21s;  // (1<<21)..
extern const fltx4 Four_2ToThe22s;  // (1<<22)..
extern const fltx4 Four_2ToThe23s;  // (1<<23)..
extern const fltx4 Four_2ToThe24s;  // (1<<24)..
extern const fltx4 Four_Origin;   // 0 0 0 1 (origin point, like vr0 on the PS2)
extern const fltx4 Four_FLT_MAX;  // FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX
extern const fltx4
    Four_Negative_FLT_MAX;  // -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX

// external aligned integer constants
extern const alignas(16) i32 g_SIMD_clear_signmask[];     // 0x7fffffff x 4
extern const alignas(16) i32 g_SIMD_signmask[];           // 0x80000000 x 4
extern const alignas(16) i32 g_SIMD_lsbmask[];            // 0xfffffffe x 4
extern const alignas(16) i32 g_SIMD_clear_wmask[];        // -1 -1 -1 0
extern const alignas(16) i32 g_SIMD_ComponentMask[4][4];  // [0xFFFFFFFF 0 0 0],
                                                      // [0 0xFFFFFFFF 0 0],
                                                      // [0 0 0xFFFFFFFF 0],
                                                      // [0 0 0 0xFFFFFFFF]

#if USE_STDC_FOR_SIMD

//---------------------------------------------------------------------
// Standard C (fallback/Linux) implementation (only there for compat - slow)
//---------------------------------------------------------------------

SOURCE_FORCEINLINE f32 SubFloat(const fltx4 &a, int idx) { return a.m128_f32[idx]; }

SOURCE_FORCEINLINE f32 &SubFloat(fltx4 &a, int idx) { return a.m128_f32[idx]; }

SOURCE_FORCEINLINE u32 SubInt(const fltx4 &a, int idx) { return a.m128_u32[idx]; }

SOURCE_FORCEINLINE u32 &SubInt(fltx4 &a, int idx) { return a.m128_u32[idx]; }

// Return one in the fastest way -- on the x360, faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadZeroSIMD() { return Four_Zeros; }

// Return one in the fastest way -- on the x360, faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadOneSIMD() { return Four_Ones; }

SOURCE_FORCEINLINE fltx4 SplatXSIMD(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 0);
  SubFloat(retVal, 1) = SubFloat(a, 0);
  SubFloat(retVal, 2) = SubFloat(a, 0);
  SubFloat(retVal, 3) = SubFloat(a, 0);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SplatYSIMD(fltx4 a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 1);
  SubFloat(retVal, 1) = SubFloat(a, 1);
  SubFloat(retVal, 2) = SubFloat(a, 1);
  SubFloat(retVal, 3) = SubFloat(a, 1);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SplatZSIMD(fltx4 a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 2);
  SubFloat(retVal, 1) = SubFloat(a, 2);
  SubFloat(retVal, 2) = SubFloat(a, 2);
  SubFloat(retVal, 3) = SubFloat(a, 2);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SplatWSIMD(fltx4 a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 3);
  SubFloat(retVal, 1) = SubFloat(a, 3);
  SubFloat(retVal, 2) = SubFloat(a, 3);
  SubFloat(retVal, 3) = SubFloat(a, 3);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SetXSIMD(const fltx4 &a, const fltx4 &x) {
  fltx4 result = a;
  SubFloat(result, 0) = SubFloat(x, 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetYSIMD(const fltx4 &a, const fltx4 &y) {
  fltx4 result = a;
  SubFloat(result, 1) = SubFloat(y, 1);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetZSIMD(const fltx4 &a, const fltx4 &z) {
  fltx4 result = a;
  SubFloat(result, 2) = SubFloat(z, 2);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetWSIMD(const fltx4 &a, const fltx4 &w) {
  fltx4 result = a;
  SubFloat(result, 3) = SubFloat(w, 3);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetComponentSIMD(const fltx4 &a, int nComponent,
                                   f32 flValue) {
  fltx4 result = a;
  SubFloat(result, nComponent) = flValue;
  return result;
}

// a b c d -> b c d a
SOURCE_FORCEINLINE fltx4 RotateLeft(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 1);
  SubFloat(retVal, 1) = SubFloat(a, 2);
  SubFloat(retVal, 2) = SubFloat(a, 3);
  SubFloat(retVal, 3) = SubFloat(a, 0);
  return retVal;
}

// a b c d -> c d a b
SOURCE_FORCEINLINE fltx4 RotateLeft2(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = SubFloat(a, 2);
  SubFloat(retVal, 1) = SubFloat(a, 3);
  SubFloat(retVal, 2) = SubFloat(a, 0);
  SubFloat(retVal, 3) = SubFloat(a, 1);
  return retVal;
}

#define BINOP(op)                                           \
  fltx4 retVal;                                             \
  SubFloat(retVal, 0) = (SubFloat(a, 0) op SubFloat(b, 0)); \
  SubFloat(retVal, 1) = (SubFloat(a, 1) op SubFloat(b, 1)); \
  SubFloat(retVal, 2) = (SubFloat(a, 2) op SubFloat(b, 2)); \
  SubFloat(retVal, 3) = (SubFloat(a, 3) op SubFloat(b, 3)); \
  return retVal;

#define IBINOP(op)                                    \
  fltx4 retVal;                                       \
  SubInt(retVal, 0) = (SubInt(a, 0) op SubInt(b, 0)); \
  SubInt(retVal, 1) = (SubInt(a, 1) op SubInt(b, 1)); \
  SubInt(retVal, 2) = (SubInt(a, 2) op SubInt(b, 2)); \
  SubInt(retVal, 3) = (SubInt(a, 3) op SubInt(b, 3)); \
  return retVal;

SOURCE_FORCEINLINE fltx4 AddSIMD(const fltx4 &a, const fltx4 &b) { BINOP(+); }

SOURCE_FORCEINLINE fltx4 SubSIMD(const fltx4 &a, const fltx4 &b)  // a-b
{
  BINOP(-);
};

SOURCE_FORCEINLINE fltx4 MulSIMD(const fltx4 &a, const fltx4 &b)  // a*b
{
  BINOP(*);
}

SOURCE_FORCEINLINE fltx4 DivSIMD(const fltx4 &a, const fltx4 &b)  // a/b
{
  BINOP(/);
}

SOURCE_FORCEINLINE fltx4 MaddSIMD(const fltx4 &a, const fltx4 &b,
                           const fltx4 &c)  // a*b + c
{
  return AddSIMD(MulSIMD(a, b), c);
}

SOURCE_FORCEINLINE fltx4 SinSIMD(const fltx4 &radians) {
  fltx4 result;
  SubFloat(result, 0) = sin(SubFloat(radians, 0));
  SubFloat(result, 1) = sin(SubFloat(radians, 1));
  SubFloat(result, 2) = sin(SubFloat(radians, 2));
  SubFloat(result, 3) = sin(SubFloat(radians, 3));
  return result;
}

SOURCE_FORCEINLINE void SinCos3SIMD(fltx4 &sine, fltx4 &cosine, const fltx4 &radians) {
  SinCos(SubFloat(radians, 0), &SubFloat(sine, 0), &SubFloat(cosine, 0));
  SinCos(SubFloat(radians, 1), &SubFloat(sine, 1), &SubFloat(cosine, 1));
  SinCos(SubFloat(radians, 2), &SubFloat(sine, 2), &SubFloat(cosine, 2));
}

SOURCE_FORCEINLINE void SinCosSIMD(fltx4 &sine, fltx4 &cosine, const fltx4 &radians) {
  SinCos(SubFloat(radians, 0), &SubFloat(sine, 0), &SubFloat(cosine, 0));
  SinCos(SubFloat(radians, 1), &SubFloat(sine, 1), &SubFloat(cosine, 1));
  SinCos(SubFloat(radians, 2), &SubFloat(sine, 2), &SubFloat(cosine, 2));
  SinCos(SubFloat(radians, 3), &SubFloat(sine, 3), &SubFloat(cosine, 3));
}

SOURCE_FORCEINLINE fltx4 ArcSinSIMD(const fltx4 &sine) {
  fltx4 result;
  SubFloat(result, 0) = asin(SubFloat(sine, 0));
  SubFloat(result, 1) = asin(SubFloat(sine, 1));
  SubFloat(result, 2) = asin(SubFloat(sine, 2));
  SubFloat(result, 3) = asin(SubFloat(sine, 3));
  return result;
}

SOURCE_FORCEINLINE fltx4 MaxSIMD(const fltx4 &a, const fltx4 &b)  // std::max(a,b)
{
  fltx4 retVal;
  SubFloat(retVal, 0) = std::max(SubFloat(a, 0), SubFloat(b, 0));
  SubFloat(retVal, 1) = std::max(SubFloat(a, 1), SubFloat(b, 1));
  SubFloat(retVal, 2) = std::max(SubFloat(a, 2), SubFloat(b, 2));
  SubFloat(retVal, 3) = std::max(SubFloat(a, 3), SubFloat(b, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 MinSIMD(const fltx4 &a, const fltx4 &b)  // std::min(a,b)
{
  fltx4 retVal;
  SubFloat(retVal, 0) = std::min(SubFloat(a, 0), SubFloat(b, 0));
  SubFloat(retVal, 1) = std::min(SubFloat(a, 1), SubFloat(b, 1));
  SubFloat(retVal, 2) = std::min(SubFloat(a, 2), SubFloat(b, 2));
  SubFloat(retVal, 3) = std::min(SubFloat(a, 3), SubFloat(b, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 AndSIMD(const fltx4 &a, const fltx4 &b)  // a & b
{
  IBINOP(&);
}

SOURCE_FORCEINLINE fltx4 AndNotSIMD(const fltx4 &a, const fltx4 &b)  // ~a & b
{
  fltx4 retVal;
  SubInt(retVal, 0) = ~SubInt(a, 0) & SubInt(b, 0);
  SubInt(retVal, 1) = ~SubInt(a, 1) & SubInt(b, 1);
  SubInt(retVal, 2) = ~SubInt(a, 2) & SubInt(b, 2);
  SubInt(retVal, 3) = ~SubInt(a, 3) & SubInt(b, 3);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 XorSIMD(const fltx4 &a, const fltx4 &b)  // a ^ b
{
  IBINOP (^);
}

SOURCE_FORCEINLINE fltx4 OrSIMD(const fltx4 &a, const fltx4 &b)  // a | b
{
  IBINOP(|);
}

SOURCE_FORCEINLINE fltx4 NegSIMD(const fltx4 &a)  // negate: -a
{
  fltx4 retval;
  SubFloat(retval, 0) = -SubFloat(a, 0);
  SubFloat(retval, 1) = -SubFloat(a, 1);
  SubFloat(retval, 2) = -SubFloat(a, 2);
  SubFloat(retval, 3) = -SubFloat(a, 3);

  return retval;
}

SOURCE_FORCEINLINE bool IsAllZeros(const fltx4 &a)  // all floats of a zero?
{
  return (SubFloat(a, 0) == 0.0) && (SubFloat(a, 1) == 0.0) &&
         (SubFloat(a, 2) == 0.0) && (SubFloat(a, 3) == 0.0);
}

// for branching when a.xyzw > b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThan(const fltx4 &a, const fltx4 &b) {
  return SubFloat(a, 0) > SubFloat(b, 0) && SubFloat(a, 1) > SubFloat(b, 1) &&
         SubFloat(a, 2) > SubFloat(b, 2) && SubFloat(a, 3) > SubFloat(b, 3);
}

// for branching when a.xyzw >= b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThanOrEq(const fltx4 &a, const fltx4 &b) {
  return SubFloat(a, 0) >= SubFloat(b, 0) && SubFloat(a, 1) >= SubFloat(b, 1) &&
         SubFloat(a, 2) >= SubFloat(b, 2) && SubFloat(a, 3) >= SubFloat(b, 3);
}

// For branching if all a.xyzw == b.xyzw
SOURCE_FORCEINLINE bool IsAllEqual(const fltx4 &a, const fltx4 &b) {
  return SubFloat(a, 0) == SubFloat(b, 0) && SubFloat(a, 1) == SubFloat(b, 1) &&
         SubFloat(a, 2) == SubFloat(b, 2) && SubFloat(a, 3) == SubFloat(b, 3);
}

SOURCE_FORCEINLINE int TestSignSIMD(
    const fltx4 &a)  // mask of which floats have the high bit set
{
  int nRet = 0;

  nRet |= (SubInt(a, 0) & 0x80000000) >> 31;  // sign(x) -> bit 0
  nRet |= (SubInt(a, 1) & 0x80000000) >> 30;  // sign(y) -> bit 1
  nRet |= (SubInt(a, 2) & 0x80000000) >> 29;  // sign(z) -> bit 2
  nRet |= (SubInt(a, 3) & 0x80000000) >> 28;  // sign(w) -> bit 3

  return nRet;
}

SOURCE_FORCEINLINE bool IsAnyNegative(
    const fltx4 &a)  // (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
  return (0 != TestSignSIMD(a));
}

SOURCE_FORCEINLINE fltx4 CmpEqSIMD(const fltx4 &a, const fltx4 &b)  // (a==b) ? ~0:0
{
  fltx4 retVal;
  SubInt(retVal, 0) = (SubFloat(a, 0) == SubFloat(b, 0)) ? ~0 : 0;
  SubInt(retVal, 1) = (SubFloat(a, 1) == SubFloat(b, 1)) ? ~0 : 0;
  SubInt(retVal, 2) = (SubFloat(a, 2) == SubFloat(b, 2)) ? ~0 : 0;
  SubInt(retVal, 3) = (SubFloat(a, 3) == SubFloat(b, 3)) ? ~0 : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4 CmpGtSIMD(const fltx4 &a, const fltx4 &b)  // (a>b) ? ~0:0
{
  fltx4 retVal;
  SubInt(retVal, 0) = (SubFloat(a, 0) > SubFloat(b, 0)) ? ~0 : 0;
  SubInt(retVal, 1) = (SubFloat(a, 1) > SubFloat(b, 1)) ? ~0 : 0;
  SubInt(retVal, 2) = (SubFloat(a, 2) > SubFloat(b, 2)) ? ~0 : 0;
  SubInt(retVal, 3) = (SubFloat(a, 3) > SubFloat(b, 3)) ? ~0 : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4 CmpGeSIMD(const fltx4 &a, const fltx4 &b)  // (a>=b) ? ~0:0
{
  fltx4 retVal;
  SubInt(retVal, 0) = (SubFloat(a, 0) >= SubFloat(b, 0)) ? ~0 : 0;
  SubInt(retVal, 1) = (SubFloat(a, 1) >= SubFloat(b, 1)) ? ~0 : 0;
  SubInt(retVal, 2) = (SubFloat(a, 2) >= SubFloat(b, 2)) ? ~0 : 0;
  SubInt(retVal, 3) = (SubFloat(a, 3) >= SubFloat(b, 3)) ? ~0 : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4 CmpLtSIMD(const fltx4 &a, const fltx4 &b)  // (a<b) ? ~0:0
{
  fltx4 retVal;
  SubInt(retVal, 0) = (SubFloat(a, 0) < SubFloat(b, 0)) ? ~0 : 0;
  SubInt(retVal, 1) = (SubFloat(a, 1) < SubFloat(b, 1)) ? ~0 : 0;
  SubInt(retVal, 2) = (SubFloat(a, 2) < SubFloat(b, 2)) ? ~0 : 0;
  SubInt(retVal, 3) = (SubFloat(a, 3) < SubFloat(b, 3)) ? ~0 : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4 CmpLeSIMD(const fltx4 &a, const fltx4 &b)  // (a<=b) ? ~0:0
{
  fltx4 retVal;
  SubInt(retVal, 0) = (SubFloat(a, 0) <= SubFloat(b, 0)) ? ~0 : 0;
  SubInt(retVal, 1) = (SubFloat(a, 1) <= SubFloat(b, 1)) ? ~0 : 0;
  SubInt(retVal, 2) = (SubFloat(a, 2) <= SubFloat(b, 2)) ? ~0 : 0;
  SubInt(retVal, 3) = (SubFloat(a, 3) <= SubFloat(b, 3)) ? ~0 : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4
CmpInBoundsSIMD(const fltx4 &a, const fltx4 &b)  // (a <= b && a >= -b) ? ~0 : 0
{
  fltx4 retVal;
  SubInt(retVal, 0) =
      (SubFloat(a, 0) <= SubFloat(b, 0) && SubFloat(a, 0) >= -SubFloat(b, 0))
          ? ~0
          : 0;
  SubInt(retVal, 1) =
      (SubFloat(a, 1) <= SubFloat(b, 1) && SubFloat(a, 1) >= -SubFloat(b, 1))
          ? ~0
          : 0;
  SubInt(retVal, 2) =
      (SubFloat(a, 2) <= SubFloat(b, 2) && SubFloat(a, 2) >= -SubFloat(b, 2))
          ? ~0
          : 0;
  SubInt(retVal, 3) =
      (SubFloat(a, 3) <= SubFloat(b, 3) && SubFloat(a, 3) >= -SubFloat(b, 3))
          ? ~0
          : 0;
  return retVal;
}

SOURCE_FORCEINLINE fltx4 MaskedAssign(const fltx4 &ReplacementMask,
                               const fltx4 &NewValue, const fltx4 &OldValue) {
  return OrSIMD(AndSIMD(ReplacementMask, NewValue),
                AndNotSIMD(ReplacementMask, OldValue));
}

SOURCE_FORCEINLINE fltx4 ReplicateX4(f32 flValue)  //  a,a,a,a
{
  fltx4 retVal;
  SubFloat(retVal, 0) = flValue;
  SubFloat(retVal, 1) = flValue;
  SubFloat(retVal, 2) = flValue;
  SubFloat(retVal, 3) = flValue;
  return retVal;
}

/// replicate a single 32 bit integer value to all 4 components of an m128
SOURCE_FORCEINLINE fltx4 ReplicateIX4(int nValue) {
  fltx4 retVal;
  SubInt(retVal, 0) = nValue;
  SubInt(retVal, 1) = nValue;
  SubInt(retVal, 2) = nValue;
  SubInt(retVal, 3) = nValue;
  return retVal;
}

// Round towards positive infinity
SOURCE_FORCEINLINE fltx4 CeilSIMD(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = ceil(SubFloat(a, 0));
  SubFloat(retVal, 1) = ceil(SubFloat(a, 1));
  SubFloat(retVal, 2) = ceil(SubFloat(a, 2));
  SubFloat(retVal, 3) = ceil(SubFloat(a, 3));
  return retVal;
}

// Round towards negative infinity
SOURCE_FORCEINLINE fltx4 FloorSIMD(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = floor(SubFloat(a, 0));
  SubFloat(retVal, 1) = floor(SubFloat(a, 1));
  SubFloat(retVal, 2) = floor(SubFloat(a, 2));
  SubFloat(retVal, 3) = floor(SubFloat(a, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SqrtEstSIMD(const fltx4 &a)  // sqrt(a), more or less
{
  fltx4 retVal;
  SubFloat(retVal, 0) = sqrt(SubFloat(a, 0));
  SubFloat(retVal, 1) = sqrt(SubFloat(a, 1));
  SubFloat(retVal, 2) = sqrt(SubFloat(a, 2));
  SubFloat(retVal, 3) = sqrt(SubFloat(a, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 SqrtSIMD(const fltx4 &a)  // sqrt(a)
{
  fltx4 retVal;
  SubFloat(retVal, 0) = sqrt(SubFloat(a, 0));
  SubFloat(retVal, 1) = sqrt(SubFloat(a, 1));
  SubFloat(retVal, 2) = sqrt(SubFloat(a, 2));
  SubFloat(retVal, 3) = sqrt(SubFloat(a, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4
ReciprocalSqrtEstSIMD(const fltx4 &a)  // 1/sqrt(a), more or less
{
  fltx4 retVal;
  SubFloat(retVal, 0) = 1.0 / sqrt(SubFloat(a, 0));
  SubFloat(retVal, 1) = 1.0 / sqrt(SubFloat(a, 1));
  SubFloat(retVal, 2) = 1.0 / sqrt(SubFloat(a, 2));
  SubFloat(retVal, 3) = 1.0 / sqrt(SubFloat(a, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 ReciprocalSqrtSIMD(const fltx4 &a)  // 1/sqrt(a)
{
  fltx4 retVal;
  SubFloat(retVal, 0) = 1.0 / sqrt(SubFloat(a, 0));
  SubFloat(retVal, 1) = 1.0 / sqrt(SubFloat(a, 1));
  SubFloat(retVal, 2) = 1.0 / sqrt(SubFloat(a, 2));
  SubFloat(retVal, 3) = 1.0 / sqrt(SubFloat(a, 3));
  return retVal;
}

SOURCE_FORCEINLINE fltx4 ReciprocalEstSIMD(const fltx4 &a)  // 1/a, more or less
{
  fltx4 retVal;
  SubFloat(retVal, 0) = 1.0 / SubFloat(a, 0);
  SubFloat(retVal, 1) = 1.0 / SubFloat(a, 1);
  SubFloat(retVal, 2) = 1.0 / SubFloat(a, 2);
  SubFloat(retVal, 3) = 1.0 / SubFloat(a, 3);
  return retVal;
}

SOURCE_FORCEINLINE fltx4 ReciprocalSIMD(const fltx4 &a)  // 1/a
{
  fltx4 retVal;
  SubFloat(retVal, 0) = 1.0 / SubFloat(a, 0);
  SubFloat(retVal, 1) = 1.0 / SubFloat(a, 1);
  SubFloat(retVal, 2) = 1.0 / SubFloat(a, 2);
  SubFloat(retVal, 3) = 1.0 / SubFloat(a, 3);
  return retVal;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
SOURCE_FORCEINLINE fltx4 ReciprocalSaturateSIMD(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) =
      1.0 / (SubFloat(a, 0) == 0.0f ? FLT_EPSILON : SubFloat(a, 0));
  SubFloat(retVal, 1) =
      1.0 / (SubFloat(a, 1) == 0.0f ? FLT_EPSILON : SubFloat(a, 1));
  SubFloat(retVal, 2) =
      1.0 / (SubFloat(a, 2) == 0.0f ? FLT_EPSILON : SubFloat(a, 2));
  SubFloat(retVal, 3) =
      1.0 / (SubFloat(a, 3) == 0.0f ? FLT_EPSILON : SubFloat(a, 3));
  return retVal;
}

// 2^x for all values (the antilog)
SOURCE_FORCEINLINE fltx4 ExpSIMD(const fltx4 &toPower) {
  fltx4 retVal;
  SubFloat(retVal, 0) = powf(2, SubFloat(toPower, 0));
  SubFloat(retVal, 1) = powf(2, SubFloat(toPower, 1));
  SubFloat(retVal, 2) = powf(2, SubFloat(toPower, 2));
  SubFloat(retVal, 3) = powf(2, SubFloat(toPower, 3));

  return retVal;
}

SOURCE_FORCEINLINE fltx4 Dot3SIMD(const fltx4 &a, const fltx4 &b) {
  f32 flDot = SubFloat(a, 0) * SubFloat(b, 0) +
              SubFloat(a, 1) * SubFloat(b, 1) + SubFloat(a, 2) * SubFloat(b, 2);
  return ReplicateX4(flDot);
}

SOURCE_FORCEINLINE fltx4 Dot4SIMD(const fltx4 &a, const fltx4 &b) {
  f32 flDot = SubFloat(a, 0) * SubFloat(b, 0) +
              SubFloat(a, 1) * SubFloat(b, 1) +
              SubFloat(a, 2) * SubFloat(b, 2) + SubFloat(a, 3) * SubFloat(b, 3);
  return ReplicateX4(flDot);
}

// Clamps the components of a vector to a specified minimum and maximum range.
SOURCE_FORCEINLINE fltx4 ClampVectorSIMD(FLTX4 in, FLTX4 min, FLTX4 max) {
  return MaxSIMD(min, MinSIMD(max, in));
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
SOURCE_FORCEINLINE fltx4 SetWToZeroSIMD(const fltx4 &a) {
  fltx4 retval;
  retval = a;
  SubFloat(retval, 0) = 0;
  return retval;
}

SOURCE_FORCEINLINE fltx4 LoadUnalignedSIMD(const f32 *pSIMD) {
  return *(reinterpret_cast<const fltx4 *>(pSIMD));
}

SOURCE_FORCEINLINE fltx4 LoadUnaligned3SIMD(const f32 *pSIMD) {
  return *(reinterpret_cast<const fltx4 *>(pSIMD));
}

SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const f32 *pSIMD) {
  return *(reinterpret_cast<const fltx4 *>(pSIMD));
}

// for the transitional class -- load a 3-by VectorAligned and squash its w
// component
SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const VectorAligned &pSIMD) {
  fltx4 retval = LoadAlignedSIMD(pSIMD.Base());
  // squelch w
  SubInt(retval, 3) = 0;
  return retval;
}

SOURCE_FORCEINLINE void StoreAlignedSIMD(f32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<fltx4 *>(pSIMD)) = a;
}

SOURCE_FORCEINLINE void StoreUnalignedSIMD(f32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<fltx4 *>(pSIMD)) = a;
}

// strongly typed -- syntactic castor oil used for typechecking as we transition
// to SIMD
SOURCE_FORCEINLINE void StoreAligned3SIMD(VectorAligned *SOURCE_RESTRICT pSIMD,
                                   const fltx4 &a) {
  StoreAlignedSIMD(pSIMD->Base(), a);
}

SOURCE_FORCEINLINE void TransposeSIMD(fltx4 &x, fltx4 &y, fltx4 &z, fltx4 &w) {
#define SWAP_FLOATS(_a_, _ia_, _b_, _ib_)      \
  {                                            \
    f32 tmp = SubFloat(_a_, _ia_);             \
    SubFloat(_a_, _ia_) = SubFloat(_b_, _ib_); \
    SubFloat(_b_, _ib_) = tmp;                 \
  }
  SWAP_FLOATS(x, 1, y, 0);
  SWAP_FLOATS(x, 2, z, 0);
  SWAP_FLOATS(x, 3, w, 0);
  SWAP_FLOATS(y, 2, z, 1);
  SWAP_FLOATS(y, 3, w, 1);
  SWAP_FLOATS(z, 3, w, 2);
}

// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
SOURCE_FORCEINLINE fltx4 FindLowestSIMD3(const fltx4 &a) {
  f32 lowest = std::min(std::min(SubFloat(a, 0), SubFloat(a, 1)), SubFloat(a, 2));
  return ReplicateX4(lowest);
}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
SOURCE_FORCEINLINE fltx4 FindHighestSIMD3(const fltx4 &a) {
  f32 highest = std::max(std::max(SubFloat(a, 0), SubFloat(a, 1)), SubFloat(a, 2));
  return ReplicateX4(highest);
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way
// to guarantee an immediate as a parameter to function
// like this.
SOURCE_FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 *SOURCE_RESTRICT pDest,
                                        const fltx4 &vSrc) {
  (*pDest)[0] = SubFloat(vSrc, 0);
  (*pDest)[1] = SubFloat(vSrc, 1);
  (*pDest)[2] = SubFloat(vSrc, 2);
  (*pDest)[3] = SubFloat(vSrc, 3);
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------
// splat all components of a vector to a signed immediate int number.
SOURCE_FORCEINLINE fltx4 IntSetImmediateSIMD(int nValue) {
  fltx4 retval;
  SubInt(retval, 0) = SubInt(retval, 1) = SubInt(retval, 2) =
      SubInt(retval, 3) = nValue;
  return retval;
}

// Load 4 aligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadAlignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return *(reinterpret_cast<const i32x4 *>(pSIMD));
}

// Load 4 unaligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadUnalignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return *(reinterpret_cast<const i32x4 *>(pSIMD));
}

// save into four words, 16-byte aligned
SOURCE_FORCEINLINE void StoreAlignedIntSIMD(i32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<i32x4 *>(pSIMD)) = a;
}

SOURCE_FORCEINLINE void StoreAlignedIntSIMD(intx4 &pSIMD, const fltx4 &a) {
  *(reinterpret_cast<i32x4 *>(pSIMD.Base())) = a;
}

SOURCE_FORCEINLINE void StoreUnalignedIntSIMD(i32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<i32x4 *>(pSIMD)) = a;
}

// Take a fltx4 containing fixed-point uints and
// return them as single precision floats. No
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD(const u32x4 &vSrcA) {
  Assert(0); /* pc has no such operation */
  fltx4 retval;
  SubFloat(retval, 0) = ((f32)SubInt(retval, 0));
  SubFloat(retval, 1) = ((f32)SubInt(retval, 1));
  SubFloat(retval, 2) = ((f32)SubInt(retval, 2));
  SubFloat(retval, 3) = ((f32)SubInt(retval, 3));
  return retval;
}

#if 0 /* pc has no such op */
// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (f32) (reinterpret_cast<i32 *>(&vSrcA.m128_s32[0])) );
	SubFloat( retval, 1 ) = ( (f32) (reinterpret_cast<i32 *>(&vSrcA.m128_s32[1])) );
	SubFloat( retval, 2 ) = ( (f32) (reinterpret_cast<i32 *>(&vSrcA.m128_s32[2])) );
	SubFloat( retval, 3 ) = ( (f32) (reinterpret_cast<i32 *>(&vSrcA.m128_s32[3])) );
	return retval;
}


/*
works on fltx4's as if they are four uints.
the first parameter contains the words to be shifted,
the second contains the amount to shift by AS INTS

for i = 0 to 3
shift = vSrcB_i*32:(i*32)+4
vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
SOURCE_FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);


	return retval;
}
#endif

#elif (defined(_X360))

//---------------------------------------------------------------------
// X360 implementation
//---------------------------------------------------------------------

SOURCE_FORCEINLINE f32 &FloatSIMD(fltx4 &a, int idx) {
  fltx4_union &a_union = (fltx4_union &)a;
  return a_union.m128_f32[idx];
}

SOURCE_FORCEINLINE u32 &UIntSIMD(fltx4 &a, int idx) {
  fltx4_union &a_union = (fltx4_union &)a;
  return a_union.m128_u32[idx];
}

SOURCE_FORCEINLINE fltx4 AddSIMD(const fltx4 &a, const fltx4 &b) {
  return __vaddfp(a, b);
}

SOURCE_FORCEINLINE fltx4 SubSIMD(const fltx4 &a, const fltx4 &b)  // a-b
{
  return __vsubfp(a, b);
}

SOURCE_FORCEINLINE fltx4 MulSIMD(const fltx4 &a, const fltx4 &b)  // a*b
{
  return __vmulfp(a, b);
}

SOURCE_FORCEINLINE fltx4 MaddSIMD(const fltx4 &a, const fltx4 &b,
                           const fltx4 &c)  // a*b + c
{
  return __vmaddfp(a, b, c);
}

SOURCE_FORCEINLINE fltx4 Dot3SIMD(const fltx4 &a, const fltx4 &b) {
  return __vmsum3fp(a, b);
}

SOURCE_FORCEINLINE fltx4 Dot4SIMD(const fltx4 &a, const fltx4 &b) {
  return __vmsum4fp(a, b);
}

SOURCE_FORCEINLINE fltx4 SinSIMD(const fltx4 &radians) { return XMVectorSin(radians); }

SOURCE_FORCEINLINE void SinCos3SIMD(fltx4 &sine, fltx4 &cosine, const fltx4 &radians) {
  XMVectorSinCos(&sine, &cosine, radians);
}

SOURCE_FORCEINLINE void SinCosSIMD(fltx4 &sine, fltx4 &cosine,
                            const fltx4 &radians)  // a*b + c
{
  XMVectorSinCos(&sine, &cosine, radians);
}

SOURCE_FORCEINLINE fltx4 ArcSinSIMD(const fltx4 &sine) { return XMVectorASin(sine); }

// DivSIMD defined further down, since it uses ReciprocalSIMD

SOURCE_FORCEINLINE fltx4 MaxSIMD(const fltx4 &a, const fltx4 &b)  // std::max(a,b)
{
  return __vmaxfp(a, b);
}

SOURCE_FORCEINLINE fltx4 MinSIMD(const fltx4 &a, const fltx4 &b)  // std::min(a,b)
{
  return __vminfp(a, b);
}

SOURCE_FORCEINLINE fltx4 AndSIMD(const fltx4 &a, const fltx4 &b)  // a & b
{
  return __vand(a, b);
}

SOURCE_FORCEINLINE fltx4 AndNotSIMD(const fltx4 &a, const fltx4 &b)  // ~a & b
{
  // NOTE: a and b are swapped in the call: SSE complements the first argument,
  // VMX the second
  return __vandc(b, a);
}

SOURCE_FORCEINLINE fltx4 XorSIMD(const fltx4 &a, const fltx4 &b)  // a ^ b
{
  return __vxor(a, b);
}

SOURCE_FORCEINLINE fltx4 OrSIMD(const fltx4 &a, const fltx4 &b)  // a | b
{
  return __vor(a, b);
}

SOURCE_FORCEINLINE fltx4 NegSIMD(const fltx4 &a)  // negate: -a
{
  return XMVectorNegate(a);
}

SOURCE_FORCEINLINE bool IsAllZeros(const fltx4 &a)  // all floats of a zero?
{
  u32 equalFlags = 0;
  __vcmpeqfpR(a, Four_Zeros, &equalFlags);
  return XMComparisonAllTrue(equalFlags);
}

SOURCE_FORCEINLINE bool IsAnyZeros(const fltx4 &a)  // any floats are zero?
{
  u32 conditionregister;
  XMVectorEqualR(&conditionregister, a, XMVectorZero());
  return XMComparisonAnyTrue(conditionregister);
}

SOURCE_FORCEINLINE bool IsAnyXYZZero(const fltx4 &a)  // are any of x,y,z zero?
{
  // copy a's x component into w, in case w was zero.
  fltx4 temp = __vrlimi(a, a, 1, 1);
  u32 conditionregister;
  XMVectorEqualR(&conditionregister, temp, XMVectorZero());
  return XMComparisonAnyTrue(conditionregister);
}

// for branching when a.xyzw > b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThan(const fltx4 &a, const fltx4 &b) {
  u32 cr;
  XMVectorGreaterR(&cr, a, b);
  return XMComparisonAllTrue(cr);
}

// for branching when a.xyzw >= b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThanOrEq(const fltx4 &a, const fltx4 &b) {
  u32 cr;
  XMVectorGreaterOrEqualR(&cr, a, b);
  return XMComparisonAllTrue(cr);
}

// For branching if all a.xyzw == b.xyzw
SOURCE_FORCEINLINE bool IsAllEqual(const fltx4 &a, const fltx4 &b) {
  u32 cr;
  XMVectorEqualR(&cr, a, b);
  return XMComparisonAllTrue(cr);
}

SOURCE_FORCEINLINE int TestSignSIMD(
    const fltx4 &a)  // mask of which floats have the high bit set
{
  // NOTE: this maps to SSE way better than it does to VMX (most code uses
  // IsAnyNegative(), though)
  int nRet = 0;

  const fltx4_union &a_union = (const fltx4_union &)a;
  nRet |= (a_union.m128_u32[0] & 0x80000000) >> 31;  // sign(x) -> bit 0
  nRet |= (a_union.m128_u32[1] & 0x80000000) >> 30;  // sign(y) -> bit 1
  nRet |= (a_union.m128_u32[2] & 0x80000000) >> 29;  // sign(z) -> bit 2
  nRet |= (a_union.m128_u32[3] & 0x80000000) >> 28;  // sign(w) -> bit 3

  return nRet;
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
SOURCE_FORCEINLINE fltx4 SetWToZeroSIMD(const fltx4 &a) {
  return __vrlimi(a, __vzero(), 1, 0);
}

SOURCE_FORCEINLINE bool IsAnyNegative(
    const fltx4 &a)  // (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
  // NOTE: this tests the top bits of each vector element using integer math
  //       (so it ignores NaNs - it will return true for "-NaN")
  u32 equalFlags = 0;
  fltx4 signMask = __vspltisw(-1);  // 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF
                                    // 0xFFFFFFFF (low order 5 bits of each
                                    // element = 31)
  signMask = __vslw(signMask,
                    signMask);  // 0x80000000 0x80000000 0x80000000 0x80000000
  __vcmpequwR(Four_Zeros, __vand(signMask, a), &equalFlags);
  return !XMComparisonAllTrue(equalFlags);
}

SOURCE_FORCEINLINE fltx4 CmpEqSIMD(const fltx4 &a, const fltx4 &b)  // (a==b) ? ~0:0
{
  return __vcmpeqfp(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpGtSIMD(const fltx4 &a, const fltx4 &b)  // (a>b) ? ~0:0
{
  return __vcmpgtfp(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpGeSIMD(const fltx4 &a, const fltx4 &b)  // (a>=b) ? ~0:0
{
  return __vcmpgefp(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpLtSIMD(const fltx4 &a, const fltx4 &b)  // (a<b) ? ~0:0
{
  return __vcmpgtfp(b, a);
}

SOURCE_FORCEINLINE fltx4 CmpLeSIMD(const fltx4 &a, const fltx4 &b)  // (a<=b) ? ~0:0
{
  return __vcmpgefp(b, a);
}

SOURCE_FORCEINLINE fltx4
CmpInBoundsSIMD(const fltx4 &a, const fltx4 &b)  // (a <= b && a >= -b) ? ~0 : 0
{
  return XMVectorInBounds(a, b);
}

// returned[i] = ReplacementMask[i] == 0 ? OldValue : NewValue
SOURCE_FORCEINLINE fltx4 MaskedAssign(const fltx4 &ReplacementMask,
                               const fltx4 &NewValue, const fltx4 &OldValue) {
  return __vsel(OldValue, NewValue, ReplacementMask);
}

// AKA "Broadcast", "Splat"
SOURCE_FORCEINLINE fltx4 ReplicateX4(f32 flValue)  //  a,a,a,a
{
  // NOTE: if flValue comes from a register, this causes a Load-Hit-Store stall
  // (don't mix fpu/vpu math!)
  f32 *pValue = &flValue;
  Assert(pValue);
  Assert(((u32)pValue & 3) == 0);
  return __vspltw(__lvlx(pValue, 0), 0);
}

SOURCE_FORCEINLINE fltx4 ReplicateX4(const f32 *pValue)  //  a,a,a,a
{
  Assert(pValue);
  return __vspltw(__lvlx(pValue, 0), 0);
}

/// replicate a single 32 bit integer value to all 4 components of an m128
SOURCE_FORCEINLINE fltx4 ReplicateIX4(int nValue) {
  // NOTE: if nValue comes from a register, this causes a Load-Hit-Store stall
  // (should not mix ints with fltx4s!)
  int *pValue = &nValue;
  Assert(pValue);
  Assert(((u32)pValue & 3) == 0);
  return __vspltw(__lvlx(pValue, 0), 0);
}

// Round towards positive infinity
SOURCE_FORCEINLINE fltx4 CeilSIMD(const fltx4 &a) { return __vrfip(a); }

// Round towards nearest integer
SOURCE_FORCEINLINE fltx4 RoundSIMD(const fltx4 &a) { return __vrfin(a); }

// Round towards negative infinity
SOURCE_FORCEINLINE fltx4 FloorSIMD(const fltx4 &a) { return __vrfim(a); }

SOURCE_FORCEINLINE fltx4 SqrtEstSIMD(const fltx4 &a)  // sqrt(a), more or less
{
  // This is emulated from rsqrt
  return XMVectorSqrtEst(a);
}

SOURCE_FORCEINLINE fltx4 SqrtSIMD(const fltx4 &a)  // sqrt(a)
{
  // This is emulated from rsqrt
  return XMVectorSqrt(a);
}

SOURCE_FORCEINLINE fltx4
ReciprocalSqrtEstSIMD(const fltx4 &a)  // 1/sqrt(a), more or less
{
  return __vrsqrtefp(a);
}

SOURCE_FORCEINLINE fltx4 ReciprocalSqrtSIMD(const fltx4 &a)  // 1/sqrt(a)
{
  // This uses Newton-Raphson to improve the HW result
  return XMVectorReciprocalSqrt(a);
}

SOURCE_FORCEINLINE fltx4 ReciprocalEstSIMD(const fltx4 &a)  // 1/a, more or less
{
  return __vrefp(a);
}

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton
/// iteration. No error checking!
SOURCE_FORCEINLINE fltx4 ReciprocalSIMD(const fltx4 &a)  // 1/a
{
  // This uses Newton-Raphson to improve the HW result
  return XMVectorReciprocal(a);
}

// TODO(d.rattman): on 360, this is very slow, since it uses ReciprocalSIMD (do we need
// DivEstSIMD?)
SOURCE_FORCEINLINE fltx4 DivSIMD(const fltx4 &a, const fltx4 &b)  // a/b
{
  return MulSIMD(ReciprocalSIMD(b), a);
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
SOURCE_FORCEINLINE fltx4 ReciprocalSaturateSIMD(const fltx4 &a) {
  // Convert zeros to epsilons
  fltx4 zero_mask = CmpEqSIMD(a, Four_Zeros);
  fltx4 a_safe = OrSIMD(a, AndSIMD(Four_Epsilons, zero_mask));
  return ReciprocalSIMD(a_safe);

  // TODO(d.rattman): This could be faster (BUT: it doesn't preserve the sign of -0.0,
  // whereas the above does) fltx4 zeroMask = CmpEqSIMD( Four_Zeros, a ); fltx4
  // a_safe = XMVectorSelect( a, Four_Epsilons, zeroMask ); return
  // ReciprocalSIMD( a_safe );
}

// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
SOURCE_FORCEINLINE fltx4 ExpSIMD(const fltx4 &toPower) { return XMVectorExp(toPower); }

// Clamps the components of a vector to a specified minimum and maximum range.
SOURCE_FORCEINLINE fltx4 ClampVectorSIMD(FLTX4 in, FLTX4 min, FLTX4 max) {
  return XMVectorClamp(in, min, max);
}

SOURCE_FORCEINLINE fltx4 LoadUnalignedSIMD(const f32 *pSIMD) {
  return XMLoadVector4((const void *)pSIMD);
}

// load a 3-vector (as opposed to LoadUnalignedSIMD, which loads a 4-vec).
SOURCE_FORCEINLINE fltx4 LoadUnaligned3SIMD(const f32 *pSIMD) {
  return XMLoadVector3((const void *)pSIMD);
}

SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const f32 *pSIMD) {
  return *(reinterpret_cast<const fltx4 *>(pSIMD));
}

// for the transitional class -- load a 3-by VectorAligned and squash its w
// component
SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const VectorAligned &pSIMD) {
  fltx4 out = XMLoadVector3A(pSIMD.Base());
  // squelch w
  return __vrlimi(out, __vzero(), 1, 0);
}

// for the transitional class -- load a 3-by VectorAligned and squash its w
// component
SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const VectorAligned *SOURCE_RESTRICT pSIMD) {
  fltx4 out = XMLoadVector3A(pSIMD);
  // squelch w
  return __vrlimi(out, __vzero(), 1, 0);
}

SOURCE_FORCEINLINE void StoreAlignedSIMD(f32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<fltx4 *>(pSIMD)) = a;
}

SOURCE_FORCEINLINE void StoreUnalignedSIMD(f32 *pSIMD, const fltx4 &a) {
  XMStoreVector4(pSIMD, a);
}

// strongly typed -- for typechecking as we transition to SIMD
SOURCE_FORCEINLINE void StoreAligned3SIMD(VectorAligned *SOURCE_RESTRICT pSIMD,
                                   const fltx4 &a) {
  XMStoreVector3A(pSIMD->Base(), a);
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way
// to guarantee an immediate as a parameter to function
// like this.
SOURCE_FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 *SOURCE_RESTRICT pDest,
                                        const fltx4 &vSrc) {
  fltx4 asInt = __vctsxs(vSrc, 0);
  XMStoreVector4A(pDest->Base(), asInt);
}

SOURCE_FORCEINLINE void TransposeSIMD(fltx4 &x, fltx4 &y, fltx4 &z, fltx4 &w) {
  XMMATRIX xyzwMatrix = _XMMATRIX(x, y, z, w);
  xyzwMatrix = XMMatrixTranspose(xyzwMatrix);
  x = xyzwMatrix.r[0];
  y = xyzwMatrix.r[1];
  z = xyzwMatrix.r[2];
  w = xyzwMatrix.r[3];
}

// Return one in the fastest way -- faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadZeroSIMD() { return XMVectorZero(); }

// Return one in the fastest way -- faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadOneSIMD() { return XMVectorSplatOne(); }

SOURCE_FORCEINLINE fltx4 SplatXSIMD(fltx4 a) { return XMVectorSplatX(a); }

SOURCE_FORCEINLINE fltx4 SplatYSIMD(fltx4 a) { return XMVectorSplatY(a); }

SOURCE_FORCEINLINE fltx4 SplatZSIMD(fltx4 a) { return XMVectorSplatZ(a); }

SOURCE_FORCEINLINE fltx4 SplatWSIMD(fltx4 a) { return XMVectorSplatW(a); }

SOURCE_FORCEINLINE fltx4 SetXSIMD(const fltx4 &a, const fltx4 &x) {
  fltx4 result = __vrlimi(a, x, 8, 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetYSIMD(const fltx4 &a, const fltx4 &y) {
  fltx4 result = __vrlimi(a, y, 4, 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetZSIMD(const fltx4 &a, const fltx4 &z) {
  fltx4 result = __vrlimi(a, z, 2, 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetWSIMD(const fltx4 &a, const fltx4 &w) {
  fltx4 result = __vrlimi(a, w, 1, 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetComponentSIMD(const fltx4 &a, int nComponent,
                                   f32 flValue) {
  static int s_nVrlimiMask[4] = {8, 4, 2, 1};
  fltx4 val = ReplicateX4(flValue);
  fltx4 result = __vrlimi(a, val, s_nVrlimiMask[nComponent], 0);
  return result;
}

SOURCE_FORCEINLINE fltx4 RotateLeft(const fltx4 &a) {
  fltx4 compareOne = a;
  return __vrlimi(compareOne, a, 8 | 4 | 2 | 1, 1);
}

SOURCE_FORCEINLINE fltx4 RotateLeft2(const fltx4 &a) {
  fltx4 compareOne = a;
  return __vrlimi(compareOne, a, 8 | 4 | 2 | 1, 2);
}

// find the lowest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Though this is only five instructions long,
// they are all dependent, making this stall city.
// Forcing this inline should hopefully help with scheduling.
SOURCE_FORCEINLINE fltx4 FindLowestSIMD3(const fltx4 &a) {
  // a is [x,y,z,G] (where G is garbage)
  // rotate left by one
  fltx4 compareOne = a;
  compareOne = __vrlimi(compareOne, a, 8 | 4, 1);
  // compareOne is [y,z,G,G]
  fltx4 retval = MinSIMD(a, compareOne);
  // retVal is [std::min(x,y), std::min(y,z), G, G]
  compareOne = __vrlimi(compareOne, a, 8, 2);
  // compareOne is [z, G, G, G]
  retval = MinSIMD(retval, compareOne);
  // retVal = [ std::min(std::min(x,y),z), G, G, G ]

  // splat the x component out to the whole vector and return
  return SplatXSIMD(retval);
}

// find the highest component of a.x, a.y, a.z,
// and replicate it to the whole return value.
// ignores a.w.
// Though this is only five instructions long,
// they are all dependent, making this stall city.
// Forcing this inline should hopefully help with scheduling.
SOURCE_FORCEINLINE fltx4 FindHighestSIMD3(const fltx4 &a) {
  // a is [x,y,z,G] (where G is garbage)
  // rotate left by one
  fltx4 compareOne = a;
  compareOne = __vrlimi(compareOne, a, 8 | 4, 1);
  // compareOne is [y,z,G,G]
  fltx4 retval = MaxSIMD(a, compareOne);
  // retVal is [std::max(x,y), std::max(y,z), G, G]
  compareOne = __vrlimi(compareOne, a, 8, 2);
  // compareOne is [z, G, G, G]
  retval = MaxSIMD(retval, compareOne);
  // retVal = [ std::max(std::max(x,y),z), G, G, G ]

  // splat the x component out to the whole vector and return
  return SplatXSIMD(retval);
}

// Transform many (horizontal) points in-place by a 3x4 matrix,
// here already loaded onto three fltx4 registers.
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1.
// To spare yourself the annoyance of loading the matrix yourself,
// use one of the overloads below.
void TransformManyPointsBy(VectorAligned *SOURCE_RESTRICT pVectors, u32 numVectors,
                           FLTX4 mRow1, FLTX4 mRow2, FLTX4 mRow3);

// Transform many (horizontal) points in-place by a 3x4 matrix.
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1.
// In this function, the matrix need not be aligned.
SOURCE_FORCEINLINE void TransformManyPointsBy(VectorAligned *SOURCE_RESTRICT pVectors,
                                       u32 numVectors,
                                       const matrix3x4_t &pMatrix) {
  return TransformManyPointsBy(
      pVectors, numVectors, LoadUnalignedSIMD(pMatrix[0]),
      LoadUnalignedSIMD(pMatrix[1]), LoadUnalignedSIMD(pMatrix[2]));
}

// Transform many (horizontal) points in-place by a 3x4 matrix.
// The points must be stored as 16-byte aligned. They are points
// and not vectors because we assume the w-component to be 1.
// In this function, the matrix must itself be aligned on a 16-byte
// boundary.
SOURCE_FORCEINLINE void TransformManyPointsByA(VectorAligned *SOURCE_RESTRICT pVectors,
                                        u32 numVectors,
                                        const matrix3x4_t &pMatrix) {
  return TransformManyPointsBy(
      pVectors, numVectors, LoadAlignedSIMD(pMatrix[0]),
      LoadAlignedSIMD(pMatrix[1]), LoadAlignedSIMD(pMatrix[2]));
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------

// Load 4 aligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadAlignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return XMLoadVector4A(pSIMD);
}

// Load 4 unaligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadUnalignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return XMLoadVector4(pSIMD);
}

// save into four words, 16-byte aligned
SOURCE_FORCEINLINE void StoreAlignedIntSIMD(i32 *pSIMD, const fltx4 &a) {
  *(reinterpret_cast<i32x4 *>(pSIMD)) = a;
}

SOURCE_FORCEINLINE void StoreAlignedIntSIMD(intx4 &pSIMD, const fltx4 &a) {
  *(reinterpret_cast<i32x4 *>(pSIMD.Base())) = a;
}

SOURCE_FORCEINLINE void StoreUnalignedIntSIMD(i32 *pSIMD, const fltx4 &a) {
  XMStoreVector4(pSIMD, a);
}

// Take a fltx4 containing fixed-point uints and
// return them as single precision floats. No
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD(const i32x4 &vSrcA) {
  return __vcfux(vSrcA, 0);
}

// Take a fltx4 containing fixed-point sints and
// return them as single precision floats. No
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 SignedIntConvertToFltSIMD(const i32x4 &vSrcA) {
  return __vcfsx(vSrcA, 0);
}

// Take a fltx4 containing fixed-point uints and
// return them as single precision floats. Each uint
// will be divided by 2^immed after conversion
// (eg, this is fixed point math).
/* as if:
SOURCE_FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const i32x4 &vSrcA, u32
uImmed )
{
        return __vcfux( vSrcA, uImmed );
}
*/
#define UnsignedFixedIntConvertToFltSIMD(vSrcA, uImmed) \
  (__vcfux((vSrcA), (uImmed)))

// Take a fltx4 containing fixed-point sints and
// return them as single precision floats. Each int
// will be divided by 2^immed (eg, this is fixed point
// math).
/* as if:
SOURCE_FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA, u32
uImmed )
{
        return __vcfsx( vSrcA, uImmed );
}
*/
#define SignedFixedIntConvertToFltSIMD(vSrcA, uImmed) \
  (__vcfsx((vSrcA), (uImmed)))

// set all components of a vector to a signed immediate int number.
/* as if:
SOURCE_FORCEINLINE fltx4 IntSetImmediateSIMD(int toImmediate)
{
        return __vspltisw( toImmediate );
}
*/
#define IntSetImmediateSIMD(x) (__vspltisw(x))

/*
works on fltx4's as if they are four uints.
the first parameter contains the words to be shifted,
the second contains the amount to shift by AS INTS

for i = 0 to 3
shift = vSrcB_i*32:(i*32)+4
vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
SOURCE_FORCEINLINE fltx4 IntShiftLeftWordSIMD(fltx4 vSrcA, fltx4 vSrcB) {
  return __vslw(vSrcA, vSrcB);
}

SOURCE_FORCEINLINE f32 SubFloat(const fltx4 &a, int idx) {
  // NOTE: if the output goes into a register, this causes a Load-Hit-Store
  // stall (don't mix fpu/vpu math!)
  const fltx4_union &a_union = (const fltx4_union &)a;
  return a_union.m128_f32[idx];
}

SOURCE_FORCEINLINE f32 &SubFloat(fltx4 &a, int idx) {
  fltx4_union &a_union = (fltx4_union &)a;
  return a_union.m128_f32[idx];
}

SOURCE_FORCEINLINE u32 SubFloatConvertToInt(const fltx4 &a, int idx) {
  fltx4 t = __vctuxs(a, 0);
  const fltx4_union &a_union = (const fltx4_union &)t;
  return a_union.m128_u32[idx];
}

SOURCE_FORCEINLINE u32 SubInt(const fltx4 &a, int idx) {
  const fltx4_union &a_union = (const fltx4_union &)a;
  return a_union.m128_u32[idx];
}

SOURCE_FORCEINLINE u32 &SubInt(fltx4 &a, int idx) {
  fltx4_union &a_union = (fltx4_union &)a;
  return a_union.m128_u32[idx];
}

#else

//---------------------------------------------------------------------
// Intel/SSE implementation
//---------------------------------------------------------------------

SOURCE_FORCEINLINE void StoreAlignedSIMD(f32 *SOURCE_RESTRICT pSIMD, const fltx4 &a) {
  _mm_store_ps(pSIMD, a);
}

SOURCE_FORCEINLINE void StoreUnalignedSIMD(f32 *SOURCE_RESTRICT pSIMD, const fltx4 &a) {
  _mm_storeu_ps(pSIMD, a);
}

// strongly typed -- syntactic castor oil used for typechecking as we transition
// to SIMD
SOURCE_FORCEINLINE void StoreAligned3SIMD(VectorAligned *SOURCE_RESTRICT pSIMD,
                                   const fltx4 &a) {
  StoreAlignedSIMD(pSIMD->Base(), a);
}

SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const f32 *pSIMD) {
  return _mm_load_ps(pSIMD);
}

SOURCE_FORCEINLINE fltx4 AndSIMD(const fltx4 &a, const fltx4 &b)  // a & b
{
  return _mm_and_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 AndNotSIMD(const fltx4 &a, const fltx4 &b)  // a & ~b
{
  return _mm_andnot_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 XorSIMD(const fltx4 &a, const fltx4 &b)  // a ^ b
{
  return _mm_xor_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 OrSIMD(const fltx4 &a, const fltx4 &b)  // a | b
{
  return _mm_or_ps(a, b);
}

// Squelch the w component of a vector to +0.0.
// Most efficient when you say a = SetWToZeroSIMD(a) (avoids a copy)
SOURCE_FORCEINLINE fltx4 SetWToZeroSIMD(const fltx4 &a) {
  return AndSIMD(a, LoadAlignedSIMD((const f32 *)g_SIMD_clear_wmask));
}

// for the transitional class -- load a 3-by VectorAligned and squash its w
// component
SOURCE_FORCEINLINE fltx4 LoadAlignedSIMD(const VectorAligned &pSIMD) {
  return SetWToZeroSIMD(LoadAlignedSIMD(pSIMD.Base()));
}

SOURCE_FORCEINLINE fltx4 LoadUnalignedSIMD(const f32 *pSIMD) {
  return _mm_loadu_ps(pSIMD);
}

SOURCE_FORCEINLINE fltx4 LoadUnaligned3SIMD(const f32 *pSIMD) {
  return _mm_loadu_ps(pSIMD);
}

/// replicate a single 32 bit integer value to all 4 components of an m128
SOURCE_FORCEINLINE fltx4 ReplicateIX4(int i) {
  fltx4 value = _mm_set_ss(*((f32 *)&i));
  return _mm_shuffle_ps(value, value, 0);
}

SOURCE_FORCEINLINE fltx4 ReplicateX4(f32 flValue) {
  __m128 value = _mm_set_ss(flValue);
  return _mm_shuffle_ps(value, value, 0);
}

SOURCE_FORCEINLINE f32 SubFloat(const fltx4 &a, int idx) {
  // NOTE: if the output goes into a register, this causes a Load-Hit-Store
  // stall (don't mix fpu/vpu math!)
#ifndef OS_POSIX
  return a.m128_f32[idx];
#else
  return (reinterpret_cast<f32 const *>(&a))[idx];
#endif
}

SOURCE_FORCEINLINE f32 &SubFloat(fltx4 &a, int idx) {
#ifndef OS_POSIX
  return a.m128_f32[idx];
#else
  return (reinterpret_cast<f32 *>(&a))[idx];
#endif
}

SOURCE_FORCEINLINE u32 SubFloatConvertToInt(const fltx4 &a, int idx) {
  return (u32)SubFloat(a, idx);
}

SOURCE_FORCEINLINE u32 SubInt(const fltx4 &a, int idx) {
#ifndef OS_POSIX
  return a.m128_u32[idx];
#else
  return (reinterpret_cast<u32 const *>(&a))[idx];
#endif
}

SOURCE_FORCEINLINE u32 &SubInt(fltx4 &a, int idx) {
#ifndef OS_POSIX
  return a.m128_u32[idx];
#else
  return (reinterpret_cast<u32 *>(&a))[idx];
#endif
}

// Return one in the fastest way -- on the x360, faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadZeroSIMD() { return Four_Zeros; }

// Return one in the fastest way -- on the x360, faster even than loading.
SOURCE_FORCEINLINE fltx4 LoadOneSIMD() { return Four_Ones; }

SOURCE_FORCEINLINE fltx4 MaskedAssign(const fltx4 &ReplacementMask,
                               const fltx4 &NewValue, const fltx4 &OldValue) {
  return OrSIMD(AndSIMD(ReplacementMask, NewValue),
                AndNotSIMD(ReplacementMask, OldValue));
}

// remember, the SSE numbers its words 3 2 1 0
// The way we want to specify shuffles is backwards from the default
// MM_SHUFFLE_REV is in array index order (default is reversed)
#define MM_SHUFFLE_REV(a, b, c, d) _MM_SHUFFLE(d, c, b, a)

SOURCE_FORCEINLINE fltx4 SplatXSIMD(fltx4 const &a) {
  return _mm_shuffle_ps(a, a, MM_SHUFFLE_REV(0, 0, 0, 0));
}

SOURCE_FORCEINLINE fltx4 SplatYSIMD(fltx4 const &a) {
  return _mm_shuffle_ps(a, a, MM_SHUFFLE_REV(1, 1, 1, 1));
}

SOURCE_FORCEINLINE fltx4 SplatZSIMD(fltx4 const &a) {
  return _mm_shuffle_ps(a, a, MM_SHUFFLE_REV(2, 2, 2, 2));
}

SOURCE_FORCEINLINE fltx4 SplatWSIMD(fltx4 const &a) {
  return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3));
}

SOURCE_FORCEINLINE fltx4 SetXSIMD(const fltx4 &a, const fltx4 &x) {
  fltx4 result = MaskedAssign(
      LoadAlignedSIMD((const f32 *)(g_SIMD_ComponentMask[0])), x, a);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetYSIMD(const fltx4 &a, const fltx4 &y) {
  fltx4 result = MaskedAssign(
      LoadAlignedSIMD((const f32 *)(g_SIMD_ComponentMask[1])), y, a);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetZSIMD(const fltx4 &a, const fltx4 &z) {
  fltx4 result = MaskedAssign(
      LoadAlignedSIMD((const f32 *)(g_SIMD_ComponentMask[2])), z, a);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetWSIMD(const fltx4 &a, const fltx4 &w) {
  fltx4 result = MaskedAssign(
      LoadAlignedSIMD((const f32 *)(g_SIMD_ComponentMask[3])), w, a);
  return result;
}

SOURCE_FORCEINLINE fltx4 SetComponentSIMD(const fltx4 &a, int nComponent,
                                   f32 flValue) {
  fltx4 val = ReplicateX4(flValue);
  fltx4 result = MaskedAssign(
      LoadAlignedSIMD((const f32 *)(g_SIMD_ComponentMask[nComponent])), val, a);
  return result;
}

// a b c d -> b c d a
SOURCE_FORCEINLINE fltx4 RotateLeft(const fltx4 &a) {
  return _mm_shuffle_ps(a, a, MM_SHUFFLE_REV(1, 2, 3, 0));
}

// a b c d -> c d a b
SOURCE_FORCEINLINE fltx4 RotateLeft2(const fltx4 &a) {
  return _mm_shuffle_ps(a, a, MM_SHUFFLE_REV(2, 3, 0, 1));
}

// a b c d -> d a b c
SOURCE_FORCEINLINE fltx4 RotateRight(const fltx4 &a) {
  return _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 3, 2, 1));
}

// a b c d -> c d a b
SOURCE_FORCEINLINE fltx4 RotateRight2(const fltx4 &a) {
  return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 0, 3, 2));
}

SOURCE_FORCEINLINE fltx4 AddSIMD(const fltx4 &a, const fltx4 &b)  // a+b
{
  return _mm_add_ps(a, b);
};

SOURCE_FORCEINLINE fltx4 SubSIMD(const fltx4 &a, const fltx4 &b)  // a-b
{
  return _mm_sub_ps(a, b);
};

SOURCE_FORCEINLINE fltx4 MulSIMD(const fltx4 &a, const fltx4 &b)  // a*b
{
  return _mm_mul_ps(a, b);
};

SOURCE_FORCEINLINE fltx4 DivSIMD(const fltx4 &a, const fltx4 &b)  // a/b
{
  return _mm_div_ps(a, b);
};

SOURCE_FORCEINLINE fltx4 MaddSIMD(const fltx4 &a, const fltx4 &b,
                           const fltx4 &c)  // a*b + c
{
  return AddSIMD(MulSIMD(a, b), c);
}

SOURCE_FORCEINLINE fltx4 Dot3SIMD(const fltx4 &a, const fltx4 &b) {
  fltx4 m = MulSIMD(a, b);
  f32 flDot = SubFloat(m, 0) + SubFloat(m, 1) + SubFloat(m, 2);
  return ReplicateX4(flDot);
}

SOURCE_FORCEINLINE fltx4 Dot4SIMD(const fltx4 &a, const fltx4 &b) {
  fltx4 m = MulSIMD(a, b);
  f32 flDot = SubFloat(m, 0) + SubFloat(m, 1) + SubFloat(m, 2) + SubFloat(m, 3);
  return ReplicateX4(flDot);
}

// TODO: implement as four-way Taylor series (see xbox implementation)
SOURCE_FORCEINLINE fltx4 SinSIMD(const fltx4 &radians) {
  fltx4 result;
  SubFloat(result, 0) = sin(SubFloat(radians, 0));
  SubFloat(result, 1) = sin(SubFloat(radians, 1));
  SubFloat(result, 2) = sin(SubFloat(radians, 2));
  SubFloat(result, 3) = sin(SubFloat(radians, 3));
  return result;
}

SOURCE_FORCEINLINE void SinCos3SIMD(fltx4 &sine, fltx4 &cosine, const fltx4 &radians) {
  // TODO(d.rattman): Make a fast SSE version
  SinCos(SubFloat(radians, 0), &SubFloat(sine, 0), &SubFloat(cosine, 0));
  SinCos(SubFloat(radians, 1), &SubFloat(sine, 1), &SubFloat(cosine, 1));
  SinCos(SubFloat(radians, 2), &SubFloat(sine, 2), &SubFloat(cosine, 2));
}

SOURCE_FORCEINLINE void SinCosSIMD(fltx4 &sine, fltx4 &cosine,
                            const fltx4 &radians)  // a*b + c
{
  // TODO(d.rattman): Make a fast SSE version
  SinCos(SubFloat(radians, 0), &SubFloat(sine, 0), &SubFloat(cosine, 0));
  SinCos(SubFloat(radians, 1), &SubFloat(sine, 1), &SubFloat(cosine, 1));
  SinCos(SubFloat(radians, 2), &SubFloat(sine, 2), &SubFloat(cosine, 2));
  SinCos(SubFloat(radians, 3), &SubFloat(sine, 3), &SubFloat(cosine, 3));
}

// TODO: implement as four-way Taylor series (see xbox implementation)
SOURCE_FORCEINLINE fltx4 ArcSinSIMD(const fltx4 &sine) {
  // TODO(d.rattman): Make a fast SSE version
  fltx4 result;
  SubFloat(result, 0) = asin(SubFloat(sine, 0));
  SubFloat(result, 1) = asin(SubFloat(sine, 1));
  SubFloat(result, 2) = asin(SubFloat(sine, 2));
  SubFloat(result, 3) = asin(SubFloat(sine, 3));
  return result;
}

SOURCE_FORCEINLINE fltx4 NegSIMD(const fltx4 &a)  // negate: -a
{
  return SubSIMD(LoadZeroSIMD(), a);
}

SOURCE_FORCEINLINE int TestSignSIMD(
    const fltx4 &a)  // mask of which floats have the high bit set
{
  return _mm_movemask_ps(a);
}

SOURCE_FORCEINLINE bool IsAnyNegative(
    const fltx4 &a)  // (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
  return (0 != TestSignSIMD(a));
}

SOURCE_FORCEINLINE fltx4 CmpEqSIMD(const fltx4 &a, const fltx4 &b)  // (a==b) ? ~0:0
{
  return _mm_cmpeq_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpGtSIMD(const fltx4 &a, const fltx4 &b)  // (a>b) ? ~0:0
{
  return _mm_cmpgt_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpGeSIMD(const fltx4 &a, const fltx4 &b)  // (a>=b) ? ~0:0
{
  return _mm_cmpge_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpLtSIMD(const fltx4 &a, const fltx4 &b)  // (a<b) ? ~0:0
{
  return _mm_cmplt_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 CmpLeSIMD(const fltx4 &a, const fltx4 &b)  // (a<=b) ? ~0:0
{
  return _mm_cmple_ps(a, b);
}

// for branching when a.xyzw > b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThan(const fltx4 &a, const fltx4 &b) {
  return TestSignSIMD(CmpLeSIMD(a, b)) == 0;
}

// for branching when a.xyzw >= b.xyzw
SOURCE_FORCEINLINE bool IsAllGreaterThanOrEq(const fltx4 &a, const fltx4 &b) {
  return TestSignSIMD(CmpLtSIMD(a, b)) == 0;
}

// For branching if all a.xyzw == b.xyzw
SOURCE_FORCEINLINE bool IsAllEqual(const fltx4 &a, const fltx4 &b) {
  return TestSignSIMD(CmpEqSIMD(a, b)) == 0xf;
}

SOURCE_FORCEINLINE fltx4
CmpInBoundsSIMD(const fltx4 &a, const fltx4 &b)  // (a <= b && a >= -b) ? ~0 : 0
{
  return AndSIMD(CmpLeSIMD(a, b), CmpGeSIMD(a, NegSIMD(b)));
}

SOURCE_FORCEINLINE fltx4 MinSIMD(const fltx4 &a, const fltx4 &b)  // std::min(a,b)
{
  return _mm_min_ps(a, b);
}

SOURCE_FORCEINLINE fltx4 MaxSIMD(const fltx4 &a, const fltx4 &b)  // std::max(a,b)
{
  return _mm_max_ps(a, b);
}

// SSE lacks rounding operations.
// Really.
// You can emulate them by setting the rounding mode for the
// whole processor and then converting to int, and then back again.
// But every time you set the rounding mode, you clear out the
// entire pipeline. So, I can't do them per operation. You
// have to do it once, before the loop that would call these.
// Round towards positive infinity
SOURCE_FORCEINLINE fltx4 CeilSIMD(const fltx4 &a) {
  fltx4 retVal;
  SubFloat(retVal, 0) = ceil(SubFloat(a, 0));
  SubFloat(retVal, 1) = ceil(SubFloat(a, 1));
  SubFloat(retVal, 2) = ceil(SubFloat(a, 2));
  SubFloat(retVal, 3) = ceil(SubFloat(a, 3));
  return retVal;
}

fltx4 fabs(const fltx4 &x);
// Round towards negative infinity
// This is the implementation that was here before; it assumes
// you are in round-to-floor mode, which I guess is usually the
// case for us vis-a-vis SSE. It's totally unnecessary on
// VMX, which has a native floor op.
SOURCE_FORCEINLINE fltx4 FloorSIMD(const fltx4 &val) {
  fltx4 fl4Abs = fabs(val);
  fltx4 ival = SubSIMD(AddSIMD(fl4Abs, Four_2ToThe23s), Four_2ToThe23s);
  ival = MaskedAssign(CmpGtSIMD(ival, fl4Abs), SubSIMD(ival, Four_Ones), ival);
  return XorSIMD(ival, XorSIMD(val, fl4Abs));  // restore sign bits
}

inline bool IsAllZeros(const fltx4 &var) {
  return TestSignSIMD(CmpEqSIMD(var, Four_Zeros)) == 0xF;
}

SOURCE_FORCEINLINE fltx4 SqrtEstSIMD(const fltx4 &a)  // sqrt(a), more or less
{
  return _mm_sqrt_ps(a);
}

SOURCE_FORCEINLINE fltx4 SqrtSIMD(const fltx4 &a)  // sqrt(a)
{
  return _mm_sqrt_ps(a);
}

SOURCE_FORCEINLINE fltx4
ReciprocalSqrtEstSIMD(const fltx4 &a)  // 1/sqrt(a), more or less
{
  return _mm_rsqrt_ps(a);
}

/// uses newton iteration for higher precision results than
/// ReciprocalSqrtEstSIMD
SOURCE_FORCEINLINE fltx4 ReciprocalSqrtSIMD(const fltx4 &a)  // 1/sqrt(a)
{
  fltx4 guess = ReciprocalSqrtEstSIMD(a);
  // newton iteration for 1/sqrt(a) : y(n+1) = 1/2 (y(n)*(3-a*y(n)^2));
  guess =
      MulSIMD(guess, SubSIMD(Four_Threes, MulSIMD(a, MulSIMD(guess, guess))));
  guess = MulSIMD(Four_PointFives, guess);
  return guess;
}

SOURCE_FORCEINLINE fltx4 ReciprocalEstSIMD(const fltx4 &a)  // 1/a, more or less
{
  return _mm_rcp_ps(a);
}

/// 1/x for all 4 values. uses reciprocal approximation instruction plus newton
/// iteration. No error checking!
SOURCE_FORCEINLINE fltx4 ReciprocalSIMD(const fltx4 &a)  // 1/a
{
  fltx4 ret = ReciprocalEstSIMD(a);
  // newton iteration is: Y(n+1) = 2*Y(n)-a*Y(n)^2
  ret = SubSIMD(AddSIMD(ret, ret), MulSIMD(a, MulSIMD(ret, ret)));
  return ret;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
SOURCE_FORCEINLINE fltx4 ReciprocalSaturateSIMD(const fltx4 &a) {
  fltx4 zero_mask = CmpEqSIMD(a, Four_Zeros);
  fltx4 ret = OrSIMD(a, AndSIMD(Four_Epsilons, zero_mask));
  ret = ReciprocalSIMD(ret);
  return ret;
}

// CHRISG: is it worth doing integer bitfiddling for this?
// 2^x for all values (the antilog)
SOURCE_FORCEINLINE fltx4 ExpSIMD(const fltx4 &toPower) {
  fltx4 retval;
  SubFloat(retval, 0) = powf(2, SubFloat(toPower, 0));
  SubFloat(retval, 1) = powf(2, SubFloat(toPower, 1));
  SubFloat(retval, 2) = powf(2, SubFloat(toPower, 2));
  SubFloat(retval, 3) = powf(2, SubFloat(toPower, 3));

  return retval;
}

// Clamps the components of a vector to a specified minimum and maximum range.
SOURCE_FORCEINLINE fltx4 ClampVectorSIMD(FLTX4 in, FLTX4 min, FLTX4 max) {
  return MaxSIMD(min, MinSIMD(max, in));
}

SOURCE_FORCEINLINE void TransposeSIMD(fltx4 &x, fltx4 &y, fltx4 &z, fltx4 &w) {
  _MM_TRANSPOSE4_PS(x, y, z, w);
}

SOURCE_FORCEINLINE fltx4 FindLowestSIMD3(const fltx4 &a) {
  // a is [x,y,z,G] (where G is garbage)
  // rotate left by one
  fltx4 compareOne = RotateLeft(a);
  // compareOne is [y,z,G,x]
  fltx4 retval = MinSIMD(a, compareOne);
  // retVal is [std::min(x,y), ... ]
  compareOne = RotateLeft2(a);
  // compareOne is [z, G, x, y]
  retval = MinSIMD(retval, compareOne);
  // retVal = [ std::min(std::min(x,y),z)..]
  // splat the x component out to the whole vector and return
  return SplatXSIMD(retval);
}

SOURCE_FORCEINLINE fltx4 FindHighestSIMD3(const fltx4 &a) {
  // a is [x,y,z,G] (where G is garbage)
  // rotate left by one
  fltx4 compareOne = RotateLeft(a);
  // compareOne is [y,z,G,x]
  fltx4 retval = MaxSIMD(a, compareOne);
  // retVal is [std::max(x,y), ... ]
  compareOne = RotateLeft2(a);
  // compareOne is [z, G, x, y]
  retval = MaxSIMD(retval, compareOne);
  // retVal = [ std::max(std::max(x,y),z)..]
  // splat the x component out to the whole vector and return
  return SplatXSIMD(retval);
}

// ------------------------------------
// INTEGER SIMD OPERATIONS.
// ------------------------------------

// Load 4 aligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadAlignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return _mm_load_ps(reinterpret_cast<const f32 *>(pSIMD));
}

// Load 4 unaligned words into a SIMD register
SOURCE_FORCEINLINE i32x4 LoadUnalignedIntSIMD(const i32 *SOURCE_RESTRICT pSIMD) {
  return _mm_loadu_ps(reinterpret_cast<const f32 *>(pSIMD));
}

// save into four words, 16-byte aligned
SOURCE_FORCEINLINE void StoreAlignedIntSIMD(i32 *SOURCE_RESTRICT pSIMD, const fltx4 &a) {
  _mm_store_ps(reinterpret_cast<f32 *>(pSIMD), a);
}

SOURCE_FORCEINLINE void StoreAlignedIntSIMD(intx4 &pSIMD, const fltx4 &a) {
  _mm_store_ps(reinterpret_cast<f32 *>(pSIMD.Base()), a);
}

SOURCE_FORCEINLINE void StoreUnalignedIntSIMD(i32 *SOURCE_RESTRICT pSIMD, const fltx4 &a) {
  _mm_storeu_ps(reinterpret_cast<f32 *>(pSIMD), a);
}

// CHRISG: the conversion functions all seem to operate on m64's only...
// how do we make them work here?

// Take a fltx4 containing fixed-point uints and
// return them as single precision floats. No
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD(const u32x4 &vSrcA) {
  fltx4 retval = vSrcA;
  SubFloat(retval, 0) = ((f32)SubInt(retval, 0));
  SubFloat(retval, 1) = ((f32)SubInt(retval, 1));
  SubFloat(retval, 2) = ((f32)SubInt(retval, 2));
  SubFloat(retval, 3) = ((f32)SubInt(retval, 3));
  return retval;
}

// Take a fltx4 containing fixed-point sints and
// return them as single precision floats. No
// fixed point conversion is done.
SOURCE_FORCEINLINE fltx4 SignedIntConvertToFltSIMD(const i32x4 &vSrcA) {
  fltx4 retval;
  SubFloat(retval, 0) = ((f32)(reinterpret_cast<const i32 *>(&vSrcA)[0]));
  SubFloat(retval, 1) = ((f32)(reinterpret_cast<const i32 *>(&vSrcA)[1]));
  SubFloat(retval, 2) = ((f32)(reinterpret_cast<const i32 *>(&vSrcA)[2]));
  SubFloat(retval, 3) = ((f32)(reinterpret_cast<const i32 *>(&vSrcA)[3]));
  return retval;
}

/*
works on fltx4's as if they are four uints.
the first parameter contains the words to be shifted,
the second contains the amount to shift by AS INTS

for i = 0 to 3
shift = vSrcB_i*32:(i*32)+4
vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
*/
SOURCE_FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB) {
  i32x4 retval;
  SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
  SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
  SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
  SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);

  return retval;
}

// Fixed-point conversion and save as SIGNED INTS.
// pDest->x = Int (vSrc.x)
// note: some architectures have means of doing
// fixed point conversion when the fix depth is
// specified as an immediate.. but there is no way
// to guarantee an immediate as a parameter to function
// like this.
SOURCE_FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 *SOURCE_RESTRICT pDest,
                                        const fltx4 &vSrc) {
  __m64 bottom = _mm_cvttps_pi32(vSrc);
  __m64 top = _mm_cvttps_pi32(_mm_movehl_ps(vSrc, vSrc));

  *reinterpret_cast<__m64 *>(&(*pDest)[0]) = bottom;
  *reinterpret_cast<__m64 *>(&(*pDest)[2]) = top;

  _mm_empty();
}

#endif

/// class FourVectors stores 4 independent vectors for use in SIMD processing.
/// These vectors are stored in the format x x x x y y y y z z z z so that they
/// can be efficiently SIMD-accelerated.
class alignas(16) FourVectors {
 public:
  fltx4 x, y, z;

  SOURCE_FORCEINLINE void DuplicateVector(
      Vector const &v)  //< set all 4 vectors to the same vector value
  {
    x = ReplicateX4(v.x);
    y = ReplicateX4(v.y);
    z = ReplicateX4(v.z);
  }

  SOURCE_FORCEINLINE fltx4 const &operator[](int idx) const { return *((&x) + idx); }

  SOURCE_FORCEINLINE fltx4 &operator[](int idx) { return *((&x) + idx); }

  SOURCE_FORCEINLINE void operator+=(
      FourVectors const &b)  //< add 4 vectors to another 4 vectors
  {
    x = AddSIMD(x, b.x);
    y = AddSIMD(y, b.y);
    z = AddSIMD(z, b.z);
  }

  SOURCE_FORCEINLINE void operator-=(
      FourVectors const &b)  //< subtract 4 vectors from another 4
  {
    x = SubSIMD(x, b.x);
    y = SubSIMD(y, b.y);
    z = SubSIMD(z, b.z);
  }

  SOURCE_FORCEINLINE void operator*=(
      FourVectors const &b)  //< scale all four vectors per component scale
  {
    x = MulSIMD(x, b.x);
    y = MulSIMD(y, b.y);
    z = MulSIMD(z, b.z);
  }

  SOURCE_FORCEINLINE void operator*=(const fltx4 &scale)  //< scale
  {
    x = MulSIMD(x, scale);
    y = MulSIMD(y, scale);
    z = MulSIMD(z, scale);
  }

  SOURCE_FORCEINLINE void operator*=(f32 scale)  //< uniformly scale all 4 vectors
  {
    fltx4 scalepacked = ReplicateX4(scale);
    *this *= scalepacked;
  }

  SOURCE_FORCEINLINE fltx4 operator*(FourVectors const &b) const  //< 4 dot products
  {
    fltx4 dot = MulSIMD(x, b.x);
    dot = MaddSIMD(y, b.y, dot);
    dot = MaddSIMD(z, b.z, dot);
    return dot;
  }

  SOURCE_FORCEINLINE fltx4
  operator*(Vector const &b) const  //< dot product all 4 vectors with 1 vector
  {
    fltx4 dot = MulSIMD(x, ReplicateX4(b.x));
    dot = MaddSIMD(y, ReplicateX4(b.y), dot);
    dot = MaddSIMD(z, ReplicateX4(b.z), dot);
    return dot;
  }

  SOURCE_FORCEINLINE void VProduct(
      FourVectors const &b)  //< component by component mul
  {
    x = MulSIMD(x, b.x);
    y = MulSIMD(y, b.y);
    z = MulSIMD(z, b.z);
  }
  SOURCE_FORCEINLINE void MakeReciprocal(void)  //< (x,y,z)=(1/x,1/y,1/z)
  {
    x = ReciprocalSIMD(x);
    y = ReciprocalSIMD(y);
    z = ReciprocalSIMD(z);
  }

  SOURCE_FORCEINLINE void MakeReciprocalSaturate(
      void)  //< (x,y,z)=(1/x,1/y,1/z), 1/0=1.0e23
  {
    x = ReciprocalSaturateSIMD(x);
    y = ReciprocalSaturateSIMD(y);
    z = ReciprocalSaturateSIMD(z);
  }

  // Assume the given matrix is a rotation, and rotate these vectors by it.
  // If you have a long list of FourVectors structures that you all want
  // to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
  inline void RotateBy(const matrix3x4_t &matrix);

  /// You can use this to rotate a long array of FourVectors all by the same
  /// matrix. The first parameter is the head of the array. The second is the
  /// number of vectors to rotate. The third is the matrix.
  static void RotateManyBy(FourVectors *SOURCE_RESTRICT pVectors, u32 numVectors,
                           const matrix3x4_t &rotationMatrix);

  /// Assume the vectors are points, and transform them in place by the matrix.
  inline void TransformBy(const matrix3x4_t &matrix);

  /// You can use this to Transform a long array of FourVectors all by the same
  /// matrix. The first parameter is the head of the array. The second is the
  /// number of vectors to rotate. The third is the matrix. The fourth is the
  /// output buffer, which must not overlap the pVectors buffer. This is not
  /// an in-place transformation.
  static void TransformManyBy(FourVectors *SOURCE_RESTRICT pVectors, u32 numVectors,
                              const matrix3x4_t &rotationMatrix,
                              FourVectors *SOURCE_RESTRICT pOut);

  /// You can use this to Transform a long array of FourVectors all by the same
  /// matrix. The first parameter is the head of the array. The second is the
  /// number of vectors to rotate. The third is the matrix. The fourth is the
  /// output buffer, which must not overlap the pVectors buffer.
  /// This is an in-place transformation.
  static void TransformManyBy(FourVectors *SOURCE_RESTRICT pVectors, u32 numVectors,
                              const matrix3x4_t &rotationMatrix);

  // X(),Y(),Z() - get at the desired component of the i'th (0..3) vector.
  SOURCE_FORCEINLINE const f32 &X(int idx) const {
    // NOTE: if the output goes into a register, this causes a Load-Hit-Store
    // stall (don't mix fpu/vpu math!)
    return SubFloat((fltx4 &)x, idx);
  }

  SOURCE_FORCEINLINE const f32 &Y(int idx) const { return SubFloat((fltx4 &)y, idx); }

  SOURCE_FORCEINLINE const f32 &Z(int idx) const { return SubFloat((fltx4 &)z, idx); }

  SOURCE_FORCEINLINE f32 &X(int idx) { return SubFloat(x, idx); }

  SOURCE_FORCEINLINE f32 &Y(int idx) { return SubFloat(y, idx); }

  SOURCE_FORCEINLINE f32 &Z(int idx) { return SubFloat(z, idx); }

  SOURCE_FORCEINLINE Vector Vec(int idx) const  //< unpack one of the vectors
  {
    return Vector(X(idx), Y(idx), Z(idx));
  }

  FourVectors() {}

  FourVectors(FourVectors const &src) {
    x = src.x;
    y = src.y;
    z = src.z;
  }

  SOURCE_FORCEINLINE void operator=(FourVectors const &src) {
    x = src.x;
    y = src.y;
    z = src.z;
  }

  /// LoadAndSwizzle - load 4 Vectors into a FourVectors, performing transpose
  /// op
  SOURCE_FORCEINLINE void LoadAndSwizzle(Vector const &a, Vector const &b,
                                  Vector const &c, Vector const &d) {
    // TransposeSIMD has large sub-expressions that the compiler can't eliminate
    // on x360 use an unfolded implementation here
#if _X360
    fltx4 tx = LoadUnalignedSIMD(&a.x);
    fltx4 ty = LoadUnalignedSIMD(&b.x);
    fltx4 tz = LoadUnalignedSIMD(&c.x);
    fltx4 tw = LoadUnalignedSIMD(&d.x);
    fltx4 r0 = __vmrghw(tx, tz);
    fltx4 r1 = __vmrghw(ty, tw);
    fltx4 r2 = __vmrglw(tx, tz);
    fltx4 r3 = __vmrglw(ty, tw);

    x = __vmrghw(r0, r1);
    y = __vmrglw(r0, r1);
    z = __vmrghw(r2, r3);
#else
    x = LoadUnalignedSIMD(&(a.x));
    y = LoadUnalignedSIMD(&(b.x));
    z = LoadUnalignedSIMD(&(c.x));
    fltx4 w = LoadUnalignedSIMD(&(d.x));
    // now, matrix is:
    // x y z ?
    // x y z ?
    // x y z ?
    // x y z ?
    TransposeSIMD(x, y, z, w);
#endif
  }

  /// LoadAndSwizzleAligned - load 4 Vectors into a FourVectors, performing
  /// transpose op. all 4 vectors must be 128 bit boundary
  SOURCE_FORCEINLINE void LoadAndSwizzleAligned(const f32 *SOURCE_RESTRICT a,
                                         const f32 *SOURCE_RESTRICT b,
                                         const f32 *SOURCE_RESTRICT c,
                                         const f32 *SOURCE_RESTRICT d) {
#if _X360
    fltx4 tx = LoadAlignedSIMD(a);
    fltx4 ty = LoadAlignedSIMD(b);
    fltx4 tz = LoadAlignedSIMD(c);
    fltx4 tw = LoadAlignedSIMD(d);
    fltx4 r0 = __vmrghw(tx, tz);
    fltx4 r1 = __vmrghw(ty, tw);
    fltx4 r2 = __vmrglw(tx, tz);
    fltx4 r3 = __vmrglw(ty, tw);

    x = __vmrghw(r0, r1);
    y = __vmrglw(r0, r1);
    z = __vmrghw(r2, r3);
#else
    x = LoadAlignedSIMD(a);
    y = LoadAlignedSIMD(b);
    z = LoadAlignedSIMD(c);
    fltx4 w = LoadAlignedSIMD(d);
    // now, matrix is:
    // x y z ?
    // x y z ?
    // x y z ?
    // x y z ?
    TransposeSIMD(x, y, z, w);
#endif
  }

  SOURCE_FORCEINLINE void LoadAndSwizzleAligned(Vector const &a, Vector const &b,
                                         Vector const &c, Vector const &d) {
    LoadAndSwizzleAligned(&a.x, &b.x, &c.x, &d.x);
  }

  /// return the squared length of all 4 vectors
  SOURCE_FORCEINLINE fltx4 length2(void) const { return (*this) * (*this); }

  /// return the approximate length of all 4 vectors. uses the sqrt
  /// approximation instruction
  SOURCE_FORCEINLINE fltx4 length(void) const { return SqrtEstSIMD(length2()); }

  /// normalize all 4 vectors in place. not mega-accurate (uses reciprocal
  /// approximation instruction)
  SOURCE_FORCEINLINE void VectorNormalizeFast() {
    fltx4 mag_sq = (*this) * (*this);          // length^2
    (*this) *= ReciprocalSqrtEstSIMD(mag_sq);  // *(1.0/sqrt(length^2))
  }

  /// normalize all 4 vectors in place.
  SOURCE_FORCEINLINE void VectorNormalize() {
    fltx4 mag_sq = (*this) * (*this);       // length^2
    (*this) *= ReciprocalSqrtSIMD(mag_sq);  // *(1.0/sqrt(length^2))
  }

  /// construct a FourVectors from 4 separate Vectors
  SOURCE_FORCEINLINE FourVectors(Vector const &a, Vector const &b, Vector const &c,
                          Vector const &d) {
    LoadAndSwizzle(a, b, c, d);
  }

  /// construct a FourVectors from 4 separate Vectors
  SOURCE_FORCEINLINE FourVectors(VectorAligned const &a, VectorAligned const &b,
                          VectorAligned const &c, VectorAligned const &d) {
    LoadAndSwizzleAligned(a, b, c, d);
  }
};

/// form 4 cross products
inline FourVectors operator^(const FourVectors &a, const FourVectors &b) {
  FourVectors ret;
  ret.x = SubSIMD(MulSIMD(a.y, b.z), MulSIMD(a.z, b.y));
  ret.y = SubSIMD(MulSIMD(a.z, b.x), MulSIMD(a.x, b.z));
  ret.z = SubSIMD(MulSIMD(a.x, b.y), MulSIMD(a.y, b.x));
  return ret;
}

/// component-by-componentwise MAX operator
inline FourVectors maximum(const FourVectors &a, const FourVectors &b) {
  FourVectors ret;
  ret.x = MaxSIMD(a.x, b.x);
  ret.y = MaxSIMD(a.y, b.y);
  ret.z = MaxSIMD(a.z, b.z);
  return ret;
}

/// component-by-componentwise MIN operator
inline FourVectors minimum(const FourVectors &a, const FourVectors &b) {
  FourVectors ret;
  ret.x = MinSIMD(a.x, b.x);
  ret.y = MinSIMD(a.y, b.y);
  ret.z = MinSIMD(a.z, b.z);
  return ret;
}

/// calculate reflection vector. incident and normal dir assumed normalized
SOURCE_FORCEINLINE FourVectors VectorReflect(const FourVectors &incident,
                                      const FourVectors &normal) {
  FourVectors ret = incident;
  fltx4 iDotNx2 = incident * normal;
  iDotNx2 = AddSIMD(iDotNx2, iDotNx2);
  FourVectors nPart = normal;
  nPart *= iDotNx2;
  ret -= nPart;  // i-2(n*i)n
  return ret;
}

/// calculate slide vector. removes all components of a vector which are
/// perpendicular to a normal vector.
SOURCE_FORCEINLINE FourVectors VectorSlide(const FourVectors &incident,
                                    const FourVectors &normal) {
  FourVectors ret = incident;
  fltx4 iDotN = incident * normal;
  FourVectors nPart = normal;
  nPart *= iDotN;
  ret -= nPart;  // i-(n*i)n
  return ret;
}

// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::RotateBy(const matrix3x4_t &matrix) {
  // Splat out each of the entries in the matrix to a fltx4. Do this
  // in the order that we will need them, to hide latency. I'm
  // avoiding making an array of them, so that they'll remain in
  // registers.
  fltx4 matSplat00, matSplat01, matSplat02, matSplat10, matSplat11, matSplat12,
      matSplat20, matSplat21, matSplat22;

  {
    // Load the matrix into local vectors. Sadly, matrix3x4_ts are
    // often unaligned. The w components will be the tranpose row of
    // the matrix, but we don't really care about that.
    fltx4 matCol0 = LoadUnalignedSIMD(matrix[0]);
    fltx4 matCol1 = LoadUnalignedSIMD(matrix[1]);
    fltx4 matCol2 = LoadUnalignedSIMD(matrix[2]);

    matSplat00 = SplatXSIMD(matCol0);
    matSplat01 = SplatYSIMD(matCol0);
    matSplat02 = SplatZSIMD(matCol0);

    matSplat10 = SplatXSIMD(matCol1);
    matSplat11 = SplatYSIMD(matCol1);
    matSplat12 = SplatZSIMD(matCol1);

    matSplat20 = SplatXSIMD(matCol2);
    matSplat21 = SplatYSIMD(matCol2);
    matSplat22 = SplatZSIMD(matCol2);
  }

  // Trust in the compiler to schedule these operations correctly:
  fltx4 outX, outY, outZ;
  outX = AddSIMD(AddSIMD(MulSIMD(x, matSplat00), MulSIMD(y, matSplat01)),
                 MulSIMD(z, matSplat02));
  outY = AddSIMD(AddSIMD(MulSIMD(x, matSplat10), MulSIMD(y, matSplat11)),
                 MulSIMD(z, matSplat12));
  outZ = AddSIMD(AddSIMD(MulSIMD(x, matSplat20), MulSIMD(y, matSplat21)),
                 MulSIMD(z, matSplat22));

  x = outX;
  y = outY;
  z = outZ;
}

// Assume the given matrix is a rotation, and rotate these vectors by it.
// If you have a long list of FourVectors structures that you all want
// to rotate by the same matrix, use FourVectors::RotateManyBy() instead.
void FourVectors::TransformBy(const matrix3x4_t &matrix) {
  // Splat out each of the entries in the matrix to a fltx4. Do this
  // in the order that we will need them, to hide latency. I'm
  // avoiding making an array of them, so that they'll remain in
  // registers.
  fltx4 matSplat00, matSplat01, matSplat02, matSplat10, matSplat11, matSplat12,
      matSplat20, matSplat21, matSplat22;

  {
    // Load the matrix into local vectors. Sadly, matrix3x4_ts are
    // often unaligned. The w components will be the tranpose row of
    // the matrix, but we don't really care about that.
    fltx4 matCol0 = LoadUnalignedSIMD(matrix[0]);
    fltx4 matCol1 = LoadUnalignedSIMD(matrix[1]);
    fltx4 matCol2 = LoadUnalignedSIMD(matrix[2]);

    matSplat00 = SplatXSIMD(matCol0);
    matSplat01 = SplatYSIMD(matCol0);
    matSplat02 = SplatZSIMD(matCol0);

    matSplat10 = SplatXSIMD(matCol1);
    matSplat11 = SplatYSIMD(matCol1);
    matSplat12 = SplatZSIMD(matCol1);

    matSplat20 = SplatXSIMD(matCol2);
    matSplat21 = SplatYSIMD(matCol2);
    matSplat22 = SplatZSIMD(matCol2);
  }

  // Trust in the compiler to schedule these operations correctly:
  fltx4 outX, outY, outZ;

  outX = MaddSIMD(z, matSplat02,
                  AddSIMD(MulSIMD(x, matSplat00), MulSIMD(y, matSplat01)));
  outY = MaddSIMD(z, matSplat12,
                  AddSIMD(MulSIMD(x, matSplat10), MulSIMD(y, matSplat11)));
  outZ = MaddSIMD(z, matSplat22,
                  AddSIMD(MulSIMD(x, matSplat20), MulSIMD(y, matSplat21)));

  x = AddSIMD(outX, ReplicateX4(matrix[0][3]));
  y = AddSIMD(outY, ReplicateX4(matrix[1][3]));
  z = AddSIMD(outZ, ReplicateX4(matrix[2][3]));
}

/// quick, low quality perlin-style noise() function suitable for real time use.
/// return value is -1..1. Only reliable around +/- 1 million or so.
fltx4 NoiseSIMD(const fltx4 &x, const fltx4 &y, const fltx4 &z);
fltx4 NoiseSIMD(FourVectors const &v);

/// calculate the absolute value of a packed single
inline fltx4 fabs(const fltx4 &x) {
  return AndSIMD(x, LoadAlignedSIMD((f32 *)g_SIMD_clear_signmask));
}

/// negate all four components of a SIMD packed single
inline fltx4 fnegate(const fltx4 &x) {
  return XorSIMD(x, LoadAlignedSIMD((f32 *)g_SIMD_signmask));
}

fltx4 Pow_FixedPoint_Exponent_SIMD(const fltx4 &x, int exponent);

// PowSIMD - raise a SIMD register to a power.  This is analogous to the C pow()
// function, with some restictions: fractional exponents are only handled with 2
// bits of precision. Basically, fractions of 0,.25,.5, and .75 are handled.
// PowSIMD(x,.30) will be the same as PowSIMD(x,.25). negative and fractional
// powers are handled by the SIMD reciprocal and square root approximation
// instructions and so are not especially accurate ----Note that this routine
// does not raise numeric exceptions because it uses SIMD--- This routine is
// O(log2(exponent)).
inline fltx4 PowSIMD(const fltx4 &x, f32 exponent) {
  return Pow_FixedPoint_Exponent_SIMD(x, (int)(4.0 * exponent));
}

// random number generation - generate 4 random numbers quickly.

void SeedRandSIMD(u32 seed);  // seed the random # generator
fltx4 RandSIMD(int nContext = 0);  // return 4 numbers in the 0..1 range

// for multithreaded, you need to use these and use the argument form of
// RandSIMD:
int GetSIMDRandContext();
void ReleaseSIMDRandContext(int nContext);

SOURCE_FORCEINLINE fltx4 RandSignedSIMD(void)  // -1..1
{
  return SubSIMD(MulSIMD(Four_Twos, RandSIMD()), Four_Ones);
}

// SIMD versions of mathlib simplespline functions
// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
inline fltx4 SimpleSpline(const fltx4 &value) {
  // Arranged to avoid a data dependency between these two MULs:
  fltx4 valueDoubled = MulSIMD(value, Four_Twos);
  fltx4 valueSquared = MulSIMD(value, value);

  // Nice little ease-in, ease-out spline-like curve
  return SubSIMD(MulSIMD(Four_Threes, valueSquared),
                 MulSIMD(valueDoubled, valueSquared));
}

// remaps a value in [startInterval, startInterval+rangeInterval] from linear to
// spline using SimpleSpline
inline fltx4 SimpleSplineRemapValWithDeltas(const fltx4 &val, const fltx4 &A,
                                            const fltx4 & /*BMinusA*/,
                                            const fltx4 &OneOverBMinusA,
                                            const fltx4 &C,
                                            const fltx4 &DMinusC) {
  // 	if ( A == B )
  // 		return val >= B ? D : C;
  fltx4 cVal = MulSIMD(SubSIMD(val, A), OneOverBMinusA);
  return AddSIMD(C, MulSIMD(DMinusC, SimpleSpline(cVal)));
}

inline fltx4 SimpleSplineRemapValWithDeltasClamped(
    const fltx4 &val, const fltx4 &A, const fltx4 & /*BMinusA*/,
    const fltx4 &OneOverBMinusA, const fltx4 &C, const fltx4 &DMinusC) {
  // 	if ( A == B )
  // 		return val >= B ? D : C;
  fltx4 cVal = MulSIMD(SubSIMD(val, A), OneOverBMinusA);
  cVal = MinSIMD(Four_Ones, MaxSIMD(Four_Zeros, cVal));
  return AddSIMD(C, MulSIMD(DMinusC, SimpleSpline(cVal)));
}

SOURCE_FORCEINLINE fltx4 FracSIMD(const fltx4 &val) {
  fltx4 fl4Abs = fabs(val);
  fltx4 ival = SubSIMD(AddSIMD(fl4Abs, Four_2ToThe23s), Four_2ToThe23s);
  ival = MaskedAssign(CmpGtSIMD(ival, fl4Abs), SubSIMD(ival, Four_Ones), ival);
  return XorSIMD(SubSIMD(fl4Abs, ival),
                 XorSIMD(val, fl4Abs));  // restore sign bits
}

SOURCE_FORCEINLINE fltx4 Mod2SIMD(const fltx4 &val) {
  fltx4 fl4Abs = fabs(val);
  fltx4 ival = SubSIMD(AndSIMD(LoadAlignedSIMD((f32 *)g_SIMD_lsbmask),
                               AddSIMD(fl4Abs, Four_2ToThe23s)),
                       Four_2ToThe23s);
  ival = MaskedAssign(CmpGtSIMD(ival, fl4Abs), SubSIMD(ival, Four_Twos), ival);
  return XorSIMD(SubSIMD(fl4Abs, ival),
                 XorSIMD(val, fl4Abs));  // restore sign bits
}

SOURCE_FORCEINLINE fltx4 Mod2SIMDPositiveInput(const fltx4 &val) {
  fltx4 ival = SubSIMD(AndSIMD(LoadAlignedSIMD((f32 *)g_SIMD_lsbmask),
                               AddSIMD(val, Four_2ToThe23s)),
                       Four_2ToThe23s);
  ival = MaskedAssign(CmpGtSIMD(ival, val), SubSIMD(ival, Four_Twos), ival);
  return SubSIMD(val, ival);
}

// approximate sin of an angle, with -1..1 representing the whole sin wave
// period instead of -pi..pi. no range reduction is done - for values outside of
// 0..1 you won't like the results
SOURCE_FORCEINLINE fltx4 _SinEst01SIMD(const fltx4 &val) {
  // really rough approximation - x*(4-x*4) - a parabola. s(0) = 0, s(.5) = 1,
  // s(1)=0, smooth in-between. sufficient for simple oscillation.
  return MulSIMD(val, SubSIMD(Four_Fours, MulSIMD(val, Four_Fours)));
}

SOURCE_FORCEINLINE fltx4 _Sin01SIMD(const fltx4 &val) {
  // not a bad approximation : parabola always over-estimates. Squared parabola
  // always underestimates. So lets blend between them:  goodsin = badsin +
  // .225*( badsin^2-badsin)
  fltx4 fl4BadEst = MulSIMD(val, SubSIMD(Four_Fours, MulSIMD(val, Four_Fours)));
  return AddSIMD(MulSIMD(Four_Point225s,
                         SubSIMD(MulSIMD(fl4BadEst, fl4BadEst), fl4BadEst)),
                 fl4BadEst);
}

// full range useable implementations
SOURCE_FORCEINLINE fltx4 SinEst01SIMD(const fltx4 &val) {
  fltx4 fl4Abs = fabs(val);
  fltx4 fl4Reduced2 = Mod2SIMDPositiveInput(fl4Abs);
  fltx4 fl4OddMask = CmpGeSIMD(fl4Reduced2, Four_Ones);
  fltx4 fl4val = SubSIMD(fl4Reduced2, AndSIMD(Four_Ones, fl4OddMask));
  fltx4 fl4Sin = _SinEst01SIMD(fl4val);
  fl4Sin = XorSIMD(fl4Sin, AndSIMD(LoadAlignedSIMD((f32 *)g_SIMD_signmask),
                                   XorSIMD(val, fl4OddMask)));
  return fl4Sin;
}

SOURCE_FORCEINLINE fltx4 Sin01SIMD(const fltx4 &val) {
  fltx4 fl4Abs = fabs(val);
  fltx4 fl4Reduced2 = Mod2SIMDPositiveInput(fl4Abs);
  fltx4 fl4OddMask = CmpGeSIMD(fl4Reduced2, Four_Ones);
  fltx4 fl4val = SubSIMD(fl4Reduced2, AndSIMD(Four_Ones, fl4OddMask));
  fltx4 fl4Sin = _Sin01SIMD(fl4val);
  fl4Sin = XorSIMD(fl4Sin, AndSIMD(LoadAlignedSIMD((f32 *)g_SIMD_signmask),
                                   XorSIMD(val, fl4OddMask)));
  return fl4Sin;
}

// Schlick style Bias approximation see graphics gems 4 : bias(t,a)= t/(
// (1/a-2)*(1-t)+1)

SOURCE_FORCEINLINE fltx4 PreCalcBiasParameter(const fltx4 &bias_parameter) {
  // convert perlin-style-bias parameter to the value right for the
  // approximation
  return SubSIMD(ReciprocalSIMD(bias_parameter), Four_Twos);
}

SOURCE_FORCEINLINE fltx4 BiasSIMD(const fltx4 &val, const fltx4 &precalc_param) {
  // similar to bias function except pass precalced bias value from calling
  // PreCalcBiasParameter.

  //!!speed!! use reciprocal est?
  //!!speed!! could save one op by precalcing _2_ values
  return DivSIMD(
      val, AddSIMD(MulSIMD(precalc_param, SubSIMD(Four_Ones, val)), Four_Ones));
}


// Box/plane test
// NOTE: The w component of emins + emaxs must be 1 for this to work

SOURCE_FORCEINLINE int BoxOnPlaneSideSIMD(const fltx4 &emins, const fltx4 &emaxs,
                                   const cplane_t *p, f32 tolerance = 0.f) {
  fltx4 corners[2];
  fltx4 normal = LoadUnalignedSIMD(p->normal.Base());
  fltx4 dist = ReplicateX4(-p->dist);
  normal = SetWSIMD(normal, dist);
  fltx4 t4 = ReplicateX4(tolerance);
  fltx4 negt4 = ReplicateX4(-tolerance);
  fltx4 cmp = CmpGeSIMD(normal, Four_Zeros);
  corners[0] = MaskedAssign(cmp, emaxs, emins);
  corners[1] = MaskedAssign(cmp, emins, emaxs);
  fltx4 dot1 = Dot4SIMD(normal, corners[0]);
  fltx4 dot2 = Dot4SIMD(normal, corners[1]);
  cmp = CmpGeSIMD(dot1, t4);
  fltx4 cmp2 = CmpGtSIMD(negt4, dot2);
  fltx4 result = MaskedAssign(cmp, Four_Ones, Four_Zeros);
  fltx4 result2 = MaskedAssign(cmp2, Four_Twos, Four_Zeros);
  result = AddSIMD(result, result2);
  intx4 sides;
  ConvertStoreAsIntsSIMD(&sides, result);
  return sides[0];
}

#endif  // SOURCE_MATHLIB_SSEMATH_H_
