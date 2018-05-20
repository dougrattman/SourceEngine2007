// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
#define BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_

#include "base/include/base_types.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Scoped error mode.
class ScopedErrorMode {
 public:
  // Set error mode to |error_mode| for scope.
  explicit ScopedErrorMode(const u32 error_mode) noexcept
      : old_error_mode_{SetErrorMode(GetErrorMode() | error_mode)},
        scoped_error_mode_{GetErrorMode()} {}

  ~ScopedErrorMode() {
    // If nobody changed error mode in our scope.
    if (GetErrorMode() == scoped_error_mode_) {
      SetErrorMode(old_error_mode_);
    }
  }

 private:
  // Old and scope error modes.
  const u32 old_error_mode_, scoped_error_mode_;

  ScopedErrorMode(const ScopedErrorMode& s) = delete;
  ScopedErrorMode& operator=(const ScopedErrorMode& s) = delete;
};
}  // namespace source::windows

#endif  // BASE_WINDOWS_INCLUDE_SCOPED_ERROR_MODE_H_
