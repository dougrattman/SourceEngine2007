// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
#define BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_

#include "base/include/base_types.h"
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Build error from message |message| and error code |error_code|.
inline wstr BuildError(_In_ wstr message, _In_ u32 error_code) {
  message += L"\n\nPrecise error description: ";
  message += make_windows_errno_info(win32_to_windows_errno_code(error_code))
                 .description;
  return message;
}

// Show error box with |message|.
inline void ShowErrorBox(_In_ wstr message) {
  MessageBoxW(nullptr, message.c_str(), SOURCE_APP_NAME L" - Startup Error",
              MB_ICONERROR | MB_OK);
}

// Show error box with |message| by |error_code|.
inline u32 NotifyAboutError(_In_ wstr message,
                            _In_ u32 error_code = GetLastError()) {
  ShowErrorBox(BuildError(message, error_code));
  return error_code;
}
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
