// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_WINDOW_MESSAGE_HANDLERS_H_
#define BASE_INCLUDE_WINDOWS_WINDOW_MESSAGE_HANDLERS_H_

#include "base/include/windows/windows_light.h"

#include <windowsx.h>

// Useful macros to switch/case window message dispatching.
#define SOURCE_HANDLE_WINDOW_MSG(hwnd, message, wParam, lParam, fn) \
  case (message):                                                   \
    return HANDLE_##message((hwnd), (wParam), (lParam), (fn))

#undef HANDLE_WM_SYSCOMMAND
/* LRESULT Cls_OnSysCommand(HWND hwnd, UINT cmd, int x, int y) */
#define HANDLE_WM_SYSCOMMAND(hwnd, wParam, lParam, fn)      \
  ((fn)((hwnd), (unsigned int)(wParam), (int)(short)LOWORD(lParam), \
        (int)(short)HIWORD(lParam)))

#undef FORWARD_WM_SYSCOMMAND
#define FORWARD_WM_SYSCOMMAND(hwnd, cmd, x, y, fn)          \
  (LRESULT)(fn)((hwnd), WM_SYSCOMMAND, (WPARAM)(UINT)(cmd), \
                MAKELPARAM((x), (y)))

#undef HANDLE_WM_COPYDATA
/* BOOL Cls_OnCopyData(HWND hwnd, COPYDATASTRUCT* copy_data_struct) */
#define HANDLE_WM_COPYDATA(hwnd, wParam, lParam, fn) \
  ((fn)((hwnd), (COPYDATASTRUCT *)(lParam))) ? TRUE : FALSE

#undef FORWARD_WM_COPYDATA
#define FORWARD_WM_COPYDATA(hwnd, copy_data_struct, fn)        \
  (BOOL)(UINT)(DWORD)(fn)((hwnd), WM_COPYDATA, (WPARAM)(hwnd), \
                          (LPARAM)(LONG_PTR)(COPYDATASTRUCT *)(lParam))

/* LRESULT Cls_OnImeNotify(HWND hwnd, int ime_command, LPARAM command_data) */
#define HANDLE_WM_IME_NOTIFY(hwnd, wParam, lParam, fn) \
  ((fn)((hwnd), (int)(wParam), (lParam)))
#define FORWARD_WM_IME_NOTIFY(hwnd, ime_command, command_data) \
  (LRESULT)(fn)((hwnd), WM_IME_NOTIFY, (WPARAM)(int)(wParam),  \
                (LPARAM)(command_data))

#endif  // BASE_INCLUDE_WINDOWS_WINDOW_MESSAGE_HANDLERS_H_
