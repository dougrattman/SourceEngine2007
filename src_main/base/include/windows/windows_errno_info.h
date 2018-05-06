// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_WINDOWS_ERRNO_INFO_H_
#define BASE_INCLUDE_WINDOWS_WINDOWS_ERRNO_INFO_H_

#include <tchar.h>
#include <cstdio>  // sprintf_s

#include "base/include/windows/windows_light.h"

#include <comdef.h>  // _com_error

#include "base/include/check.h"
#include "base/include/compiler_specific.h"
#include "base/include/errno_info.h"

namespace source::windows {
// Windows error code.
using windows_errno_code = HRESULT;

// Generic test for success on any windows status value (non-negative numbers
// indicate success).
[[nodiscard]] constexpr inline bool succeeded(
    const windows_errno_code error_code) noexcept {
  return error_code >= 0;
}

// Generic test for failure on any windows status value (negative numbers
// indicate failure).
[[nodiscard]] constexpr inline bool failed(
    const windows_errno_code error_code) noexcept {
  return error_code < 0;
}

// Converts win32 code to Windows error code.
[[nodiscard]] constexpr inline windows_errno_code win32_to_windows_errno_code(
    const unsigned long win32_code) noexcept {
  return win32_code <= 0
             ? (windows_errno_code)win32_code
             : (windows_errno_code)((win32_code & 0x0000FFFF) |
                                    (FACILITY_WIN32 << 16) | 0x80000000);
}

// Windows errno info.
using windows_errno_info =
    source::errno_info<windows_errno_code, TCHAR, 512, succeeded>;

// Creates windows errno info from |errno_code|.
[[nodiscard]] inline windows_errno_info make_windows_errno_info(
    windows_errno_code errno_code) noexcept {
  windows_errno_info info = {errno_code};
  int sprintf_code;

  if (succeeded(errno_code)) {
    sprintf_code =
        _stprintf_s(info.description, _T("Ok (hr 0x%.8x)"), errno_code);
  } else {
    _com_error error{errno_code};
    const TCHAR* description{error.ErrorMessage()};

    sprintf_code = _stprintf_s(
        info.description, _T("%s (hr 0x%.8x)"),
        (description != nullptr ? description : _T("Unknown error")),
        errno_code);
  }

  CHECK(sprintf_code != -1, errno);
  return info;
}

// Last Windows errno code.
[[nodiscard]] inline windows_errno_code
windows_errno_code_last_error() noexcept {
  return win32_to_windows_errno_code(GetLastError());
}

// Last Windows errno info.
[[nodiscard]] inline windows_errno_info
windows_errno_info_last_error() noexcept {
  return make_windows_errno_info(windows_errno_code_last_error());
}

// Success Windows code info.
inline static const windows_errno_code windows_errno_code_ok{S_OK};

// Success Windows errno info.
inline static const windows_errno_info windows_errno_info_ok{
    make_windows_errno_info(windows_errno_code_ok)};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_WINDOWS_ERRNO_INFO_H_
