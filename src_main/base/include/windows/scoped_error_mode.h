// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
#define BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_

#include <cstdint>
#include "base/include/windows/windows_light.h"

namespace source {
namespace windows {
// Scoped error mode.
class ScopedErrorMode {
 public:
  explicit ScopedErrorMode(const UINT error_mode)
      : old_error_mode_{SetErrorMode(GetErrorMode() | error_mode)},
        new_error_mode_{GetErrorMode()} {}

  ~ScopedErrorMode() {
    // If nobody changed error mode in our scope.
    if (GetErrorMode() == new_error_mode_) {
      SetErrorMode(old_error_mode_);
    }
  }

 private:
  const UINT old_error_mode_, new_error_mode_;

  ScopedErrorMode(const ScopedErrorMode& s) = delete;
  ScopedErrorMode& operator=(const ScopedErrorMode& s) = delete;
};
}  // namespace windows
}  // namespace source

#endif  // BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
