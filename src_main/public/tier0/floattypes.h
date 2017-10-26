// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_FLOATTYPES_H_
#define SOURCE_TIER0_FLOATTYPES_H_

#include <corecrt.h>
#include <sal.h>

using float32_t = float;
static_assert(sizeof(float32_t) == 4);

using float64_t = double;
static_assert(sizeof(float64_t) == 8);

using vec_t = float;

// In case this ever changes.
#define M_PI 3.14159265358979323846

// This assumes the ANSI/IEEE 754-1985 standard.
#ifdef __cplusplus

constexpr inline unsigned long &FloatBits(vec_t &f) {
  return *reinterpret_cast<unsigned long *>(&f);
}

constexpr inline unsigned long const &FloatBits(vec_t const &f) {
  return *reinterpret_cast<unsigned long const *>(&f);
}

constexpr inline vec_t BitsToFloat(unsigned long i) {
  return *reinterpret_cast<vec_t *>(&i);
}

constexpr inline bool IsFinite(vec_t f) {
  return ((FloatBits(f) & 0x7F800000) != 0x7F800000);
}

constexpr inline unsigned long FloatAbsBits(vec_t f) {
  return FloatBits(f) & 0x7FFFFFFF;  //-V112
}

constexpr inline float FloatMakeNegative(vec_t f) {
  return BitsToFloat(FloatBits(f) | 0x80000000);  //-V112
}

#ifdef _WIN32
// Just use prototype from cmath.
extern "C" {
_Check_return_ _CRT_JIT_INTRINSIC double __cdecl fabs(_In_ double _X);
}
// In win32 try to use the intrinsic fabs so the optimizer can do it's thing
// inline in the code.
#pragma intrinsic(fabs)

// Also, alias float make positive to use fabs, too
// NOTE:  Is there a perf issue with double<->float conversion?
inline float FloatMakePositive(vec_t f) { return static_cast<float>(fabs(f)); }
#else
inline float FloatMakePositive(vec_t f) {
  return BitsToFloat(FloatBits(f) & 0x7FFFFFFF);
}
#endif  // _WIN32

constexpr inline float FloatNegate(vec_t f) {
  return BitsToFloat(FloatBits(f) ^ 0x80000000);  //-V112
}

#define FLOAT32_NAN_BITS (unsigned long)0x7FC00000  // not a number!
#define FLOAT32_NAN BitsToFloat(FLOAT32_NAN_BITS)

#define VEC_T_NAN FLOAT32_NAN

#endif  // __cplusplus

#endif  // SOURCE_TIER0_FLOATTYPES_H_
