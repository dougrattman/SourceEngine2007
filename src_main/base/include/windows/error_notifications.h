// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
#define BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_

#include <tchar.h>
#include "base/include/base_types.h"
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

#ifdef _UNICODE
#define tstr wstr
#else
#define tstr str
#endif

namespace source::windows {
// Build error from message |message| and error info |errno_info|.
inline tstr BuildError(_In_ tstr message,
                       _In_ const windows_errno_info& errno_info) {
  message += TEXT("\n\nPrecise error description: ");
  message += errno_info.description;
  return message;
}

// Build error from message |message| and error code |errno_code|.
inline tstr BuildError(_In_ tstr message, _In_ windows_errno_code errno_code) {
  return BuildError(message, make_windows_errno_info(errno_code));
}

// Show error box with |message|.
inline void ShowErrorBox(_In_ tstr message) {
  MessageBox(nullptr, message.c_str(), SOURCE_APP_NAME TEXT(" - Error"),
             MB_ICONERROR | MB_OK);
}

// Show error box with |message| by |errno_info|.
inline windows_errno_code NotifyAboutError(
    _In_ tstr message,
    _In_ windows_errno_info errno_info = windows_errno_info_last_error()) {
  ShowErrorBox(BuildError(message, errno_info));
  return errno_info.code;
}

// Show error box with |message| by |errno_code|.
inline windows_errno_code NotifyAboutError(
    _In_ tstr message,
    _In_ windows_errno_code errno_code = windows_errno_code_last_error()) {
  return NotifyAboutError(message, make_windows_errno_info(errno_code));
}
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_ERROR_NOTIFICATIONS_H_
