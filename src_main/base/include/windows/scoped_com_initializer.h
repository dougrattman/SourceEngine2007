// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_

#include "base/include/windows/windows_light.h"

#include <ObjBase.h>
#include <comip.h>
#include <type_traits>

namespace source::windows {
// Initializes COM at scope level.
class ScopedComInitializer {
 public:
  // Initializes COM with |coinit| flags in scope.
  explicit ScopedComInitializer(const COINIT coinit) : hr_ { CoInitializeEx(nullptr, coinit) }
#ifndef NDEBUG
  , thread_id_ { GetCurrentThreadId() }
#endif
  {}

  ~ScopedComInitializer() {
#ifndef NDEBUG
    const DWORD this_thread_id{GetCurrentThreadId()};

    // COM should be freed on the same thread as it was initialized.
    assert(this_thread_id == thread_id_);
#endif

    if (SUCCEEDED(hr_)) {
      CoUninitialize();
    }
  }

  // Get COM initialization result.
  [[nodiscard]] HRESULT hr() const noexcept { return hr_; }

 private:
  const HRESULT hr_;
#ifndef NDEBUG
  const DWORD thread_id_;
#endif

  ScopedComInitializer(const ScopedComInitializer &s) = delete;
  ScopedComInitializer &operator=(ScopedComInitializer &s) = delete;
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_COM_INITIALIZER_H_
