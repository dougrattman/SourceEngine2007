// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// This should contain ONLY general purpose macros.

#ifndef BASE_INCLUDE_MACROS_H_
#define BASE_INCLUDE_MACROS_H_

#include <cstring>      // For std::memcpy.
#include <iterator>     // For std::size
#include <type_traits>  // For std::is_pod.

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to Foo
// to a pointer to SuperclassOfFoo or casting a pointer to Foo to
// a const pointer to Foo).
// When you use implicit_cast, the compiler checks that the cast is safe.
// Such explicit implicit_casts are necessary in surprisingly many
// situations where C++ demands an exact type match instead of an
// argument type convertible to a target type.
//
// The From type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<ToType>(expr)
//
// implicit_cast would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.
template <typename To, typename From>
constexpr inline To implicit_cast(const From value) {
  return value;
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// bit_cast<Dest,Source> is a template function that implements the
// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
// very low-level functions like the protobuf library and fast math
// support.
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int32>(f);
//   // i = 0x40490fdb
//
// The classical address-casting method is:
//
//   // WRONG
//   float f = 3.14159265358979;            // WRONG
//   int i = * reinterpret_cast<int*>(&f);  // WRONG
//
// The address-casting method actually produces undefined behavior
// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
// section says: if an object in memory has one type, and a program
// accesses it with a different type, then the result is undefined
// behavior for most values of "different type".
//
// This is true for any cast syntax, either *(int*)&f or
// *reinterpret_cast<int*>(&f).  And it is particularly true for
// conversions between integral lvalues and floating-point lvalues.
//
// The purpose of 3.10 -15- is to allow optimizing compilers to assume
// that expressions with different types refer to different memory.  gcc
// 4.0.1 has an optimizer that takes advantage of this.  So a
// non-conforming program quietly produces wildly incorrect output.
//
// The problem is not the use of reinterpret_cast.  The problem is type
// punning: holding an object in memory of one type and reading its bits
// back using a different type.
//
// The C++ standard is more subtle and complex than this, but that
// is the basic idea.
//
// Anyways ...
//
// bit_cast<> calls memcpy() which is blessed by the standard,
// especially by the example in section 3.9 .  Also, of course,
// bit_cast<> wraps up the nasty logic in one place.
//
// Fortunately memcpy() is very fast.  In optimized mode, with a
// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
// code with the minimal amount of data movement.  On a 32-bit system,
// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
// compiles to two loads and two stores.
//
// I tested this code with gcc 2.95.3, gcc 4.0.1, icc 8.1, and msvc 7.1.
template <typename Dest, typename Source>
inline Dest bit_cast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "Verify sizes are equal.");
  static_assert(std::is_trivially_copyable_v<Source>,
                "Verify Source should be trivially copyable.");
  static_assert(std::is_trivially_copyable_v<Dest>,
                "Verify Dest be trivially copyable.");
  static_assert(std::is_default_constructible_v<Dest>,
                "Verify Dest should be default constructible.");

  Dest dest;
  std::memcpy(&dest, &source, sizeof(dest));
  return dest;
}

// Bitwise copy with type-checking. Returns reference to dest.
template <typename Dest, typename Source>
inline Dest& bitwise_copy(Dest& dest, const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "Verify sizes are equal.");
  static_assert(std::is_trivially_copyable_v<Source>,
                "Verify Source should be trivially copyable.");
  static_assert(std::is_trivially_copyable_v<Dest>,
                "Verify Dest be trivially copyable.");
  static_assert(std::is_default_constructible_v<Dest>,
                "Verify Dest should be default constructible.");

  std::memcpy(&dest, &source, sizeof(dest));
  return dest;
}

// Makes a 4-byte "packed ID" out of 4 chacters.
template <typename R = u32>
constexpr inline R MAKEID(ch d, ch c, ch b, ch a) {
  return (static_cast<R>(a) << 24) | (static_cast<R>(b) << 16) |
         (static_cast<R>(c) << 8) | (static_cast<R>(d));
}

template <typename T, typename Y>
constexpr inline auto SETBITS(T& bit_vector, Y bits) {
  return bit_vector |= bits;
}

template <typename T, typename Y>
constexpr inline auto CLEARBITS(T& bit_vector, Y bits) {
  return bit_vector &= ~bits;
}

template <typename T, typename Y>
constexpr inline auto FBitSet(const T &bit_vector, Y bit) {
  return bit_vector & bit;
}

template <typename T>
constexpr inline bool IsPowerOfTwo(T value) {
  return (value & (value - 1)) == 0;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
constexpr inline u8 LowByte(T value) {
  return static_cast<u8>(static_cast<usize>(value) & 0xFF);
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
constexpr inline u8 HighByte(T value) {
  return static_cast<u8>((static_cast<usize>(value) >> 8) & 0xFF);
}

// Pad a number so it lies on an N byte boundary. So SOURCE_PAD_NUMBER(0,4) is 0
// and SOURCE_PAD_NUMBER(1,4) is 4.
#define SOURCE_PAD_NUMBER(number, boundary) \
  (((number) + ((boundary)-1)) / (boundary)) * (boundary)

// Wraps the integer in quotes, allowing us to form constant strings with
// it.
#define SOURCE_CONST_INTEGER_AS_STRING(x) #x

//__LINE__ can only be converted to an actual number by going through this,
// otherwise the output is literally "__LINE__".
#define SOURCE_HACK_LINE_AS_STRING__(x) SOURCE_CONST_INTEGER_AS_STRING(x)

// Gives you the line number in constant string form.
#define SOURCE_LINE_AS_STRING SOURCE_HACK_LINE_AS_STRING__(__LINE__)

// NOTE: This macro is the same as Windows uses; so don't change the guts of it.
// Declare handle with name |name|.
#define SOURCE_DECLARE_POINTER_HANDLE(name) \
  struct name##__ {                         \
    i32 unused;                             \
  };                                        \
  using name = struct name##__*

// Forward declare handle with name |name|.
#define SOURCE_FORWARD_DECLARE_HANDLE(name) using name = struct name##__*

#define SOURCE_UID_PREFIX generated_id_
#define SOURCE_UID_CAT1(a, c) a##c
#define SOURCE_UID_CAT2(a, c) SOURCE_UID_CAT1(a, c)
#define SOURCE_EXPAND_CONCAT(a, c) SOURCE_UID_CAT1(a, c)

#ifdef COMPILER_MSVC
#define SOURCE_UNIQUE_ID SOURCE_UID_CAT2(SOURCE_UID_PREFIX, __COUNTER__)
#else  // !COMPILER_MSVC
#define SOURCE_UNIQUE_ID SOURCE_UID_CAT2(SOURCE_UID_PREFIX, __LINE__)
#endif  // COMPILER_MSVC

#endif  // BASE_INCLUDE_MACROS_H_
