// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_BUMPVECTS_H_
#define SOURCE_MATHLIB_BUMPVECTS_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "mathlib/vector.h"

constexpr inline f64 OO_SQRT_2{0.70710676908493042f};
constexpr inline f64 OO_SQRT_3{0.57735025882720947f};
constexpr inline f64 OO_SQRT_6{0.40824821591377258f};
// sqrt(2/3)
constexpr inline f64 OO_SQRT_2_OVER_3{0.81649661064147949f};

constexpr inline usize NUM_BUMP_VECTS{3};

const TableVector g_localBumpBasis[NUM_BUMP_VECTS] = {
    {OO_SQRT_2_OVER_3, 0.0f, OO_SQRT_3},
    {-OO_SQRT_6, OO_SQRT_2, OO_SQRT_3},
    {-OO_SQRT_6, -OO_SQRT_2, OO_SQRT_3}};

extern void GetBumpNormals(const Vector& s_vec, const Vector& t_vec,
                           const Vector& flat_normal,
                           const Vector& phong_normal,
                           Vector bump_normals[NUM_BUMP_VECTS]);

#endif  // SOURCE_MATHLIB_BUMPVECTS_H_
