// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_BASETYPES_H_
#define SOURCE_TIER0_BASETYPES_H_

#include <cstdint>

#include "tier0/commonmacros.h"
#include "tier0/floattypes.h"
#include "tier0/wchartypes.h"

#include "tier0/protected_things.h"
#include "tier0/valve_off.h"

#if (defined(__x86_64__) || defined(_WIN64)) && !defined(X64BITS)
#define X64BITS
#endif

#ifdef _WIN32

#ifdef X64BITS
using size_t = unsigned __int64;
using ptrdiff_t = __int64;
using intptr_t = __int64;
using uintptr_t = unsigned __int64;
#else
using size_t = unsigned int;
using ptrdiff_t = int;
using intptr_t = int;
using uintptr_t = unsigned int;
#endif  // X64BITS

#else

#ifdef X64BITS
using size_t = unsigned long long;
using ptrdiff_t = long long;
using intptr_t = long long;
using uintptr_t = unsigned long long;
#else
using size_t = unsigned int;
using ptrdiff_t = int;
using intptr_t = int;
using uintptr_t = unsigned int;
#endif

#endif  // _WIN32

using BOOL = int;

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif  // FALSE

enum ThreeState_t {
  TRS_FALSE,
  TRS_TRUE,
  TRS_NONE,
};

template <typename T>
constexpr inline T AlignValue(T value, size_t alignment) {
  return (T)(((uintptr_t)(value) + alignment - 1) & ~(alignment - 1));
}

template <class T>
constexpr inline T clamp(T const &value, T const &min, T const &max) {
  if (value < min) return min;
  if (value > max) return max;

  return value;
}

// NOTE: This macro is the same as windows uses; so don't change the guts of it.
#define DECLARE_POINTER_HANDLE(name) \
  struct name##__ {                  \
    int unused;                      \
  };                                 \
  using name = struct name##__ *
#define FORWARD_DECLARE_HANDLE(name) using name = struct name##__ *

// TODO: Find a better home for this
#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// for platforms built with dynamic linking, the dll interface does not need
// spoofing.
#define PUBLISH_DLL_SUBSYSTEM()
#endif

// FIXME: why are these here? Hardly anyone actually needs them.
struct color24 {
  uint8_t r, g, b;
};

struct color32 {
  inline bool operator==(const color32 &other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  inline bool operator!=(const color32 &other) const {
    return !(*this == other);
  }

  uint8_t r, g, b, a;
};

struct colorVec {
  uint32_t r, g, b, a;
};

struct vrect_t {
  int32_t x, y, width, height;
  vrect_t *pnext;
};

// Used for DrawDebugText.
struct Rect_t {
  int32_t x, y;
  int32_t width, height;
};

// Used by soundemittersystem + the game.
struct interval_t {
  float32_t start;
  float32_t range;
};

#include "tier0/valve_on.h"

#endif  // SOURCE_TIER0_BASETYPES_H_
