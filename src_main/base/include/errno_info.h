// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_ERROR_INFO_H_
#define BASE_INCLUDE_ERROR_INFO_H_

#include <cstdlib>
#include <type_traits>

#include "base/include/check.h"
#include "base/include/type_traits.h"

namespace source {
// Checks either |code| is success or not.
template <typename Code>
using IsSuccessCode = bool(const Code code) noexcept;

// Info about error.
template <typename Code, typename Char, size_t description_size,
          IsSuccessCode<Code> is_success_code>
struct errno_info {
  static_assert(std::is_integral_v<Code>, "Verify code is integral.");
  static_assert(source::type_traits::is_char_v<Char>,
                "Verify char is character type.");

  // Error code.
  Code code;
  // Error description.
  Char description[description_size];

  // Checks either |code| success or not.
  bool is_success() const noexcept { return is_success_code(code); }
};
}  // namespace source

#endif  // !BASE_INCLUDE_ERROR_INFO_H_
