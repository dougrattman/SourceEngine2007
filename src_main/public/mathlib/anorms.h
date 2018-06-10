// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_ANORMS_H_
#define SOURCE_MATHLIB_ANORMS_H_

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"

constexpr inline usize NUMVERTEXNORMALS{162};

// The angle between consecutive g_anorms[] vectors is ~14.55 degrees.
constexpr inline f64 VERTEXNORMAL_CONE_INNER_ANGLE{DEG2RAD(7.275)};

extern Vector g_anorms[NUMVERTEXNORMALS];

#endif  // SOURCE_MATHLIB_ANORMS_H_
