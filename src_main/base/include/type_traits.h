// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_TYPE_TRAITS_H_
#define BASE_INCLUDE_TYPE_TRAITS_H_

#include <type_traits>

namespace type_traits {
// Checks argument is pointer to function.
template <typename T>
struct is_function_pointer {
  // Is type a pointer to the function?
  static constexpr bool value =
      std::is_pointer<T>::value &&
      std::is_function<typename std::remove_pointer<T>::type>::value;
};

// Alias for is_function_pointer<T>::value.
template <typename T>
constexpr bool is_function_pointer_v = is_function_pointer<T>::value;
}  // namespace type_traits

#endif  // BASE_INCLUDE_TYPE_TRAITS_H_
