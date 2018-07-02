// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines the entry point for the application.

#include <cstdio>
#include <tuple>

#include "build/include/build_config.h"

#include "base/include/windows/error_notifications.h"
#include "base/include/windows/scoped_winsock_initializer.h"
#include "base/include/windows/windows_light.h"

#include <shellapi.h>
#include <shlwapi.h>  // registry stuff

#include "appframework/AppFramework.h"
#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "engine_launcher_api.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/system_info.h"
#include "tier0/include/vcrmode.h"
#include "tier1/strtools.h"

#include "file_system_access_logger.h"
#include "iresource_listing_writer.h"
#include "scoped_memory_leak_dumper.h"
#include "source_app_system_group.h"
#include "vcr_helpers.h"

#include "tier0/include/memdbgon.h"

namespace {
// Spew function.
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
  static ch base_directory[SOURCE_MAX_PATH] = {0};
  const ch *cmd_base_directory{command_line->CheckParm(
      source::tier0::command_line_switches::baseDirectory)};

  if (cmd_base_directory) {
    strcpy_s(base_directory, cmd_base_directory);
    _strlwr_s(base_directory);
    Q_FixSlashes(base_directory);

    return base_directory;
  }

  return nullptr;
}

// Gets the executable name.
inline source::windows::windows_errno_code GetExecutableName(
    _In_ ch *exe_name, usize exe_name_length) {
  const HMODULE exe_module = GetModuleHandleW(nullptr);
  if (exe_module &&
      GetModuleFileNameA(exe_module, exe_name,
                         static_cast<DWORD>(exe_name_length)) != 0) {
    return S_OK;
  }

  return source::windows::windows_errno_code_last_error();
}

std::tuple<const ch *, source::windows::windows_errno_code>
ComputeBaseDirectoryFromExePath() {
  static ch base_directory[SOURCE_MAX_PATH];
  const source::windows::windows_errno_code errno_code{
      GetExecutableName(base_directory, std::size(base_directory))};

  if (source::windows::succeeded(errno_code)) {
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

    _strlwr_s(base_directory);
    Q_FixSlashes(base_directory);
  }

  return {base_directory, errno_code};
}

// Purpose: Determine the directory where this .exe is running from
std::tuple<const ch *, source::windows::windows_errno_code>
ComputeBaseDirectory(_In_ const ICommandLine *command_line) {
  const ch *cmd_base_directory{
      ComputeBaseDirectoryFromCommandLine(command_line)};

  // nullptr means nothing to get from command line.
  return cmd_base_directory == nullptr
             ? ComputeBaseDirectoryFromExePath()
             : std::make_tuple(cmd_base_directory, S_OK);
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

BOOL WINAPI ConsoleCtrlHandler(_In_ DWORD ctrl_type) {
  Warning("Exit process, since event '%s' occurred.",
          GetCtrlEventDescription(ctrl_type));
  // TODO(d.rattman): Process ctrl_type, change process exit code.
  ExitProcess(NO_ERROR);
}

source::windows::windows_errno_result<bool> InitTextModeIfNeeded(
    _In_ const ICommandLine *command_line) {
  if (!command_line->CheckParm(
          source::tier0::command_line_switches::textMode)) {
    return {false, S_OK};
  }

  source::windows::windows_errno_code errno_code{
      AllocConsole() != FALSE
          ? S_OK
          : source::windows::windows_errno_code_last_error()};
  if (source::windows::succeeded(errno_code)) {
    FILE *dummy;
    // reopen stdin, stout, stderr handles as console window input, output and
    // output.
    errno_code = !freopen_s(&dummy, "CONIN$", "rb", stdin) &&
                         !freopen_s(&dummy, "CONOUT$", "wb", stdout) &&
                         !freopen_s(&dummy, "CONOUT$", "wb", stderr)
                     ? S_OK
                     : E_FAIL;  // Yea, no direct mapping errno => HRESULT :(

    // freopen set last error in errno (not 0 => not NO_ERROR).
    if (errno_code != S_OK) {
      ch error_description[96];
      strerror_s(error_description, errno);
      Error(
          "Can't redirect stdin, stout, stderr to console (%d), "
          "message %s.",
          errno, error_description);
    }
  }

  if (errno_code == S_OK) {
    errno_code = SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE) != FALSE
                     ? S_OK
                     : source::windows::windows_errno_code_last_error();
  }

  return {source::windows::succeeded(errno_code), errno_code};
}

// Remove all but the last source::tier0::command_line_switches::kGamePath
// parameter.  This is for mods based off something other than Half-Life 2 (like
// HL2MP mods).  The Steam UI does 'steam -applaunch 320
// source::tier0::command_line_switches::kGamePath
// c:\steam\steamapps\sourcemods\modname', but applaunch inserts its own
// source::tier0::command_line_switches::kGamePath parameter, which would
// supersede the one we really want if we didn't intercede here.
void RemoveSpuriousGameParameters(ICommandLine *const command_line) {
  // Find the last source::tier0::command_line_switches::kGamePath parameter.
  usize count_game_args = 0;
  ch last_game_arg[SOURCE_MAX_PATH];

  for (usize i = 0; i < command_line->ParmCount() - 1; i++) {
    if (_stricmp(command_line->GetParm(i),
                 source::tier0::command_line_switches::kGamePath) == 0) {
      sprintf_s(last_game_arg, "\"%s\"", command_line->GetParm(i + 1));
      ++count_game_args;
      ++i;
    }
  }

  // We only care if > 1 was specified.
  if (count_game_args > 1) {
    command_line->RemoveParm(source::tier0::command_line_switches::kGamePath);
    command_line->AppendParm(source::tier0::command_line_switches::kGamePath,
                             last_game_arg);
  }
}

void RelaunchIfNeeded() {
  // If there is a URL here, exec it. This supports the capability of
  // immediately re-launching the game via Steam in a different audio language.
  HKEY source_key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Source",
                    REG_OPTION_RESERVED, KEY_ALL_ACCESS,
                    &source_key) == NO_ERROR) {
    ch relaunch_url[SOURCE_MAX_PATH];
    DWORD relaunch_url_length = std::size(relaunch_url);

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
      u32 errno_code = static_cast<u32>(reinterpret_cast<i32>(ShellExecuteA(
          nullptr, "open", relaunch_url, nullptr, nullptr, SW_SHOW)));
      MSVC_END_WARNING_OVERRIDE_SCOPE();

      // And 0 means the operating system is out of memory or resources.
      errno_code =
          errno_code > maxErrorCode
              ? NO_ERROR
              : (errno_code == 0 ? ERROR_NOT_ENOUGH_MEMORY : errno_code);

      if (errno_code != NO_ERROR) {
        Warning(
            "Can't relaunch by %s: %s.", relaunch_url,
            source::windows::make_windows_errno_info(errno_code).description);
      }

      errno_code =
          static_cast<u32>(RegDeleteValueW(source_key, L"Relaunch URL"));

      if (errno_code != NO_ERROR) {
        Warning(
            "Can't delete registry key value Software\\Valve\\Source Relaunch "
            "URL: %s.",
            source::windows::make_windows_errno_info(errno_code).description);
      }
    }

    const u32 errno_code = static_cast<u32>(RegCloseKey(source_key));
    if (errno_code != NO_ERROR) {
      Warning("Can't close registry key Software\\Valve\\Source: %s.",
              source::windows::make_windows_errno_info(errno_code).description);
    }
  }
}

// Create command line.
ICommandLine *const CreateCommandLine() {
  ICommandLine *const command_line{CommandLine()};
  command_line->CreateCmdLine(VCRHook_GetCommandLine());
  return command_line;
}

source::windows::windows_errno_code SetProcessPriorityInternal(
    _In_z_ const ch *priority_switch, _In_ DWORD priority_class) {
  const BOOL errno_code{SetPriorityClass(GetCurrentProcess(), priority_class)};
  if (errno_code != FALSE) return S_OK;

  const auto errno_info{source::windows::windows_errno_info_last_error()};
  Warning("%s: Can't set process priority: %s.", priority_switch,
          errno_info.description);

  return errno_info.code;
}

source::windows::windows_errno_code SetProcessPriorityIfNeeded(
    _In_ const ICommandLine *command_line) {
  const bool is_low_priority{!!command_line->CheckParm(
      source::tier0::command_line_switches::kCpuPriorityLow)};
  const bool is_high_priority{!!command_line->CheckParm(
      source::tier0::command_line_switches::kCpuPriorityHigh)};

  if (is_low_priority && is_high_priority) {
    Error(
        "Can't set process priority to low and high at the same time. Please, "
        "use single of %s/%s.",
        source::tier0::command_line_switches::kCpuPriorityLow,
        source::tier0::command_line_switches::kCpuPriorityHigh);
    return source::windows::win32_to_windows_errno_code(ERROR_BAD_ARGUMENTS);
  }

  if (!is_low_priority && !is_high_priority) return S_OK;

  if (is_low_priority) {
    return SetProcessPriorityInternal(
        source::tier0::command_line_switches::kCpuPriorityLow,
        IDLE_PRIORITY_CLASS);
  }

  return SetProcessPriorityInternal(
      source::tier0::command_line_switches::kCpuPriorityHigh,
      HIGH_PRIORITY_CLASS);
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

source::windows::windows_errno_code Run(_In_ ICommandLine *command_line,
                                        _In_z_ const ch *base_directory,
                                        _In_ const bool is_text_mode) {
  bool need_restart{true};
  FileSystemAccessLogger file_system_access_logger{base_directory,
                                                   command_line};

  while (need_restart) {
    SourceAppSystemGroup source_app_system_group{
        base_directory, is_text_mode, command_line, file_system_access_logger};
    CSteamApplication steam_app(&source_app_system_group);
    int errno_code{steam_app.Run()};

    need_restart =
        steam_app.GetErrorStage() == SourceAppSystemGroup::INITIALIZATION &&
            errno_code == INIT_RESTART ||
        errno_code == RUN_RESTART;

    bool should_continue_generate_reslists{false};
    if (!need_restart) {
      should_continue_generate_reslists = ResourceListing()->ShouldContinue();
      need_restart = should_continue_generate_reslists;
    }

    if (!should_continue_generate_reslists) {
      CleanupSettings(command_line);
    }
  }

  return S_OK;
}
}  // namespace

// Here app go.
SOURCE_API_EXPORT source::windows::windows_errno_code LauncherMain(
    _In_ HINSTANCE instance, _In_ int) {
  if (instance == nullptr) return ERROR_INVALID_HANDLE;

  using namespace source::windows;

  // Parse command line early, since used extensively. Should not use SSE and
  // /or SSE2.
  ICommandLine *const command_line = CreateCommandLine();

  // CPU should be supported, or we crash later.
  auto[cpu_info, errno_code] = source::QueryCpuInfo(sizeof(source::CpuInfo));
  if (failed(errno_code) || !cpu_info.is_info.has_sse ||
      !cpu_info.is_info.has_sse2) {
    if (!command_line->CheckParm("-skip_cpu_checks")) {
      // Debug infrastructure is not ready, so use Windows message box.
      return NotifyAboutError(
          "Sorry, query CPU compatibility for the game is failed. Looks like "
          "your CPU doesn't support CPUID/SSE/SSE2 instructions, which are "
          "required to run the game.\n\nYou can try launch the game with "
          "-skip_cpu_checks flag to skip CPU checks, but there is no "
          "guarantee that it helps. If the game still fails than you should "
          "upgrade the CPU to run the game :(.",
          failed(errno_code)
              ? errno_code
              : win32_to_windows_errno_code(ERROR_NOT_SUPPORTED));
    }
  }

  SetAppInstance(instance);
  SpewOutputFunc(LauncherSpewFunc);

  // Dump memory leaks if needed.
  ScopedMemoryLeakDumper scoped_memory_leak_dumper{
      g_pMemAlloc, command_line->FindParm("-leakcheck") > 0};

  // Run in text mode? (No graphics or sound).
  bool is_text_mode;
  std::tie(is_text_mode, errno_code) = InitTextModeIfNeeded(command_line);
  if (failed(errno_code)) {
    Error("The game failed to run in text mode: %s.",
          make_windows_errno_info(errno_code).description);
    return errno_code;
  }

  // Winsock is ready, skip if can't initialize.
  ScopedWinsockInitializer scoped_winsock_initializer{
      WinsockVersion::Version2_2};
  errno_code = scoped_winsock_initializer.error_code();
  if (failed(errno_code)) {
    Warning("Winsock 2.2 unavailable, networking may not run: %s.",
            make_windows_errno_info(errno_code).description);
  }

  VCRHelpers vcr_helpers;
  const ch *base_directory;

  // Find directory exe is running from.
  std::tie(base_directory, errno_code) = ComputeBaseDirectory(command_line);
  if (succeeded(errno_code)) {
    // VCR helpers is ready.
    std::tie(vcr_helpers, errno_code) = BootstrapVCRHelpers(command_line);
  }

  if (succeeded(errno_code)) {
    // Rehook the command line through VCR mode.
    CommandLine()->CreateCmdLine(VCRHook_GetCommandLine());

    // See the function for why we do this.
    RemoveSpuriousGameParameters(command_line);

    // Set process priority if requested.
    errno_code = SetProcessPriorityIfNeeded(command_line);
  }

  if (succeeded(errno_code)) {
    // If game is not run from Steam then add -insecure in order to avoid
    // client timeout message.
    if (command_line->FindParm("-steam") == 0) {
      command_line->AppendParm("-insecure", nullptr);
    }

    // Figure out the directory the executable is running from and make that
    // be the current working directory.
    errno_code = SetCurrentDirectoryA(base_directory) != FALSE
                     ? S_OK
                     : windows_errno_code_last_error();
  }

  if (succeeded(errno_code)) {
    errno_code = Run(command_line, base_directory, is_text_mode);
  }

  RelaunchIfNeeded();

  return errno_code;
}
