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
constexpr const inline T AlignValue(const T value, const usize alignment) {
  return (T)(((usize)(value) + alignment - 1) & ~(alignment - 1));
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

// TODO: Find a better home for this.
#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// For platforms built with dynamic linking, the dll interface does not need
// spoofing.
#define PUBLISH_DLL_SUBSYSTEM()
#endif

// FIXME: Why are these here? Hardly anyone actually needs them.
struct color24 {
  color24() = default;
  constexpr color24(u8 r_, u8 g_, u8 b_) : r{r_}, g{g_}, b{b_} {}

  constexpr inline bool operator==(const color24 &other) const {
    return r == other.r && g == other.g && b == other.b;
  }
  constexpr inline bool operator!=(const color24 &other) const {
    return !(*this == other);
  }

  u8 r, g, b;
};

struct color32 : public color24 {
  color32() = default;
  constexpr color32(u8 r_, u8 g_, u8 b_, u8 a_) : color24{r_, g_, b_}, a{a_} {}

  constexpr inline bool operator==(const color32 &other) const {
    return color24::operator==(other) && a == other.a;
  }
  constexpr inline bool operator!=(const color32 &other) const {
    return !(*this == other);
  }

  u8 a;
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
