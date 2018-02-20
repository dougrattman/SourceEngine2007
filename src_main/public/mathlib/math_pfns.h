// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_MATH_PFNS_H_
#define SOURCE_MATHLIB_MATH_PFNS_H_

#include "base/include/base_types.h"

// These globals are initialized by mathlib and redirected based on available
// fpu features
extern f32 (*pfSqrt)(f32 x);
extern f32 (*pfRSqrt)(f32 x);
extern f32 (*pfRSqrtFast)(f32 x);
extern void (*pfFastSinCos)(f32 x, f32 *s, f32 *c);
extern f32 (*pfFastCos)(f32 x);

// The following are not declared as macros because they are often used in
// limiting situations, and sometimes the compiler simply refuses to inline them
// for some reason
#define FastSqrt(x) (*pfSqrt)(x)
#define FastRSqrt(x) (*pfRSqrt)(x)
#define FastRSqrtFast(x) (*pfRSqrtFast)(x)
#define FastSinCos(x, s, c) (*pfFastSinCos)(x, s, c)
#define FastCos(x) (*pfFastCos)(x)

#endif  // SOURCE_MATHLIB_MATH_PFNS_H_
