// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines the entry point for the application.

#include <cstdio>
#include <tuple>

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/scoped_winsock_initializer.h"
#include "base/include/windows/windows_light.h"

#include <shellapi.h>
#include <shlwapi.h>  // registry stuff
#endif

#include "appframework/AppFramework.h"
#include "engine_launcher_api.h"
#include "tier0/include/compiler_specific_macroses.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "tier1/strtools.h"

#include "file_system_access_logger.h"
#include "ireslistgenerator.h"
#include "scoped_memory_leak_dumper.h"
#include "source_app_system_group.h"
#include "vcr_helpers.h"

#include "tier0/include/memdbgon.h"

namespace {
// Spew function!
SpewRetval_t LauncherSpewFunc(_In_ SpewType_t spew_type,
                              _In_z_ ch const *message) {
  OutputDebugStringA(message);
  switch (spew_type) {
    case SPEW_MESSAGE:
    case SPEW_LOG:
      fprintf(stdout, "%s", message);
      return SPEW_CONTINUE;

    case SPEW_WARNING:
      fprintf(stderr, "%s", message);
      if (!_stricmp(GetSpewOutputGroup(), "init")) {
        MessageBoxA(nullptr, message, "Awesome Launcher - Warning",
                    MB_OK | MB_ICONWARNING);
      }
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
      fprintf(stderr, "%s", message);
      if (!ShouldUseNewAssertDialog())
        MessageBoxA(nullptr, message, "Awesome Launcher - Assert",
                    MB_OK | MB_ICONWARNING);
      return SPEW_DEBUGGER;

    case SPEW_ERROR:
    default:
      fprintf(stderr, "%s", message);
      MessageBoxA(nullptr, message, "Awesome Launcher - Error",
                  MB_OK | MB_ICONERROR);
      _exit(1);
  }
}

const ch *ComputeBaseDirectoryFromCommandLine(
    _In_ const ICommandLine *command_line) {
  static ch base_directory[MAX_PATH] = {0};
  const ch *cmd_base_directory{command_line->CheckParm(
      source::tier0::command_line_switches::baseDirectory)};

  if (cmd_base_directory) {
    strcpy_s(base_directory, cmd_base_directory);
    Q_strlower(base_directory);
    Q_FixSlashes(base_directory);

    return base_directory;
  }

  return nullptr;
}

// Gets the executable name
inline u32 GetExecutableName(_In_ ch *exe_name, usize exe_name_length) {
  const HMODULE exe_module = GetModuleHandleW(nullptr);
  if (exe_module &&
      GetModuleFileNameA(exe_module, exe_name,
                         static_cast<DWORD>(exe_name_length)) != 0) {
    return NO_ERROR;
  }

  return GetLastError();
}

std::tuple<const ch *, u32> ComputeBaseDirectoryFromExePath() {
  static ch base_directory[MAX_PATH];
  const u32 return_code{
      GetExecutableName(base_directory, ARRAYSIZE(base_directory))};

  if (return_code == NO_ERROR) {
    ch *last_backward_slash{strrchr(base_directory, '\\')};
    if (*last_backward_slash) {
      *(last_backward_slash + 1) = '\0';
    }

    const usize base_directory_length{strlen(base_directory)};
    if (base_directory_length > 0) {
      ch &one_but_last_char{base_directory[base_directory_length - 1]};
      if (one_but_last_char == '\\' || one_but_last_char == '/') {
        one_but_last_char = '\0';
      }
    }

    Q_strlower(base_directory);
    Q_FixSlashes(base_directory);
  }

  return {base_directory, return_code};
}

// Purpose: Determine the directory where this .exe is running from
std::tuple<const ch *, u32> ComputeBaseDirectory(
    _In_ const ICommandLine *command_line) {
  const ch *cmd_base_directory{
      ComputeBaseDirectoryFromCommandLine(command_line)};

  // nullptr means nothing to get from command line.
  if (cmd_base_directory == nullptr) {
    return ComputeBaseDirectoryFromExePath();
  }

  return {cmd_base_directory, NO_ERROR};
}

inline const ch *GetCtrlEventDescription(_In_ DWORD ctrl_type) {
  switch (ctrl_type) {
    case CTRL_C_EVENT:
      return "CTRL+C";
    case CTRL_BREAK_EVENT:
      return "CTRL+BREAK";
    case CTRL_CLOSE_EVENT:
      return "Console window close";
    case CTRL_LOGOFF_EVENT:
      return "User is logging off";
    case CTRL_SHUTDOWN_EVENT:
      return "System is shutting down";
    default:
      assert(false);
      return "N/A";
  }
}

[[noreturn]] BOOL WINAPI ConsoleCtrlHandler(_In_ DWORD ctrl_type) {
  Warning("Exit process, since event '%s' occurred.",
          GetCtrlEventDescription(ctrl_type));
  // TODO: Process ctrl_type, change process exit code.
  ExitProcess(NO_ERROR);
}

std::tuple<bool, u32> InitTextModeIfNeeded(
    _In_ const ICommandLine *command_line) {
  if (!command_line->CheckParm(
          source::tier0::command_line_switches::textMode)) {
    return {false, NO_ERROR};
  }

  u32 return_code{AllocConsole() != FALSE ? NO_ERROR : GetLastError()};
  if (return_code == NO_ERROR) {
    // reopen stdin, stout, stderr handles as console window input, output and
    // output.
    return_code =
        freopen("CONIN$", "rb", stdin) && freopen("CONOUT$", "wb", stdout) &&
                freopen("CONOUT$", "wb", stderr)
            ? NO_ERROR
            : errno;  // Yea, no direct mapping errno => GetLastError() :(

    // freopen set last error in errno (not 0 => not NO_ERROR).
    if (return_code != NO_ERROR) {
      Error(
          "Can't redirect stdin, stout, stderr to console, error code %d, "
          "message %s.",
          errno, strerror(errno));
    }
  }

  if (return_code == NO_ERROR) {
    return_code = SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE) != FALSE
                      ? NO_ERROR
                      : GetLastError();
  }

  return {return_code == NO_ERROR, return_code};
}

// Remove all but the last -game parameter. This is for mods based off something
// other than Half-Life 2 (like HL2MP mods). The Steam UI does 'steam -applaunch
// 320 -game c:\steam\steamapps\sourcemods\modname', but applaunch inserts its
// own -game parameter, which would supersede the one we really want if we
// didn't intercede here.
void RemoveSpuriousGameParameters(ICommandLine *const command_line) {
  // Find the last -game parameter.
  usize count_game_args = 0;
  ch last_game_arg[MAX_PATH];

  for (usize i = 0; i < command_line->ParmCount() - 1; i++) {
    if (Q_stricmp(command_line->GetParm(i),
                  source::tier0::command_line_switches::game_path) == 0) {
      Q_snprintf(last_game_arg, ARRAYSIZE(last_game_arg), "\"%s\"",
                 command_line->GetParm(i + 1));
      ++count_game_args;
      ++i;
    }
  }

  // We only care if > 1 was specified.
  if (count_game_args > 1) {
    command_line->RemoveParm(source::tier0::command_line_switches::game_path);
    command_line->AppendParm(source::tier0::command_line_switches::game_path,
                             last_game_arg);
  }
}

void RelaunchWithNewLanguageViaSteam() {
  // If there is a URL here, exec it. This supports the capability of
  // immediately re-launching the game via Steam in a different audio language.
  HKEY source_key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Source",
                    REG_OPTION_RESERVED, KEY_ALL_ACCESS,
                    &source_key) == NO_ERROR) {
    ch relaunch_url[MAX_PATH];
    DWORD relaunch_url_length = ARRAYSIZE(relaunch_url);

    if (RegQueryValueExA(source_key, "Relaunch URL", nullptr, nullptr,
                         (LPBYTE)relaunch_url,
                         &relaunch_url_length) == NO_ERROR) {
      constexpr u32 maxErrorCode = 32;

      // Fine, HINSTANCE is int here. See
      // https://msdn.microsoft.com/en-us/library/windows/desktop/bb762153(v=vs.85).aspx
      MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
      MSVC_DISABLE_WARNING(4302)  // 'reinterpret_cast': pointer truncation from
                                  // 'HINSTANCE' to 'int'
      // 'reinterpret_cast': truncation from 'HINSTANCE' to 'int'
      MSVC_DISABLE_WARNING(4311)
      u32 return_code = static_cast<u32>(reinterpret_cast<i32>(ShellExecuteA(
          nullptr, "open", relaunch_url, nullptr, nullptr, SW_SHOW)));
      MSVC_END_WARNING_OVERRIDE_SCOPE();

      // And 0 <=> The operating system is out of memory or resources.
      return_code =
          return_code > maxErrorCode
              ? NO_ERROR
              : (return_code == 0 ? ERROR_NOT_ENOUGH_MEMORY : return_code);

      if (return_code != NO_ERROR) {
        Warning("Can't relaunch %s, error code 0x%x.", relaunch_url,
                return_code);
      }

      return_code =
          static_cast<u32>(RegDeleteValueW(source_key, L"Relaunch URL"));

      if (return_code != NO_ERROR) {
        Warning("Can't delete registry key value %s, error code 0x%x.",
                "Software\\Valve\\Source Relaunch URL", return_code);
      }
    }

    const u32 return_code = static_cast<u32>(RegCloseKey(source_key));

    if (return_code != NO_ERROR) {
      Warning("Can't close registry key %s, error code 0x%x.",
              "Software\\Valve\\Source", return_code);
    }
  }
}

// Create command line.
ICommandLine *const CreateCommandLine() {
  ICommandLine *const command_line{CommandLine()};
  command_line->CreateCmdLine(VCRHook_GetCommandLine());
  return command_line;
}

u32 SetProcessPriorityInternal(_In_z_ const ch *priority_switch,
                               _In_ DWORD priority_class) {
  const BOOL return_code{SetPriorityClass(GetCurrentProcess(), priority_class)};

  if (return_code == FALSE) {
    const u32 error_code{GetLastError()};
    Warning("Can't set process priority by switch %s, error code 0x%x.",
            priority_switch, error_code);
    return error_code;
  }

  return NO_ERROR;
}

u32 SetProcessPriorityIfNeeded(_In_ const ICommandLine *command_line) {
  const bool is_low_priority{!!command_line->CheckParm(
      source::tier0::command_line_switches::priority_low)};
  const bool is_high_priority{!!command_line->CheckParm(
      source::tier0::command_line_switches::priority_high)};

  if (is_low_priority && is_high_priority) {
    Error(
        "Can't set process priority to low and high at the same time. Use one "
        "of %s,%s switches.",
        source::tier0::command_line_switches::priority_low,
        source::tier0::command_line_switches::priority_high);
    return ERROR_BAD_ARGUMENTS;
  }

  if (!is_low_priority && !is_high_priority) return NO_ERROR;

  if (is_low_priority) {
    return SetProcessPriorityInternal(
        source::tier0::command_line_switches::priority_low,
        IDLE_PRIORITY_CLASS);
  }

  return SetProcessPriorityInternal(
      source::tier0::command_line_switches::priority_high, HIGH_PRIORITY_CLASS);
}

// Remove any overrides in case settings changed.
void CleanupSettings(_In_ ICommandLine *const command_line) {
  command_line->RemoveParm("-w");
  command_line->RemoveParm("-h");
  command_line->RemoveParm("-width");
  command_line->RemoveParm("-height");
  command_line->RemoveParm("-sw");
  command_line->RemoveParm("-startwindowed");
  command_line->RemoveParm("-windowed");
  command_line->RemoveParm("-window");
  command_line->RemoveParm("-full");
  command_line->RemoveParm("-fullscreen");
  command_line->RemoveParm("-dxlevel");
  command_line->RemoveParm("-autoconfig");
  command_line->RemoveParm("+mat_hdr_level");
}

u32 RunSteamApplication(_In_ ICommandLine *command_line,
                        _In_z_ const ch *base_directory,
                        _In_ const bool is_text_mode) {
  bool need_restart{true};
  FileSystemAccessLogger file_system_access_logger{base_directory,
                                                   command_line};

  while (need_restart) {
    SourceAppSystemGroup source_app_system_group{
        base_directory, is_text_mode, command_line, file_system_access_logger};
    CSteamApplication steam_app(&source_app_system_group);
    int return_code{steam_app.Run()};

    need_restart =
        steam_app.GetErrorStage() == SourceAppSystemGroup::INITIALIZATION &&
            return_code == INIT_RESTART ||
        return_code == RUN_RESTART;

    bool should_continue_generate_reslists{false};
    if (!need_restart) {
      should_continue_generate_reslists = reslistgenerator->ShouldContinue();
      need_restart = should_continue_generate_reslists;
    }

    if (!should_continue_generate_reslists) {
      CleanupSettings(command_line);
    }
  }

  return NO_ERROR;
}
}  // namespace

// The real entry point for the application
DLL_EXPORT int LauncherMain(_In_ HINSTANCE instance, _In_ int) {
  // First, since almost all depends on this.
  SetAppInstance(instance);
  SpewOutputFunc(LauncherSpewFunc);

  // Parse command line early, since used extensively.
  ICommandLine *const command_line = CreateCommandLine();

  // Dump memory leaks if needed.
  ScopedMemoryLeakDumper scoped_memory_leak_dumper{
      g_pMemAlloc, command_line->FindParm("-leakcheck") > 0};

  // Run in text mode? (No graphics or sound).
  auto [is_text_mode, return_code] = InitTextModeIfNeeded(command_line);
  if (return_code != NO_ERROR) {
    Error("Can't run in text mode, error code 0x%x.", return_code);
    return return_code;
  }

  // Winsock is ready, skip if can't initialize.
  source::windows::ScopedWinsockInitializer scoped_winsock_initializer{
      source::windows::WinsockVersion::Version2_2};
  return_code = scoped_winsock_initializer.error_code();
  if (return_code != NO_ERROR) {
    Warning("Winsock 2.2 unavailable, error code 0x%x.", return_code);
  }

  VCRHelpers vcr_helpers;

  // Find directory exe is running from.
  auto [base_directory, return_code] = ComputeBaseDirectory(command_line);
  if (return_code == NO_ERROR) {
    // VCR helpers is ready.
    std::tie(vcr_helpers, return_code) = BootstrapVCRHelpers(command_line);
  }

  if (return_code == NO_ERROR) {
    // See the function for why we do this.
    RemoveSpuriousGameParameters(command_line);

    // Set process priority if requested.
    return_code = SetProcessPriorityIfNeeded(command_line);
  }

  if (return_code == NO_ERROR) {
    // If game is not run from Steam then add -insecure in order to avoid
    // client timeout message.
    if (command_line->FindParm("-steam") == 0) {
      command_line->AppendParm("-insecure", nullptr);
    }

    // Figure out the directory the executable is running from and make that
    // be the current working directory.
    return_code = SetCurrentDirectoryA(base_directory) != FALSE
                      ? NO_ERROR
                      : GetLastError();
  }

  if (return_code == NO_ERROR) {
    return_code =
        RunSteamApplication(command_line, base_directory, is_text_mode);
  }

  RelaunchWithNewLanguageViaSteam();

  return return_code;
}
