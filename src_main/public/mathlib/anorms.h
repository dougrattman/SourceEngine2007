// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_ANORMS_H_
#define SOURCE_MATHLIB_ANORMS_H_

#include "mathlib/vector.h"

#define NUMVERTEXNORMALS 162

// the angle between consecutive g_anorms[] vectors is ~14.55 degrees
#define VERTEXNORMAL_CONE_INNER_ANGLE DEG2RAD(7.275)

extern Vector g_anorms[NUMVERTEXNORMALS];

#endif  // SOURCE_MATHLIB_ANORMS_H_
