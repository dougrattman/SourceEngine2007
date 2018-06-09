// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <tuple>

#include "base/include/base_types.h"
#include "base/include/unique_module_ptr.h"
#include "base/include/windows/error_notifications.h"
#include "base/include/windows/scoped_error_mode.h"
#include "base/include/windows/windows_light.h"

// Indicates to hybrid graphics systems to prefer the discrete part by default.
extern "C" {
// Starting with the Release 302 drivers, application developers can direct the
// Optimus driver at runtime to use the High Performance Graphics to render any
// application —- even those applications for which there is no existing
// application profile.  See
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// This will select the high performance GPU as long as no profile exists that
// assigns the application to another GPU.  Please make sure to use a 13.35 or
// newer driver.  Older drivers do not support this.  See
// https://community.amd.com/thread/169965
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

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

// Show no launcher at path |launcher_dll_path| error box with system-specific
// error message from error info |errno_info|.
inline source::windows::windows_errno_code NotifyAboutNoLauncherError(
    _In_ const wstr &launcher_dll_path,
    _In_ source::windows::windows_errno_info errno_info) {
  wstr user_friendly_message{
      L"Please, contact support. Failed to load the launcher.dll from " +
      launcher_dll_path};
  return source::windows::NotifyAboutError(user_friendly_message, errno_info);
}

// Show no launcher at path |launcher_dll_path| entry point
// |launcher_dll_entry_point_name| error box with system-specific error message
// from error info |errno_info|.
inline source::windows::windows_errno_code NotifyAboutNoLauncherEntryPointError(
    _In_ const wstr &launcher_dll_path,
    _In_z_ const wch *launcher_dll_entry_point_name,
    _In_ source::windows::windows_errno_info errno_info) {
  wstr user_friendly_message{L"Please, contact support. Failed to find the " +
                             launcher_dll_path + L" entry point " +
                             launcher_dll_entry_point_name + L"."};
  return source::windows::NotifyAboutError(user_friendly_message, errno_info);
}

// Enable termination on heap corruption.
inline source::windows::windows_errno_code EnableTerminationOnHeapCorruption() {
  // Enables the terminate-on-corruption feature.  If the heap manager detects
  // an error in any heap used by the process, it calls the Windows Error
  // Reporting service and terminates the process.
  //
  // See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366705(v=vs.85).aspx
  return HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                            0)
             ? S_OK
             : source::windows::windows_errno_code_last_error();
}

// Get module's file path with instance |instance|.
inline source::windows::windows_errno_result<wstr> GetThisModuleFilePath(
    _In_ HINSTANCE instance) {
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
             ? std::make_tuple(wstr{this_module_file_path},
                               source::windows::windows_errno_code_last_error())
             : std::make_tuple(
                   wstr{}, source::windows::windows_errno_code_last_error());
}
}  // namespace

// Game for Windows entry point.
int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE,
                      _In_ wchar_t *, _In_ int cmd_show) {
  using namespace source::windows;

  // Game uses features of Windows 10.
  if (!IsWindows10OrGreater()) {
    return NotifyAboutError(
        L"Unfortunately, your environment is not supported."
        L" " SOURCE_APP_NAME L" requires at least Windows 10 to survive.",
        win32_to_windows_errno_code(ERROR_EXE_MACHINE_TYPE_MISMATCH));
  }

  // Do not show fault error boxes, etc.
  const ScopedErrorMode scoped_error_mode{
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
  windows_errno_code errno_code{EnableTerminationOnHeapCorruption()};
  if (failed(errno_code)) {
    return NotifyAboutError(
        L"Please, contact support. Failed to enable termination on heap "
        L"corruption feature for your environment.",
        errno_code);
  }

  // Use the .exe name to determine the root directory.
  wstr this_module_file_path;
  std::tie(this_module_file_path, errno_code) = GetThisModuleFilePath(instance);
  if (failed(errno_code)) {
    return NotifyAboutError(
        L"Please, contact support. Can't get current exe file path.",
        errno_code);
  }

  const wstr root_dir{GetDirectoryFromFilePath(this_module_file_path)};
  // Assemble the full path to our "launcher.dll".
  const wstr launcher_dll_path{root_dir + L"\\bin\\launcher.dll"};

  // STEAM OK ... file system not mounted yet.
  auto[launcher_module, errno_info] =
      source::unique_module_ptr::from_load_library(
          launcher_dll_path, LOAD_WITH_ALTERED_SEARCH_PATH);

  if (errno_info.is_success() && launcher_module) {
    using LauncherMain =
        source::windows::windows_errno_code (*)(HINSTANCE, int);
    LauncherMain main;
    std::tie(main, errno_info) =
        launcher_module.get_address_as<LauncherMain>("LauncherMain");

    // Go!
    return errno_info.is_success() && main
               ? (*main)(instance, cmd_show)
               : NotifyAboutNoLauncherEntryPointError(
                     launcher_dll_path, L"LauncherMain", errno_info);
  }

  return NotifyAboutNoLauncherError(launcher_dll_path, errno_info);
}
