// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_ERROR_INFO_H_
#define BASE_INCLUDE_ERROR_INFO_H_

#include <cstdlib>
#include <type_traits>

#include "base/include/check.h"

namespace source {
template <typename Code>
using IsSuccessCode = bool(const Code code) noexcept;

template <typename Code, typename Char, size_t description_size,
          IsSuccessCode<Code> is_success_code>
struct errno_info {
  static_assert(std::is_integral_v<Code>, "Verify code is integral.");
  static_assert(std::is_integral_v<Char>, "Verify char is integral.");

  Code code;
  Char description[description_size];

  bool is_success() const noexcept { return is_success_code(code); }
};
}  // namespace source

#endif  // !BASE_INCLUDE_ERROR_INFO_H_
