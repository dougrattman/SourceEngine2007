// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "mathlib/halton.h"

HaltonSequenceGenerator_t::HaltonSequenceGenerator_t(int b) {
  base = b;
  fbase = (f32)b;
  seed = 1;
}

f32 HaltonSequenceGenerator_t::GetElement(int elem) {
  int tmpseed = seed;
  f32 ret = 0.0f, base_inv = 1.0f / fbase;
  while (tmpseed) {
    int dig = tmpseed % base;
    ret += ((f32)dig) * base_inv;
    base_inv /= fbase;
    tmpseed /= base;
  }
  return ret;
}
