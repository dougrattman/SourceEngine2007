// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: generates 4 random numbers in the range 0..1 quickly, using SIMD

#include <memory.h>
#include <cfloat>  // Needed for FLT_EPSILON
#include <cmath>
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

// see knuth volume 3 for insight.

class SIMDRandStreamContext {
  fltx4 rand_y_[55];
  fltx4 *rand_j_, *rand_k_;

 public:
  void Seed(u32 seed) {
    rand_j_ = rand_y_ + 23;
    rand_k_ = rand_y_ + 54;

    for (auto &rand_y : rand_y_) {
      for (int j = 0; j < 4; j++) {
        SubFloat(rand_y, j) = (seed >> 16) / 65536.0f;
        seed = (seed + 1) * 3141592621u;
      }
    }
  }

  inline fltx4 RandSIMD() {
    // ret= rand[k]+rand[j]
    fltx4 retval = AddSIMD(*rand_k_, *rand_j_);

    // if ( ret>=1.0) ret-=1.0
    fltx4 overflow_mask = CmpGeSIMD(retval, Four_Ones);
    retval = SubSIMD(retval, AndSIMD(Four_Ones, overflow_mask));

    *rand_k_ = retval;

    // update pointers w/ wrap-around
    if (--rand_j_ < rand_y_) rand_j_ = rand_y_ + 54;
    if (--rand_k_ < rand_y_) rand_k_ = rand_y_ + 54;

    return retval;
  }
};

constexpr inline usize MAX_SIMULTANEOUS_RANDOM_STREAMS{32};

static SIMDRandStreamContext
    s_SIMDRandContexts[MAX_SIMULTANEOUS_RANDOM_STREAMS];
static volatile int s_nRandContextsInUse[MAX_SIMULTANEOUS_RANDOM_STREAMS];

void SeedRandSIMD(u32 seed) {
  for (u32 i = 0; i < MAX_SIMULTANEOUS_RANDOM_STREAMS; i++)
    s_SIMDRandContexts[i].Seed(seed + i);
}

fltx4 RandSIMD(usize nContextIndex) {
  return s_SIMDRandContexts[nContextIndex].RandSIMD();
}

usize GetSIMDRandContext() {
  for (;;) {
    for (usize i = 0; i < std::size(s_SIMDRandContexts); i++) {
      if (!s_nRandContextsInUse[i])  // available?
      {
        // try to take it!
        if (ThreadInterlockedAssignIf(&(s_nRandContextsInUse[i]), 1, 0)) {
          return i;  // done!
        }
      }
    }
    Assert(0);  // why don't we have enough buffers?
    ThreadSleep();
  }
}

void ReleaseSIMDRandContext(usize nContext) {
  s_nRandContextsInUse[nContext] = 0;
}

fltx4 RandSIMD() { return s_SIMDRandContexts[0].RandSIMD(); }
