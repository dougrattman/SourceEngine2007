// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_BASE_TYPES_H_
#define BASE_INCLUDE_BASE_TYPES_H_

#include <cmath>
#include <cstddef>  // std::size_t, std::ptrdiff_t
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "build/include/build_config.h"

// Small and precise synonyms of fixed width standard types.
// These typedefs are not defined if no types with such characteristics exist,
// so define for your platform as needed.
using u8 = std::uint8_t;
using i8 = std::int8_t;

using u16 = std::uint16_t;
using i16 = std::int16_t;

using u32 = std::uint32_t;
using i32 = std::int32_t;

using u64 = std::uint64_t;
using i64 = std::int64_t;

// Unsigned pointer size type.
using usize = std::size_t;
// Signed pointer size type.
using isize = std::ptrdiff_t;

// These strict synonyms are for floating point types.
using f32 = float;
static_assert(
    sizeof(f32) == 4,
    "f32 should be 4 bytes length. "
    "Please, define 4 bytes f32 for your platform in base/base_types.h.");

using f64 = double;
static_assert(
    sizeof(f64) == 8,
    "f64 should be 8 bytes length. "
    "Please, define 8 bytes f64 for your platform in base/base_types.h.");

// Easy to use void type.
using v = void;

// Special type to ignore function call result.
using ignore_result = v;

using ch = char;

using wch = wchar_t;

// Basic char string type.
using str = std::string;

// Basic wide char string type.
using wstr = std::wstring;

template <typename T>
using vec = std::vector<T, std::allocator<T>>;

// Define error code.
using err = int;

template <typename T>
using Result = std::tuple<T, err>;

#endif  // BASE_INCLUDE_BASE_TYPES_H_
