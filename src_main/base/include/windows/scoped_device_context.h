// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_SCOPED_DEVICE_CONTEXT_H_
#define BASE_INCLUDE_WINDOWS_SCOPED_DEVICE_CONTEXT_H_

#include <eh.h>

#include "base/include/check.h"
#include "base/include/windows/windows_light.h"

namespace source::windows {
// Scoped windows device context.
class ScopedDeviceContext {
 public:
  explicit ScopedDeviceContext(HWND hwnd)
      : dc_{::GetDC(hwnd)}, hwnd_{hwnd}, thread_id_{GetCurrentThreadId()} {}

  ~ScopedDeviceContext() {
    const DWORD this_thread_id{GetCurrentThreadId()};
    // ReleaseDC must be called from the same thread that called GetDC.
    CHECK(this_thread_id == thread_id_, EINVAL);

    // The return value indicates whether the DC was released.  If the DC was
    // released, the return value is 1.  See
    // https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-releasedc
    CHECK(is_succeeded() && ::ReleaseDC(hwnd_, dc_) == 1, EINVAL);
  }

  bool is_succeeded() const { return dc_ != nullptr; }

  HBITMAP CreateCompatibleBitmap(_In_ int cx, _In_ int cy) const {
    return ::CreateCompatibleBitmap(dc_, cx, cy);
  }

  HDC CreateCompatibleDC() const { return ::CreateCompatibleDC(dc_); }

  int GetDeviceCaps(_In_ int index) const {
    return ::GetDeviceCaps(dc_, index);
  }

  bool FillRect(_In_ const RECT* lprc, _In_ HBRUSH hbr) const {
    return ::FillRect(dc_, lprc, hbr) != 0;
  }

  bool DPtoLP(_Inout_updates_(c) LPPOINT lppt, _In_ int c) const {
    return ::DPtoLP(dc_, lppt, c) != 0;
  }

  bool LPtoDP(_Inout_updates_(c) LPPOINT lppt, _In_ int c) const {
    return ::LPtoDP(dc_, lppt, c) != 0;
  }

  int SetMapMode(_In_ int iMode) const { return ::SetMapMode(dc_, iMode); }

  bool SetViewportOrgEx(_In_ int x, _In_ int y, _Out_opt_ LPPOINT lppt) const {
    return ::SetViewportOrgEx(dc_, x, y, lppt) != 0;
  }

 private:
  const HDC dc_;
  const HWND hwnd_;
  const DWORD thread_id_;

  ScopedDeviceContext(const ScopedDeviceContext& s) = delete;
  ScopedDeviceContext& operator=(const ScopedDeviceContext& s) = delete;
};
}  // namespace source::windows

#endif  // BASE_INCLUDE_WINDOWS_SCOPED_DEVICE_CONTEXT_H_
