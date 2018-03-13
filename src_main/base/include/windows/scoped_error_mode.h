// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
#define BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_

#include <cstdint>
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Scoped error mode.
class ScopedErrorMode {
 public:
  // Set error mode |error_mode| in scope.
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
}  // namespace source::windows

#endif  // BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
