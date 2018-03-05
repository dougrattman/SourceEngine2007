// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>

#include "base/include/base_types.h"
#include "base/include/windows/scoped_error_mode.h"
#include "base/include/windows/unique_module_ptr.h"
#include "base/include/windows/windows_light.h"

namespace {
// Gets the .exe directory from |exe_path|.
inline wstr GetDirectoryFromExePath(_In_z_ const wch *exe_path) {
  wstr base_dir{exe_path};
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

// Get system error for |error_code|.
wstr GetSystemError(_In_ u32 error_code) {
  wch *system_error;
  if (!FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
              FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (wch *)&system_error, 0, nullptr)) {
    return L"N/A";
  }

  wstr error_message{system_error};
  LocalFree(system_error);

  return error_message;
}

// Show error box with |message|.
inline void ShowErrorBox(_In_z_ const wch *message) {
  MessageBoxW(nullptr, message, L"Awesome Launcher - Startup Error",
              MB_ICONERROR | MB_OK);
}

// Show error box with |message| by |error_code|.
inline u32 NotifyAboutError(_In_z_ const wch *message,
                            _In_ u32 error_code = GetLastError()) {
  wstr error_message{message};
  error_message += L"\nSystem error: ";
  error_message += GetSystemError(error_code);

  ShowErrorBox(error_message.c_str());

  return error_code;
}

// Show no launcher error box with message by code.
inline u32 NotifyAboutNoLauncherError(_In_ u32 error_code) {
  wstr error_message{
      L"Please, contact support. Failed to load the bin/launcher.dll:"};
  error_message += L"\nSystem error: ";
  error_message += GetSystemError(error_code);

  ShowErrorBox(error_message.c_str());

  return error_code;
}

// Show no launcher entry point error box with message by code.
inline u32 NotifyAboutNoEntryPointError(_In_ u32 error_code) {
  wstr error_message{
      L"Please, contact support. Failed to find the bin/launcher.dll entry "
      L"point:"};
  error_message += L"\nSystem error: ";
  error_message += GetSystemError(error_code);

  ShowErrorBox(error_message.c_str());

  return error_code;
}
}  // namespace

// Windows game entry point.
int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE,
                      _In_ wchar_t *, _In_ int cmd_show) {
  // Game uses features of Windows 10.
  if (!IsWindows10OrGreater()) {
    ShowErrorBox(
        L"Unfortunately, your operating system is not supported. "
        L"Please, use at least Windows 10 to play.");
    return ERROR_EXE_MACHINE_TYPE_MISMATCH;
  }

  // Do not show fault error boxes, etc.
  const source::windows::ScopedErrorMode scoped_error_mode{
#ifdef NDEBUG
  // The system does not display the critical-error-handler message box.
  // Instead, the system sends the error to the calling process.
  // Enable only in Release to detect critical errors during debug builds.
  // SEM_FAILCRITICALERRORS |
#endif
      // The system automatically fixes memory alignment faults and makes them
      // invisible to the application. It does this for the calling process and
      // any descendant processes.
      SEM_NOALIGNMENTFAULTEXCEPT |
      // The system does not display the Windows Error Reporting dialog.
      SEM_NOGPFAULTERRORBOX};

  // Enable heap corruption detection.
  if (!HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                          0)) {
    return NotifyAboutError(
        L"Failed to enable heap terminate-on-corruption feature. "
        L"Please, contact support.");
  }

  // Use the .exe name to determine the root directory.
  wch module_name[MAX_PATH];
  if (!GetModuleFileNameW(instance, module_name, ARRAYSIZE(module_name))) {
    return NotifyAboutError(
        L"GetModuleFileName call error. Please, contact support.");
  }

  // Get the directory the .exe is in.
  const wstr root_dir{GetDirectoryFromExePath(module_name)};
  // Assemble the full path to our "launcher.dll".
  const wstr launcher_dll_path{root_dir + L"\\bin\\launcher.dll"};

  // STEAM OK ... file system not mounted yet.
  auto [launcher_module, error_code] =
      source::windows::unique_module_ptr::from_load_library(
          launcher_dll_path, LOAD_WITH_ALTERED_SEARCH_PATH);

  if (launcher_module) {
    using LauncherMain = int (*)(HINSTANCE, int);
    LauncherMain main;
    std::tie(main, error_code) =
        launcher_module.get_address_as<LauncherMain>("LauncherMain");

    return main ? (*main)(instance, cmd_show)
                : NotifyAboutNoEntryPointError(error_code);
  }

  return NotifyAboutNoLauncherError(error_code);
}
