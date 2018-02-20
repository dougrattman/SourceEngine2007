// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_NOISE_H_
#define SOURCE_MATHLIB_NOISE_H_

#include <cmath>
#include "base/include/base_types.h"
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"

// The following code is the c-ification of Ken Perlin's new noise algorithm
// "JAVA REFERENCE IMPLEMENTATION OF IMPROVED NOISE - COPYRIGHT 2002 KEN PERLIN"
// as available here: http://mrl.nyu.edu/~perlin/noise/
// it generates a single octave of noise in the -1..1 range
// this should at some point probably replace SparseConvolutionNoise - jd
f32 ImprovedPerlinNoise(Vector const &pnt);

// get the noise value at a point. Output range is 0..1.
f32 SparseConvolutionNoise(Vector const &pnt);

// get the noise value at a point, passing a custom noise shaping function. The
// noise shaping function should map the domain 0..1 to 0..1.
f32 SparseConvolutionNoise(Vector const &pnt, f32 (*pNoiseShapeFunction)(f32));

// returns a 1/f noise. more octaves take longer
f32 FractalNoise(Vector const &pnt, int n_octaves);

// returns a abs(f)*1/f noise i.e. turbulence
f32 Turbulence(Vector const &pnt, int n_octaves);

#endif  // SOURCE_MATHLIB_NOISE_H_
