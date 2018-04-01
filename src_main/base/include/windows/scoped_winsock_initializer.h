// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
#define SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_

#include "base/include/check.h"
#include "base/include/windows/windows_light.h"

#include <winsock.h>
#include <type_traits>
#include "base/include/base_types.h"

namespace source::windows {
// Winsock versions.
enum class WinsockVersion : WORD { Version2_2 = MAKEWORD(2, 2) };

// Scoped winsock initializer.
class ScopedWinsockInitializer {
 public:
  // Initializes winsock with version |version|.
  ScopedWinsockInitializer(WinsockVersion version) noexcept  //-V730
      : version_{version}, wsa_data_{Initialize(this, version)} {}

  ~ScopedWinsockInitializer() {
    if (error_code_ == NOERROR) {
      const int errno_code{WSACleanup()};
      CHECK(errno_code == 0, WSAGetLastError());
    }
  }

  // Get winsock initialization error code.
  i32 error_code() const noexcept { return error_code_; }

 private:
  // Current winsock version.
  const WinsockVersion version_;
  // Winosock initialization data.
  const WSAData wsa_data_;
  // Winsock initialization error code.
  int error_code_;

  // Initializes |me| with winsock version |version|.
  static WSAData Initialize(ScopedWinsockInitializer* me,
                            WinsockVersion version) noexcept {
    DCHECK(me != nullptr, EINVAL);

    WSAData wsa_data;
    static_assert(
        std::is_same_v<WORD, std::underlying_type<decltype(version)>::type>,
        "Winsock version should be WORD.");

    me->error_code_ = WSAStartup(
        static_cast<std::underlying_type<decltype(version)>::type>(version),
        &wsa_data);
    return wsa_data;
  }

  ScopedWinsockInitializer(const ScopedWinsockInitializer& s) = delete;
  ScopedWinsockInitializer& operator=(const ScopedWinsockInitializer& s) =
      delete;
};
}  // namespace source::windows

#endif  // SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
