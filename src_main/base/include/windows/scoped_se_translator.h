// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_

#include <eh.h>

#include "base/include/check.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Set a per-thread callback function to translate Win32 exceptions (C
// structured exceptions) into C++ typed exceptions for scope.  See
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/set-se-translator.
class ScopedSeTranslator {
 public:
  explicit ScopedSeTranslator(
      const _se_translator_function scoped_se_translator)
      : old_se_translator_{_set_se_translator(scoped_se_translator)},
        scoped_se_translator_{scoped_se_translator},
        thread_id_{GetCurrentThreadId()} {}

  ~ScopedSeTranslator() {
    const DWORD this_thread_id{GetCurrentThreadId()};
    // Se translator should be reset on the same thread as it was initialized.
    CHECK(this_thread_id == thread_id_, EINVAL);

    // No _get_se_translator :(, but we need to know old one.
    const auto current_se_translator = _set_se_translator(nullptr);
    // Restore old, or save current for scope.
    _set_se_translator(current_se_translator == scoped_se_translator_
                           ? old_se_translator_
                           : current_se_translator);
  }

 private:
  // Structured exceptions translators.
  const _se_translator_function old_se_translator_, scoped_se_translator_;
  const DWORD thread_id_;
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_
