// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// halton.h - classes, etc for generating numbers using the Halton pseudo-random
// sequence.  See https://en.wikipedia.org/wiki/Halton_sequence.
//
// what this function is useful for is any sort of sampling/integration problem
// where you want to solve it by random sampling. Each call the NextValue()
// generates a random number between 0 and 1, in an unclumped manner, so that
// the space can be more or less evenly sampled with a minimum number of
// samples.
//
// It is NOT useful for generating random numbers dynamically, since the outputs
// aren't particularly random.
//
// To generate multidimensional sample values (points in a plane, etc), use two
// HaltonSequenceGenerator_t's, with different (primes) bases.

#ifndef SOURCE_MATHLIB_HALTON_H_
#define SOURCE_MATHLIB_HALTON_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/floattypes.h"

class HaltonSequenceGenerator_t {
 public:
  constexpr HaltonSequenceGenerator_t(i32 base)
      : seed_{1},
        base_{base},
        fbase_{(f32)base} {}  // < base MUST be prime, >= 2
  constexpr f32 GetElement(int elem) const {
    i32 tmpseed{seed_};
    f32 ret{0.0f}, base_inv{1.0f / fbase_};

    while (tmpseed) {
      const f32 dig{(f32)(tmpseed % base_)};

      ret += dig * base_inv;
      base_inv /= fbase_;
      tmpseed /= base_;
    }

    return ret;
  }

  constexpr inline f32 NextValue() { return GetElement(seed_++); }

 private:
  i32 seed_;
  i32 base_;
  f32 fbase_;  //< base as a f32
};

//< pseudo-random sphere sampling
class DirectionalSampler_t {
 public:
  constexpr DirectionalSampler_t() : zdot{2}, vrot{3} {}

  Vector NextValue() {
    // map from 0..1 to -1..1
    const f32 zvalue{2 * zdot.NextValue() - 1.0f};
    // now, generate a random rotation angle for x/y
    const f32 theta{2.0f * M_PI_F * vrot.NextValue()};
    const f32 phi{acos(zvalue)};
    const f32 sin_p{sin(phi)};
    return Vector{cos(theta) * sin_p, sin(theta) * sin_p, zvalue};
  }

 private:
  HaltonSequenceGenerator_t zdot, vrot;
};

#endif  // SOURCE_MATHLIB_HALTON_H_
