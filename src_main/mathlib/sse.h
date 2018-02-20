// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_SSE_H_
#define SOURCE_MATHLIB_SSE_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/calling_conventions.h"

#ifndef ARCH_CPU_X86_64

f32 _SSE_Sqrt(f32 x);
f32 _SSE_RSqrtAccurate(f32 a);
f32 _SSE_RSqrtFast(f32 x);
f32 FASTCALL _SSE_VectorNormalize(Vector& vec);
void FASTCALL _SSE_VectorNormalizeFast(Vector& vec);
f32 _SSE_InvRSquared(const f32* v);
void _SSE_SinCos(f32 x, f32* s, f32* c);
f32 _SSE_cos(f32 x);
void _SSE2_SinCos(f32 x, f32* s, f32* c);
f32 _SSE2_cos(f32 x);
void VectorTransformSSE(const f32* in1, const matrix3x4_t& in2, f32* out1);
void VectorRotateSSE(const f32* in1, const matrix3x4_t& in2, f32* out1);

#endif  // ARCH_CPU_X86_64

#endif  // SOURCE_MATHLIB_SSE_H_
