// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_

#include "base/include/check.h"
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

#include <ObjBase.h>
#include <comip.h>
#include <type_traits>

namespace source::windows {
// Initializes COM at scope level.
class ScopedComInitializer {
 public:
  // Initializes COM with |coinit| flags for scope.
  explicit ScopedComInitializer(const COINIT coinit) noexcept
      : errno_code_{CoInitializeEx(nullptr, coinit)},
        thread_id_{GetCurrentThreadId()} {}
  // Free COM at the end of scope lifetime.
  ~ScopedComInitializer() {
    const DWORD this_thread_id{GetCurrentThreadId()};
    // COM should be freed on the same thread as it was initialized.
    CHECK(this_thread_id == thread_id_, CO_E_NOTINITIALIZED);

    if (succeeded(errno_code_)) CoUninitialize();
  }

  // Get COM initialization result.
  [[nodiscard]] windows_errno_code errno_code() const noexcept {
    return errno_code_;
  }

 private:
  const windows_errno_code errno_code_;
  const DWORD thread_id_;

  ScopedComInitializer(const ScopedComInitializer &s) = delete;
  ScopedComInitializer &operator=(ScopedComInitializer &s) = delete;
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_
