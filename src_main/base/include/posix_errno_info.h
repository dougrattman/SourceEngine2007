// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_POSIX_ERRNO_INFO_H_
#define BASE_INCLUDE_POSIX_ERRNO_INFO_H_

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <tuple>

#include "base/include/errno_info.h"

#define EOK 0

namespace source {
// POSIX errno code.
using posix_errno_code = int;

// Check either POSIX errno code success or not |code|.
inline bool is_posix_errno_success(const posix_errno_code code) noexcept {
  return code == EOK;
};

// POSIX errno info.
using posix_errno_info =
    errno_info<posix_errno_code, char, 92, is_posix_errno_success>;

// Create POSIX errno info from POSIx errno code |errno_code|.
inline posix_errno_info make_posix_errno_info(
    posix_errno_code errno_code) noexcept {
  posix_errno_info info = {errno_code};

  if (errno_code == EOK) {
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

// Last POSIX errno info.
inline posix_errno_info posix_errno_info_last_error() noexcept {
  return make_posix_errno_info(errno);
}

// Success POSIX errno info.
static const posix_errno_info posix_errno_info_ok{make_posix_errno_info(EOK)};
}  // namespace source

#endif  // !BASE_INCLUDE_POSIX_ERRNO_INFO_H_
