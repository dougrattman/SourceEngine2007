// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
#define SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_

#include "base/include/windows/windows_light.h"

#include <winsock.h>
#include <cassert>
#include <type_traits>

namespace source::windows {
// Winsock versions.
enum class WinsockVersion : WORD { Version2_2 = MAKEWORD(2, 2) };

// Scoped winsock initializer.
class ScopedWinsockInitializer {
 public:
  ScopedWinsockInitializer(WinsockVersion version)  //-V730
      : version_{version}, wsa_data_{Initialize(this, version)} {}

  DWORD error_code() const { return error_code_; }

  ~ScopedWinsockInitializer() {
    if (error_code_ == ERROR_SUCCESS) {
      const int return_code = ::WSACleanup();
      (void)return_code;
      assert(return_code == ERROR_SUCCESS);
    }
  }

 private:
  const WinsockVersion version_;
  const WSAData wsa_data_;
  int error_code_;

  static WSAData Initialize(ScopedWinsockInitializer* me,
                            WinsockVersion version) {
    WSAData wsa_data;
    static_assert(
        std::is_same<WORD,
                     std::underlying_type<decltype(version)>::type>::value,
        "Winsock version should be WORD.");

    me->error_code_ = ::WSAStartup(static_cast<WORD>(version), &wsa_data);
    return wsa_data;
  }

  ScopedWinsockInitializer(const ScopedWinsockInitializer& s) = delete;
  ScopedWinsockInitializer& operator=(const ScopedWinsockInitializer& s) =
      delete;
};
}  // namespace source::windows

#endif  // SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
