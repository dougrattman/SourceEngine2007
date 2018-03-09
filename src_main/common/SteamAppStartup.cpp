// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "SteamAppStartup.h"

#include <direct.h>
#include <process.h>
#include <sys/stat.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <tuple>

#include "base/include/windows/windows_light.h"

#define SOURCE_ENGINE_CMD_LINE_STEAM_ARG "-steam"

namespace {
// Save app data to steam registry.
bool SaveAppDataToSteam(_In_z_ const char *command_line,
                        _In_z_ const char *app_path) {
  // Save out our details to the registry.
  HKEY steam_key;
  if (ERROR_SUCCESS !=
      RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", &steam_key)) {
    return false;
  }

  if (ERROR_SUCCESS !=
      RegSetValueExW(steam_key, L"TempAppPath", 0, REG_SZ, (LPBYTE)app_path,
                     static_cast<DWORD>(strlen(app_path) + 1))) {
    RegCloseKey(steam_key);
    return false;
  }
  if (ERROR_SUCCESS !=
      RegSetValueExW(steam_key, L"TempAppCmdLine", 0, REG_SZ,
                     (LPBYTE)command_line,
                     static_cast<DWORD>(strlen(command_line) + 1))) {
    RegCloseKey(steam_key);
    return false;
  }

  // Clear out the appID (since we don't know it yet).
  const int unknown_app_id{-1};
  const bool return_code =
      (ERROR_SUCCESS == RegSetValueExW(steam_key, L"TempAppID", 0, REG_DWORD,
                                       (LPBYTE)&unknown_app_id,
                                       sizeof(unknown_app_id)));

  RegCloseKey(steam_key);
  return return_code;
}

// Purpose: Get steam exe path.
std::tuple<bool, std::array<char, SOURCE_MAX_PATH>> GetSteamExePath(
    _In_z_ char *current_dir) {
  std::array<char, SOURCE_MAX_PATH> steam_exe_path;

  char *slash = strrchr(current_dir, '\\');
  while (slash) {
    // see if steam_dev.exe is in the directory first
    slash[1] = 0;
    strcat(slash, "steam_dev.exe");

    FILE *f = fopen(current_dir, "rb");
    if (f) {
      // found it
      fclose(f);
      strcpy(steam_exe_path.data(), current_dir);
      return std::make_tuple(true, steam_exe_path);
    }

    // see if steam.exe is in the directory
    slash[1] = 0;
    strcat(slash, "steam.exe");

    f = fopen(current_dir, "rb");
    if (f) {
      // found it
      fclose(f);
      strcpy(steam_exe_path.data(), current_dir);
      return std::make_tuple(true, steam_exe_path);
    }

    // kill the string at the slash
    slash[0] = 0;

    // move to the previous slash
    slash = strrchr(current_dir, '\\');
  }

  return std::make_tuple(false, steam_exe_path);
}  // namespace

// Purpose: Find or launch steam and launch app via it.
bool FindSteamAndLaunchSelfViaIt() {
  char current_dir[SOURCE_MAX_PATH];
  if (!GetCurrentDirectoryA(SOURCE_ARRAYSIZE(current_dir), current_dir)) {
    return false;
  }

  // First, search backwards through our current set of directories
  bool return_code;
  std::array<char, SOURCE_MAX_PATH> steam_exe_path;
  std::tie(return_code, steam_exe_path) = GetSteamExePath(current_dir);

  if (!return_code) {
    // Still not found, use the one in the registry
    HKEY steam_key;
    if (ERROR_SUCCESS ==
        RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", &steam_key)) {
      DWORD value_type, value_size = steam_exe_path.size();
      RegQueryValueEx(steam_key, "SteamExe", nullptr, &value_type,
                      (LPBYTE)steam_exe_path.data(), &value_size);
      RegCloseKey(steam_key);
    }
  }

  if (!steam_exe_path[0]) {
    // Still no path, error
    MessageBoxW(nullptr,
                L"Error running game: could not find steam.exe to launch",
                L"Steam Launcher - Fatal Error", MB_OK | MB_ICONERROR);
    return false;
  }

  // Fix any slashes
  for (char *slash = steam_exe_path.data(); *slash; slash++) {
    if (*slash == '/') {
      *slash = '\\';
    }
  }

  // Change to the steam directory
  strcpy_s(current_dir, steam_exe_path.data());

  char *delimiter = strrchr(current_dir, '\\');
  if (delimiter) {
    *delimiter = 0;
    if (_chdir(current_dir) < 0) {
      return false;
    }
  }

  // Exec steam.exe, in silent mode, with the launch app param
  const char *const args[4] = {steam_exe_path.data(), "-silent", "-applaunch",
                               nullptr};
  return _spawnv(_P_NOWAIT, steam_exe_path.data(), args) >= 0;
}

// Purpose: Handles launching the game indirectly via steam.
bool LaunchSelfViaSteam(_In_z_ const char *command_line) {
  // Calculate the details of our launch.
  const HMODULE app_module = GetModuleHandleA(nullptr);
  if (!app_module) {
    return false;
  }

  char app_path[SOURCE_MAX_PATH];
  if (!GetModuleFileNameA(app_module, app_path, SOURCE_ARRAYSIZE(app_path))) {
    return false;
  }

  // Strip out the exe name.
  char *slash = strrchr(app_path, '\\');
  if (slash) {
    *slash = '\0';
  }

  if (!SaveAppDataToSteam(command_line, app_path)) {
    return false;
  }

  // Search for an active steam instance.
  const HWND steam_ipc_window =
      FindWindowW(L"Valve_SteamIPC_Class", L"Hidden Window");
  if (steam_ipc_window) {
    return PostMessageW(steam_ipc_window, WM_USER + 3, 0, 0) != FALSE;
  }

  return FindSteamAndLaunchSelfViaIt();
}

// Purpose: Check file exists or not.
inline bool FileExists(const char *file_path) {
  struct _stat statbuf;
  return (_stat(file_path, &statbuf) == 0);
}
}  // namespace

// Purpose: Launches steam if necessary.
bool ShouldLaunchAppViaSteam(const char *command_line,
                             const char *steam_file_system_dll_name,
                             const char *stdio_file_system_dll_name) {
  // See if steam is on the command line.
  const char *steam_arg{strstr(command_line, SOURCE_ENGINE_CMD_LINE_STEAM_ARG)};

  // Check the character following it is a whitespace or 0.
  if (steam_arg) {
    const char *next_steam_arg_char{steam_arg +
                                    strlen(SOURCE_ENGINE_CMD_LINE_STEAM_ARG)};
    if (*next_steam_arg_char == '\0' || isspace(*next_steam_arg_char)) {
      // We're running under steam already, let the app continue.
      return false;
    }
  }

  // We're not running under steam, see which file systems are available.
  // We're being run with a stdio file system, so we can continue without
  // steam. Or make sure we have a steam file system available.
  if (FileExists(stdio_file_system_dll_name) ||
      !FileExists(steam_file_system_dll_name)) {
    return false;
  }

  // We have the steam file system, and no stdio file system, so we must need to
  // be run under steam launch steam.
  return LaunchSelfViaSteam(command_line);
}

#endif  // _WIN32
