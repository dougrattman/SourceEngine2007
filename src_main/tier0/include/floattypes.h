// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_FLOATTYPES_H_
#define SOURCE_TIER0_INCLUDE_FLOATTYPES_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#ifdef OS_WIN
#include <corecrt.h>
#include <sal.h>
#endif

// In case this ever changes.
#define M_PI 3.14159265358979323846

// This assumes the ANSI/IEEE 754-1985 standard.
#ifdef __cplusplus
constexpr inline u32 &FloatBits(f32 &f) { return *reinterpret_cast<u32 *>(&f); }

constexpr inline u32 const &FloatBits(f32 const &f) {
  return *reinterpret_cast<u32 const *>(&f);
}

constexpr inline f32 BitsToFloat(u32 i) { return *reinterpret_cast<f32 *>(&i); }

constexpr inline bool IsFinite(f32 f) {
  return ((FloatBits(f) & 0x7F800000) != 0x7F800000);
}

constexpr inline u32 FloatAbsBits(f32 f) {
  return FloatBits(f) & 0x7FFFFFFF;  //-V112
}

constexpr inline f32 FloatMakeNegative(f32 f) {
  return BitsToFloat(FloatBits(f) | 0x80000000);  //-V112
}

#ifdef OS_WIN
// Just use prototype from cmath.
extern "C" {
_Check_return_ _CRT_JIT_INTRINSIC f64 __cdecl fabs(_In_ f64 _X);
}
// In win32 try to use the intrinsic fabs so the optimizer can do it's thing
// inline in the code.
#pragma intrinsic(fabs)

// Also, alias f32 make positive to use fabs, too
// NOTE:  Is there a perf issue with f64<->f32 conversion?
inline f32 FloatMakePositive(f32 f) { return static_cast<f32>(fabs(f)); }
#else
inline f32 FloatMakePositive(f32 f) {
  return BitsToFloat(FloatBits(f) & 0x7FFFFFFF);
}
#endif  // OS_WIN

constexpr inline f32 FloatNegate(f32 f) {
  return BitsToFloat(FloatBits(f) ^ 0x80000000);  //-V112
}

#define FLOAT32_NAN_BITS (u32)0x7FC00000  // not a number!
#define FLOAT32_NAN BitsToFloat(FLOAT32_NAN_BITS)
#endif  // __cplusplus

#endif  // SOURCE_TIER0_INCLUDE_FLOATTYPES_H_
