// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
#define SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_

#include "base/include/check.h"
#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

#include <winsock.h>
#include <type_traits>
#include "base/include/base_types.h"
#include "base/include/macros.h"

namespace source::windows {
// Winsock versions.
enum class WinsockVersion : WORD { Version2_2 = MAKEWORD(2, 2) };

// Scoped winsock initializer.
class ScopedWinsockInitializer {
 public:
  // Initializes winsock with version |version|.
  explicit ScopedWinsockInitializer(WinsockVersion version) noexcept  //-V730
      : version_{version}, wsa_data_{Initialize(this, version)} {}

  ~ScopedWinsockInitializer() {
    if (error_code() == S_OK) {
      const int errno_code{WSACleanup()};
      CHECK(errno_code == 0, WSAGetLastError());
    }
  }

  // Get winsock initialization error code.
  [[nodiscard]] windows_errno_code error_code() const noexcept {
    return error_code_;
  }

 private:
  // Current winsock version.
  const WinsockVersion version_;
  // Winosock initialization data.
  const WSAData wsa_data_;
  // Winsock initialization error code. SHOULD BE S_OK.
  windows_errno_code error_code_;

  // Initializes |me| with winsock version |version|.
  static WSAData Initialize(ScopedWinsockInitializer* me,
                            WinsockVersion version) noexcept {
    DCHECK(me != nullptr, EINVAL);

    static_assert(
        std::is_same_v<WORD, std::underlying_type_t<decltype(version)>>,
        "Winsock version should be WORD.");

    std::underlying_type_t<decltype(version)> the_version{
        static_cast<std::underlying_type_t<decltype(version)>>(version)};

    WSAData wsa_data;
    // WSAStartup returns win32 error code.
    windows_errno_code error_code{
        win32_to_windows_errno_code(WSAStartup(the_version, &wsa_data))};
    if (error_code == S_OK) {
      error_code = LowByte(wsa_data.wVersion) == LowByte(the_version) &&
                           HighByte(wsa_data.wVersion) == HighByte(the_version)
                       ? S_OK
                       : win32_to_windows_errno_code(WSAVERNOTSUPPORTED);
    }

    me->error_code_ = error_code;

    return wsa_data;
  }

  ScopedWinsockInitializer(const ScopedWinsockInitializer& s) = delete;
  ScopedWinsockInitializer& operator=(const ScopedWinsockInitializer& s) =
      delete;
};
}  // namespace source::windows

#endif  // SOURCE_BASE_WINDOWS_INCLUDE_SCOPED_WINSOCK_INITIALIZER_H_
