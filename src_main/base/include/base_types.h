// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Here are small and easy to read types used everywhere.

#ifndef BASE_INCLUDE_BASE_TYPES_H_
#define BASE_INCLUDE_BASE_TYPES_H_

#include <cstddef>  // std::size_t, std::ptrdiff_t.
#include <cstdint>  // precise bit int types.
#include <string>
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
static_assert(sizeof(f32) == 4,
              "f32 should be 4 bytes length. "
              "Please, define 4 bytes float for your platform in "
              "base/include/base_types.h.");

using f64 = double;
static_assert(sizeof(f64) == 8,
              "f64 should be 8 bytes length. "
              "Please, define 8 bytes double for your platform in "
              "base/include/base_types.h.");

// Easy to use void type.
using v = void;

// Short char alias.
using ch = char;
// Short char16_t alias.
using ch16 = char16_t;
// Short char32_t alias.
using ch32 = char32_t;
// Short wchar_t alias.
using wch = wchar_t;

// Basic char string type.
using str = std::string;
// Basic wide char string type.
using wstr = std::wstring;

// Nice vector alias.
template <typename T>
using vec = std::vector<T, std::allocator<T>>;

#endif  // BASE_INCLUDE_BASE_TYPES_H_
