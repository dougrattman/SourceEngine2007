// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_

#include <eh.h>
#include <cassert>

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
#ifndef NDEBUG

        thread_id_ {
    GetCurrentThreadId()
  }
#endif
  {}

  ~ScopedSeTranslator() {
#ifndef NDEBUG
    const u32 this_thread_id{GetCurrentThreadId()};

    // Se translator should be reset on the same thread as it was initialized.
    assert(this_thread_id == thread_id_);
#endif

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
#ifndef NDEBUG
  const u32 thread_id_;
#endif
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_SE_TRANSLATOR_H_
