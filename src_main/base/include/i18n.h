// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_I18N_H_
#define BASE_INCLUDE_I18N_H_

#include <cstddef>
#include <string>
#include <utility>

#include "base/include/base_types.h"
#include "build/include/build_config.h"
#ifdef OS_WIN
#include "base/include/windows/windows_lite.h"
#endif

namespace source::i18n {
#ifdef OS_WIN
// Localizes message |message_id| for the application instance |instance|.
[[nodiscard]] wstr Localize(const HINSTANCE instance, const UINT message_id) {
  constexpr usize kMaxMessageSize{128};

  wstr message;
  message.resize(kMaxMessageSize);

  // From https://msdn.microsoft.com/en-us/library/ms647486(v=vs.85).aspx:
  // "If the function succeeds, the return value is the number of characters
  // copied into the buffer, not including the terminating null character, or
  // zero if the string resource does not exist.  To get extended error
  // information, call GetLastError."
  const int chars_count = LoadStringW(instance, message_id, &message[0],
                                      static_cast<int>(message.capacity()));
  const bool has_load_message_error =
      chars_count == 0 && GetLastError() != NOERROR;

  // Terminate the application when no message found.  Force to localize
  // application correctly.
  if (has_load_message_error) abort();

  return message;
}
#endif
}  // namespace source::i18n

#endif  // BASE_INCLUDE_I18N_H_
