// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_TYPE_TRAITS_H_
#define BASE_INCLUDE_TYPE_TRAITS_H_

#include <type_traits>

namespace source::type_traits {
// Checks argument is pointer to function.
template <typename T>
struct is_function_pointer {
  // Is type a pointer to the function?
  static constexpr bool value =
      std::is_pointer_v<T> &&
      std::is_function_v<typename std::remove_pointer<T>::type>;
};

// Alias for is_function_pointer<T>::value.
template <typename T>
constexpr bool is_function_pointer_v = is_function_pointer<T>::value;

// Checks argument is char type (char, wchar_t, char16_t, char32_t).
template <typename T>
struct is_char {
  // Is type a char type?
  static constexpr bool value =
      std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
      std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;
};

// Alias for is_char<T>::value.
template <typename T>
constexpr bool is_char_v = is_char<T>::value;
}  // namespace source::type_traits

#endif  // BASE_INCLUDE_TYPE_TRAITS_H_
