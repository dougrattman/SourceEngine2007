// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
#define BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_

#include "base/include/base_types.h"
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Build error from message |message| and error info |errno_info|.
inline wstr BuildError(_In_ wstr message,
                       _In_ const windows_errno_info& errno_info) {
  message += L"\n\nPrecise error description: ";
  message += errno_info.description;
  return message;
}

// Build error from message |message| and error code |errno_code|.
inline wstr BuildError(_In_ wstr message, _In_ windows_errno_code errno_code) {
  return BuildError(message, make_windows_errno_info(errno_code));
}

// Show error box with |message|.
inline void ShowErrorBox(_In_ wstr message) {
  MessageBoxW(nullptr, message.c_str(), SOURCE_APP_NAME L" - Error",
              MB_ICONERROR | MB_OK);
}

// Show error box with |message| by |errno_info|.
inline windows_errno_code NotifyAboutError(
    _In_ wstr message,
    _In_ windows_errno_info errno_info = windows_errno_info_last_error()) {
  ShowErrorBox(BuildError(message, errno_info));
  return errno_info.code;
}

// Show error box with |message| by |errno_code|.
inline windows_errno_code NotifyAboutError(
    _In_ wstr message,
    _In_ windows_errno_code errno_code = windows_errno_code_last_error()) {
  return NotifyAboutError(message, make_windows_errno_info(errno_code));
}
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
