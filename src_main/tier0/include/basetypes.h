// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_BASETYPES_H_
#define SOURCE_TIER0_INCLUDE_BASETYPES_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/commonmacros.h"
#include "tier0/include/floattypes.h"
#include "tier0/include/wchartypes.h"

#include "tier0/include/protected_things.h"
#include "tier0/include/valve_off.h"

using BOOL = i32;

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif  // FALSE

enum ThreeState_t {
  TRS_FALSE,
  TRS_TRUE,
  TRS_NONE,
};

// Align |value| by |alignment|.
template <typename T>
constexpr inline T AlignValue(T value, usize alignment) {
  return (T)(((usize)(value) + alignment - 1) & ~(alignment - 1));
}

// Limit |value| in [|min|..|max|] boundary.
template <typename T>
constexpr inline T clamp(T const &value, T const &min, T const &max) {
  if (value < min) return min;
  if (value > max) return max;

  return value;
}

// NOTE: This macro is the same as windows uses; so don't change the guts of it.
// Declare handle with |name|.
#define DECLARE_POINTER_HANDLE(name) \
  struct name##__ {                  \
    i32 unused;                      \
  };                                 \
  using name = struct name##__ *
// Forward declare handle with |name|.
#define FORWARD_DECLARE_HANDLE(name) using name = struct name##__ *

// TODO: Find a better home for this
#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// for platforms built with dynamic linking, the dll interface does not need
// spoofing.
#define PUBLISH_DLL_SUBSYSTEM()
#endif

// FIXME: why are these here? Hardly anyone actually needs them.
struct color24 {
  u8 r, g, b;
};

struct color32 {
  inline bool operator==(const color32 &other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  inline bool operator!=(const color32 &other) const {
    return !(*this == other);
  }

  u8 r, g, b, a;
};

struct colorVec {
  u32 r, g, b, a;
};

struct vrect_t {
  i32 x, y, width, height;
  vrect_t *pnext;
};

// Used for DrawDebugText.
struct Rect_t {
  i32 x, y;
  i32 width, height;
};

// Used by soundemittersystem + the game.
struct interval_t {
  f32 start;
  f32 range;
};

#include "tier0/include/valve_on.h"

#endif  // SOURCE_TIER0_INCLUDE_BASETYPES_H_
