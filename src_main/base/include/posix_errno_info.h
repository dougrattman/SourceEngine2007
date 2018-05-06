// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_POSIX_ERRNO_INFO_H_
#define BASE_INCLUDE_POSIX_ERRNO_INFO_H_

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <tuple>

#include "base/include/errno_info.h"
#include "base/include/macros.h"

namespace source {
// POSIX errno code.
using posix_errno_code = int;

// Success POSIX errno code.
inline static const posix_errno_code posix_errno_code_ok{0};

// Generic test for success on any posix errno code.
[[nodiscard]] constexpr inline bool succeeded(
    const posix_errno_code code) noexcept {
  return code == posix_errno_code_ok;
}

// Generic test for failure on any posix errno code.
[[nodiscard]] constexpr inline bool failed(
    const posix_errno_code code) noexcept {
  return !succeeded(code);
}

// POSIX errno info.
using posix_errno_info = errno_info<posix_errno_code, char, 92, succeeded>;

// Create POSIX errno info from POSIx errno code |errno_code|.
[[nodiscard]] inline posix_errno_info make_posix_errno_info(
    posix_errno_code errno_code) noexcept {
  posix_errno_info info = {errno_code};

  if (errno_code == posix_errno_code_ok) {
    strcpy_s(info.description, "Ok (0)");
    return info;
  }

  char description[std::size(info.description)];
  const int strerror_code{strerror_s(description, errno_code)};
  const int sprintf_code{sprintf_s(
      info.description, "%s (%d)",
      strerror_code == 0 ? description : "Unknown error", errno_code)};

  CHECK(sprintf_code != -1, errno);
  return info;
}

// Converts errno |errno_code| to posix_errno_code.
[[nodiscard]] constexpr inline posix_errno_code errno_to_posix_errno_code(
    errno_t errno_code) noexcept {
  return implicit_cast<posix_errno_code>(errno_code);
}

// Last POSIX errno info.
[[nodiscard]] inline posix_errno_info posix_errno_info_last_error() noexcept {
  return make_posix_errno_info(errno);
}

// Success POSIX errno info.
static const posix_errno_info posix_errno_info_ok{
    make_posix_errno_info(posix_errno_code_ok)};
}  // namespace source

#endif  // !BASE_INCLUDE_POSIX_ERRNO_INFO_H_
