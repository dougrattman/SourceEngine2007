// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: This is just a little redirection tool to get all the dlls in bin.

#include <cstdio>
#include <string>

#include "public/winlite.h"

#ifdef _MSC_VER
// Ensure common controls are displayed in the user's preferred visual style.
// See
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb773175(v=vs.85).aspx
// clang-format off
#pragma comment(linker, \
  "\"/manifestdependency:type='win32' \
  name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
  processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// clang-format on
#endif

namespace {
// Purpose: Return the directory where this .exe is running from.
inline std::wstring GetBaseDirectory(_In_z_ const wchar_t *module_path) {
  std::wstring base_dir{module_path};
  const std::size_t forward_slash_pos = base_dir.rfind(L'\\');
  if (std::wstring::npos != forward_slash_pos) {
    base_dir.resize(forward_slash_pos + 1);
  }

  const size_t base_dir_length = base_dir.length();
  if (base_dir_length > 0) {
    const std::size_t last_char_index = base_dir_length - 1;
    const wchar_t last_char = base_dir[last_char_index];
    if (last_char == L'\\' || last_char == L'/') {
      base_dir.resize(last_char_index);
    }
  }

  return base_dir;
}

inline void ShowErrorBox(_In_z_ const wchar_t *message) {
  MessageBoxW(nullptr, message, L"Awesome Launcher - Startup Error",
              MB_ICONERROR | MB_OK);
}

inline DWORD ShowErrorBoxAndGetErrorCode(
    _In_z_ const wchar_t *message, _In_ DWORD error_code = GetLastError()) {
  ShowErrorBox(message);
  return error_code;
}

inline DWORD ShowNoLauncherErrorBoxAndGetErrorCode(
    _In_ DWORD error_code = GetLastError()) {
  wchar_t *system_error;

  if (!FormatMessageW(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
              FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPWSTR)&system_error, 0, nullptr)) {
    ShowErrorBox(
        L"Failed to get error description for the dedicated DLL load error."
        L"Please, contact support.");
    return error_code;
  }

  wchar_t error_msg[1024];
  _snwprintf_s(error_msg, ARRAYSIZE(error_msg),
               L"Please, contact support. Failed to load the dedicated "
               L"DLL:\n\n%s\n",
               system_error);

  ShowErrorBox(error_msg);
  LocalFree(system_error);

  return error_code;
}

inline DWORD ShowNoLaucherEntryPointErrorBoxAndGetErrorCode(
    _In_ DWORD error_code = GetLastError()) {
  ShowErrorBox(
      L"Please, contact support. Failed to find the dedicated DLL entry "
      L"point.");
  return error_code;
}
}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR,
                      _In_ int cmd_show) {
  // Notice call to GetErrorMode - it allows to append error modes instead of
  // overwriting old ones.
  (void)SetErrorMode(
      GetErrorMode() |
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
      SEM_NOGPFAULTERRORBOX);

  // Use features of at least Windows 10.
  if (!IsWindows10OrGreater()) {
    ShowErrorBox(
        L"Unfortunately, your operating system is not supported. "
        L"Please, use at least Windows 10 to play.");
    return ERROR_EXE_MACHINE_TYPE_MISMATCH;
  }

  // Enable heap corruption protection.
  if (HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr,
                         0) == FALSE) {
    return ShowErrorBoxAndGetErrorCode(
        L"Failed to enable heap terminate-on-corruption. "
        L"Please, contact support.");
  }

  // Use the .EXE name to determine the root directory.
  wchar_t module_name[MAX_PATH];
  if (!GetModuleFileNameW(instance, module_name, ARRAYSIZE(module_name))) {
    return ShowErrorBoxAndGetErrorCode(
        L"Failed calling GetModuleFileName. "
        L"Please, contact support.");
  }

  // Get the root directory the .exe is in.
  const std::wstring root_dir{GetBaseDirectory(module_name)};
  // Assemble the full path to our "dedicated.dll".
  const std::wstring dedicated_dll_path{root_dir + L"\\bin\\dedicated.dll"};

  // STEAM OK ... file system not mounted yet.
  const HMODULE dedicated_module{LoadLibraryExW(
      dedicated_dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)};
  if (!dedicated_module) {
    return ShowNoLauncherErrorBoxAndGetErrorCode();
  }

  using DedicatedMainFunc = int (*)(HINSTANCE, int);
  const auto main = reinterpret_cast<DedicatedMainFunc>(
      GetProcAddress(dedicated_module, "DedicatedMain"));

  return main ? (*main)(instance, cmd_show)
              : ShowNoLaucherEntryPointErrorBoxAndGetErrorCode();
}
