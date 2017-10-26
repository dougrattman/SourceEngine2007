// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>

#include "base/include/base_types.h"
#include "base/include/windows/scoped_error_mode.h"
#include "base/include/windows/unique_module_ptr.h"
#include "base/include/windows/windows_light.h"

namespace {
// Purpose: Return the directory where this .exe is running from.
inline wstr GetBaseDirectory(_In_z_ const wch *module_path) {
  wstr base_dir{module_path};
  const usize forward_slash_pos{base_dir.rfind(L'\\')};
  if (wstr::npos != forward_slash_pos) {
    base_dir.resize(forward_slash_pos + 1);
  }

  const usize base_dir_length{base_dir.length()};
  if (base_dir_length > 0) {
    const usize last_char_index{base_dir_length - 1};
    const wch last_char{base_dir[last_char_index]};
    if (last_char == L'\\' || last_char == L'/') {
      base_dir.resize(last_char_index);
    }
  }

  return base_dir;
}

inline void ShowErrorBox(_In_z_ const wch *message) {
  MessageBoxW(nullptr, message, L"Awesome Launcher - Startup Error",
              MB_ICONERROR | MB_OK);
}

inline DWORD ShowErrorBoxAndGetErrorCode(
    _In_z_ const wch *message, _In_ DWORD error_code = GetLastError()) {
  ShowErrorBox(message);
  return error_code;
}

inline DWORD ShowNoLauncherErrorBoxAndGetErrorCode(
    _In_ DWORD error_code = GetLastError()) {
  wch *system_error;

  if (!FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
              FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPWSTR)&system_error, 0, nullptr)) {
    ShowErrorBox(
        L"Failed to get error description for the launcher DLL load error."
        L"Please, contact support.");
    return error_code;
  }

  wch error_msg[1024];
  _snwprintf_s(error_msg, ARRAYSIZE(error_msg),
               L"Please, contact support. Failed to load the launcher "
               L"DLL:\n\n%s\n",
               system_error);

  ShowErrorBox(error_msg);
  LocalFree(system_error);

  return error_code;
}

inline DWORD ShowNoLaucherEntryPointErrorBoxAndGetErrorCode(
    _In_ DWORD error_code = GetLastError()) {
  ShowErrorBox(
      L"Please, contact support. Failed to find the launcher DLL entry point.");
  return error_code;
}
}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR,
                      _In_ int cmd_show) {
  // Use features of at least Windows 10.
  if (!IsWindows10OrGreater()) {
    ShowErrorBox(
        L"Unfortunately, your operating system is not supported. "
        L"Please, use at least Windows 10 to play.");
    return ERROR_EXE_MACHINE_TYPE_MISMATCH;
  }

  const source::windows::ScopedErrorMode scoped_error_mode{
#ifndef _DEBUG
      // The system does not display the critical-error-handler message box.
      // Instead, the system sends the error to the calling process.
      // Enable only in Release to detect critical errors during debug builds.
      SEM_FAILCRITICALERRORS |
#endif
      // The system automatically fixes memory alignment faults and makes them
      // invisible to the application. It does this for the calling process and
      // any descendant processes.
      SEM_NOALIGNMENTFAULTEXCEPT |
      // The system does not display the Windows Error Reporting dialog.
      SEM_NOGPFAULTERRORBOX};

  // Enable heap corruption protection.
  if (HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                         0) == FALSE) {
    return ShowErrorBoxAndGetErrorCode(
        L"Failed to enable heap terminate-on-corruption feature. "
        L"Please, contact support.");
  }

  // Use the .EXE name to determine the root directory.
  wch module_name[MAX_PATH];
  if (!GetModuleFileNameW(instance, module_name, ARRAYSIZE(module_name))) {
    return ShowErrorBoxAndGetErrorCode(
        L"Failed calling GetModuleFileName. "
        L"Please, contact support.");
  }

  // Get the root directory the .exe is in.
  const wstr root_dir{GetBaseDirectory(module_name)};
  // Assemble the full path to our "launcher.dll".
  const wstr launcher_dll_path{root_dir + L"\\bin\\launcher.dll"};

  // STEAM OK ... file system not mounted yet.
  const auto launcher_module{
      source::windows::unique_module_ptr::from_load_library(
          launcher_dll_path, LOAD_WITH_ALTERED_SEARCH_PATH)};

  if (launcher_module) {
    using LauncherMainFunc = int (*)(HINSTANCE, int);
    const LauncherMainFunc main{
        launcher_module.get_address<LauncherMainFunc>("LauncherMain")};

    return main ? (*main)(instance, cmd_show)
                : ShowNoLaucherEntryPointErrorBoxAndGetErrorCode();
  }

  return ShowNoLauncherErrorBoxAndGetErrorCode();
}
