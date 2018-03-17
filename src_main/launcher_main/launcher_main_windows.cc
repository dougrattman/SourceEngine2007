// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <tuple>

#include "base/include/base_types.h"
#include "base/include/windows/scoped_error_mode.h"
#include "base/include/windows/unique_module_ptr.h"
#include "base/include/windows/windows_light.h"

namespace {
// Gets the file directory from |file_path|.
inline wstr GetDirectoryFromFilePath(_In_ wstr file_path) {
  const usize forward_slash_pos{file_path.rfind(L'\\')};
  if (wstr::npos != forward_slash_pos) {
    file_path.resize(forward_slash_pos + 1);
  }

  const usize base_dir_length{file_path.length()};
  if (base_dir_length > 0) {
    const usize last_char_index{base_dir_length - 1};
    const wch last_char{file_path[last_char_index]};
    if (last_char == L'\\' || last_char == L'/') {
      file_path.resize(last_char_index);
    }
  }

  return file_path;
}

// Get system-specific error for |error_code|.
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

// Build error from message |message| and error code |error_code|.
inline wstr BuildError(_In_ wstr message, _In_ u32 error_code) {
  message += L"\n\nPrecise error description: ";
  message += GetSystemError(error_code);
  return message;
}

// Show error box with |message|.
inline void ShowErrorBox(_In_ wstr message) {
  MessageBoxW(nullptr, message.c_str(), SOURCE_GAME_NAME L" - Startup Error",
              MB_ICONERROR | MB_OK);
}

// Show error box with |message| by |error_code|.
inline u32 NotifyAboutError(_In_ wstr message,
                            _In_ u32 error_code = GetLastError()) {
  ShowErrorBox(BuildError(message, error_code));
  return error_code;
}

// Show no launcher at path |launcher_dll_path| error box with system-specific
// error message from error code |error_code|.
inline u32 NotifyAboutNoLauncherError(_In_ const wstr &launcher_dll_path,
                                      _In_ u32 error_code) {
  wstr user_friendly_message{
      L"Please, contact support. Failed to load the launcher.dll from " +
      launcher_dll_path};
  return NotifyAboutError(user_friendly_message, error_code);
}

// Show no launcher at path |launcher_dll_path| entry point error box with
// system-specific error message from error code |error_code|.
inline u32 NotifyAboutNoEntryPointError(_In_ const wstr &launcher_dll_path,
                                        _In_ u32 error_code) {
  wstr user_friendly_message{L"Please, contact support. Failed to find the " +
                             launcher_dll_path + L" entry point."};
  return NotifyAboutError(user_friendly_message, error_code);
}

// Enable termination on heap corruption.
inline u32 EnableTerminationOnHeapCorruption() {
  // Enables the terminate-on-corruption feature.  If the heap manager detects
  // an error in any heap used by the process, it calls the Windows Error
  // Reporting service and terminates the process.
  //
  // See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366705(v=vs.85).aspx
  return HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                            0)
             ? NOERROR
             : GetLastError();
}

// Get module's file path with instance |instance|.
inline std::tuple<wstr, u32> GetThisModuleFilePath(_In_ HINSTANCE instance) {
  wch this_module_file_path[MAX_PATH];
  // If the function succeeds, the return value is the length of the string that
  // is copied to the buffer, in characters, not including the terminating null
  // character.  If the buffer is too small to hold the module name, the string
  // is truncated to nSize characters including the terminating null character,
  // the function returns nSize, and the function sets the last error to
  // ERROR_INSUFFICIENT_BUFFER.  If the function fails, the return value is 0
  // (zero).  To get extended error information, call GetLastError.
  //
  // See https://msdn.microsoft.com/en-us/library/ms683197(v=vs.85).aspx
  return GetModuleFileNameW(instance, this_module_file_path,
                            ARRAYSIZE(this_module_file_path))
             ? std::make_tuple(wstr{this_module_file_path}, GetLastError())
             : std::make_tuple(wstr{}, GetLastError());
}
}  // namespace

// Game for Windows entry point.
int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE,
                      _In_ wchar_t *, _In_ int cmd_show) {
  // Game uses features of Windows 10.
  if (!IsWindows10OrGreater()) {
    return NotifyAboutError(
        L"Unfortunately, your environment is not supported."
        L" " SOURCE_GAME_NAME L" requires at least Windows 10 to survive.",
        ERROR_EXE_MACHINE_TYPE_MISMATCH);
  }

  // Do not show fault error boxes, etc.
  const source::windows::ScopedErrorMode scoped_error_mode{
#ifdef NDEBUG
      // The system does not display the critical-error-handler message box.
      // Instead, the system sends the error to the calling process.
      // Enable only in Release to detect critical errors during debug builds.
      SEM_FAILCRITICALERRORS |
#endif
      // The system automatically fixes memory alignment faults and makes them
      // invisible to the application.  It does this for the calling process and
      // any descendant processes.
      SEM_NOALIGNMENTFAULTEXCEPT |
      // The system does not display the Windows Error Reporting dialog.
      SEM_NOGPFAULTERRORBOX};

  // Enable heap corruption detection & app termination.
  u32 error_code = EnableTerminationOnHeapCorruption();
  if (error_code != NOERROR) {
    return NotifyAboutError(
        L"Please, contact support. Failed to enable termination on heap "
        L"corruption feature for your environment.");
  }

  // Use the .exe name to determine the root directory.
  wstr this_module_file_path;
  std::tie(this_module_file_path, error_code) = GetThisModuleFilePath(instance);
  if (error_code != NOERROR) {
    return NotifyAboutError(
        L"Please, contact support. Can't get current exe file path.",
        error_code);
  }

  const wstr root_dir{GetDirectoryFromFilePath(this_module_file_path)};
  // Assemble the full path to our "launcher.dll".
  const wstr launcher_dll_path{root_dir + L"\\bin\\launcher.dll"};

  // STEAM OK ... file system not mounted yet.
  source::windows::unique_module_ptr launcher_module;
  std::tie(launcher_module, error_code) =
      source::windows::unique_module_ptr::from_load_library(
          launcher_dll_path, LOAD_WITH_ALTERED_SEARCH_PATH);

  if (error_code == NOERROR && launcher_module) {
    using LauncherMain = int (*)(HINSTANCE, int);
    LauncherMain main;
    std::tie(main, error_code) =
        launcher_module.get_address_as<LauncherMain>("LauncherMain");

    // Go!
    return error_code == NOERROR && main
               ? (*main)(instance, cmd_show)
               : NotifyAboutNoEntryPointError(launcher_dll_path, error_code);
  }

  return NotifyAboutNoLauncherError(launcher_dll_path, error_code);
}
