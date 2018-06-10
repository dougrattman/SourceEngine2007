// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "mathlib/bumpvects.h"

#include <cassert>

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"

#include "tier0/include/memdbgon.h"

// Z is coming out of the face.
void GetBumpNormals(const Vector& s_vec, const Vector& t_vec,
                    const Vector& flat_normal, const Vector& phong_normal,
                    Vector bump_normals[NUM_BUMP_VECTS]) {
  Vector normal_vec;
  // Are we left or right handed?
  CrossProduct(s_vec, t_vec, normal_vec);

  const bool is_left_handed{DotProduct(flat_normal, normal_vec) < 0.0f};

  // Build a basis for the face around the phong normal.
  matrix3x4_t smooth_basis;
  CrossProduct(phong_normal.Base(), s_vec.Base(), smooth_basis[1]);
  VectorNormalize(smooth_basis[1]);
  CrossProduct(smooth_basis[1], phong_normal.Base(), smooth_basis[0]);
  VectorNormalize(smooth_basis[0]);
  VectorCopy(phong_normal.Base(), smooth_basis[2]);

  if (is_left_handed) VectorNegate(smooth_basis[1]);

  static_assert(std::size(g_localBumpBasis) == NUM_BUMP_VECTS);

  // Move the g_localBumpBasis into world space to create bump_normals.
  for (usize i{0}; i < NUM_BUMP_VECTS; ++i) {
    VectorIRotate(g_localBumpBasis[i], smooth_basis, bump_normals[i]);
  }
}

#endif  // !_STATIC_LINKED || _SHARED_LIB