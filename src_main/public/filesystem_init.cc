// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#undef PROTECTED_THINGS_ENABLE
#undef PROTECT_FILEIO_FUNCTIONS

#ifndef OS_POSIX
#undef fopen
#endif

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "filesystem_init.h"

#ifdef OS_WIN
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <cstdio>
#include "base/include/windows/windows_light.h"
#elif defined(OS_POSIX)
#include <unistd.h>
#define _putenv putenv
#define _chdir chdir
#define _access access
#endif

#include "appframework/iappsystemgroup.h"
#include "tier0/include/icommandline.h"
#include "tier1/keyvalues.h"
#include "tier1/smartptr.h"
#include "tier1/strtools.h"

#include "tier0/include/memdbgon.h"

#define GAMEINFO_FILENAME "gameinfo.txt"
#define GAMEINFO_FILENAME_ALTERNATE "gameinfo.txt"

static FSErrorMode_t g_FileSystemErrorMode = FS_ERRORMODE_VCONFIG;

void FileSystem_SetErrorMode(FSErrorMode_t errorMode) {
  g_FileSystemErrorMode = errorMode;
}

static bool s_bUseVProjectBinDir = false;

// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir(bool bEnable) {
  s_bUseVProjectBinDir = bEnable;
}

static thread_local ch g_FileSystemError[256];

const ch *FileSystem_GetLastErrorString() { return g_FileSystemError; }

// This class lets you modify environment variables, and it restores the
// original value when it goes out of scope.
class ScopedEnvVariable {
 public:
  ScopedEnvVariable(const ch *env_var_name)
      : should_restore_original_{true}, env_var_name_{env_var_name} {
    const ch *env_var_value = nullptr;

#ifdef OS_WIN
    // Use GetEnvironmentVariable instead of getenv because getenv doesn't pick
    // up changes to the process environment after the DLL was loaded.
    ch env_var_value_buffer[4096];
    if (GetEnvironmentVariable(env_var_name_, env_var_value_buffer,
                               std::size(env_var_value_buffer)) != 0) {
      env_var_value = env_var_value_buffer;
    }
#else
    // LINUX BUG: see above
    env_var_value = getenv(pVarName);
#endif

    is_env_var_exist_ = !!env_var_value;

    if (env_var_value) {
      original_env_var_.SetSize(strlen(env_var_value) + 1);
      memcpy(original_env_var_.Base(), env_var_value,
             original_env_var_.Count());
    }
  }

  ~ScopedEnvVariable() {
    if (should_restore_original_) {
      // Restore the original value.
      if (is_env_var_exist_) {
        SetValue("%s", original_env_var_.Base());
      } else {
        ClearValue();
      }
    }
  }

  void SetRestoreOriginalValue(bool bRestore) {
    should_restore_original_ = bRestore;
  }

  template <usize value_size>
  usize GetValue(ch (&env_var_value)[value_size]) {
    if (!env_var_value || (value_size == 0)) return 0;

#ifdef OS_WIN
    // Use GetEnvironmentVariable instead of getenv because getenv doesn't pick
    // up changes to the process environment after the DLL was loaded.
    return GetEnvironmentVariable(env_var_name_, env_var_value, value_size);
#else
    // LINUX BUG: see above
    const ch *env_var_value = getenv(env_var_name_);
    if (!env_var_value) {
      *env_var_value = '\0';
      return 0;
    }

    strcpy_s(env_var_value, env_var_value);
    return strlen(env_var_value);
#endif
  }

  bool SetValue(const ch *pValue, ...) {
    ch value_str[4096];
    va_list marker;
    va_start(marker, pValue);
    vsprintf_s(value_str, pValue, marker);
    va_end(marker);

    ch str[4096];
    sprintf_s(str, "%s=%s", env_var_name_, value_str);

    return _putenv(str) == 0;
  }

  bool ClearValue() {
    ch str[512];
    sprintf_s(str, "%s=", env_var_name_);

    return _putenv(str) == 0;
  }

 private:
  bool should_restore_original_;
  const ch *env_var_name_;
  bool is_env_var_exist_;
  CUtlVector<ch> original_env_var_;
};

struct SteamEnvVariables {
  SteamEnvVariables()
      : SteamAppId{"SteamAppId"},
        SteamUserPassphrase{"SteamUserPassphrase"},
        SteamAppUser{"SteamAppUser"},
        Path{"path"} {}

  void SetRestoreOriginalValue_ALL(bool bRestore) {
    SteamAppId.SetRestoreOriginalValue(bRestore);
    SteamUserPassphrase.SetRestoreOriginalValue(bRestore);
    SteamAppUser.SetRestoreOriginalValue(bRestore);
    Path.SetRestoreOriginalValue(bRestore);
  }

  ScopedEnvVariable SteamAppId;
  ScopedEnvVariable SteamUserPassphrase;
  ScopedEnvVariable SteamAppUser;
  ScopedEnvVariable Path;
};

void AddLanguageGameDir(IFileSystem *file_system, const ch *pLocation,
                        const ch *pLanguage) {
#ifndef SWDS
  ch temp[SOURCE_MAX_PATH];
  sprintf_s(temp, "%s_%s", pLocation, pLanguage);
  file_system->AddSearchPath(temp, "GAME", PATH_ADD_TO_TAIL);

  if (!file_system->IsSteam()) {
    // also look in "..\localization\<folder>" if not running Steam
    ch base_dir[SOURCE_MAX_PATH];
    strcpy_s(base_dir, pLocation);
    ch *game_part{strstr(base_dir, "\\game\\")};

    if (game_part) {
      ch *gameDir = game_part + strlen("\\game\\");
      *game_part = '\0';

      sprintf_s(temp, "%s%clocalization%c%s_%s", base_dir,
                CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, gameDir,
                pLanguage);
      file_system->AddSearchPath(temp, "GAME", PATH_ADD_TO_TAIL);
    }
  }
#endif
}

void AddGameBinDir(IFileSystem *pFileSystem, const ch *pLocation) {
  ch temp[SOURCE_MAX_PATH];
  sprintf_s(temp, "%s%cbin", pLocation, CORRECT_PATH_SEPARATOR);
  pFileSystem->AddSearchPath(temp, "GAMEBIN", PATH_ADD_TO_TAIL);
}

KeyValues *ReadKeyValuesFile(const ch *pFilename) {
  // Read in the gameinfo.txt file and 0-terminate it.
  FILE *fd;
  if (!fopen_s(&fd, pFilename, "rb")) {
    CUtlVector<ch> buf;

    fseek(fd, 0, SEEK_END);
    buf.SetSize(ftell(fd) + 1);

    fseek(fd, 0, SEEK_SET);
    fread(buf.Base(), 1, buf.Count() - 1, fd);

    fclose(fd);
    buf[buf.Count() - 1] = '\0';

    KeyValues *kv = new KeyValues("");
    if (!kv->LoadFromBuffer(pFilename, buf.Base())) {
      kv->deleteThis();
      return nullptr;  //-V773
    }

    return kv;
  }

  return nullptr;
}

template <usize len>
static bool Sys_GetExecutableName(ch (&out)[len]) {
#ifdef OS_WIN
  if (!::GetModuleFileName((HINSTANCE)GetModuleHandle(nullptr), out, len)) {
    return false;
  }
#else
  if (CommandLine()->GetParm(0)) {
    Q_MakeAbsolutePath(out, len, CommandLine()->GetParm(0));
  } else {
    return false;
  }
#endif

  return true;
}

template <usize exeDirLen>
bool FileSystem_GetExecutableDir(ch (&exedir)[exeDirLen]) {
  exedir[0] = 0;

  if (s_bUseVProjectBinDir) {
    const ch *pProject = GetVProjectCmdLineValue();
    if (!pProject) {
      // Check their registry.
      ch env_token[SOURCE_MAX_PATH];
      size_t env_token_size;

      pProject = !getenv_s(&env_token_size, env_token, GAMEDIR_TOKEN) &&
                         env_token_size > 0
                     ? env_token
                     : nullptr;
    }

    if (pProject && pProject[0]) {
      sprintf_s(exedir, "%s%c..%cbin", pProject, CORRECT_PATH_SEPARATOR,
                CORRECT_PATH_SEPARATOR);
      return true;
    }

    return false;
  }

  if (!Sys_GetExecutableName(exedir)) return false;

  Q_StripFilename(exedir);
  Q_FixSlashes(exedir);

  // Return the bin directory as the executable dir if it's not in there
  // because that's really where we're running from...
  ch ext[SOURCE_MAX_PATH];
  Q_StrRight(exedir, 4, ext, sizeof(ext));
  if (ext[0] != CORRECT_PATH_SEPARATOR || _stricmp(ext + 1, "bin") != 0) {
    strcat_s(exedir, "\\bin");
    Q_FixSlashes(exedir);
  }

  return true;
}

template <usize baseDirLen>
static bool FileSystem_GetBaseDir(ch (&baseDir)[baseDirLen]) {
  if (FileSystem_GetExecutableDir(baseDir)) {
    Q_StripFilename(baseDir);
    return true;
  }

  return false;
}

void LaunchVConfig() {
#ifdef OS_WIN
  ch vconfigExe[SOURCE_MAX_PATH];
  FileSystem_GetExecutableDir(vconfigExe);

  Q_AppendSlash(vconfigExe, std::size(vconfigExe));
  strcat_s(vconfigExe, "vconfig.exe");

  const ch *const argv[] = {vconfigExe, "-allowdebug", nullptr};
  _spawnv(_P_NOWAIT, vconfigExe, argv);
#endif
}

const ch *GetVProjectCmdLineValue() {
  return CommandLine()->ParmValue(
      "-vproject", CommandLine()->ParmValue(
                       source::tier0::command_line_switches::kGamePath));
}

FSReturnCode_t SetupFileSystemError(bool bRunVConfig, FSReturnCode_t retVal,
                                    const ch *pMsg, ...) {
  va_list marker;
  va_start(marker, pMsg);
  vsprintf_s(g_FileSystemError, pMsg, marker);
  va_end(marker);

  Warning("%s\n", g_FileSystemError);

  // Run vconfig?
  // Don't do it if they specifically asked for it not to, or if they manually
  // specified a vconfig with -game or -vproject.
  if (bRunVConfig && g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG &&
      !CommandLine()->FindParm(CMDLINEOPTION_NOVCONFIG) &&
      !GetVProjectCmdLineValue()) {
    LaunchVConfig();
  }

  if (g_FileSystemErrorMode == FS_ERRORMODE_AUTO ||
      g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG) {
    Error("%s\n", g_FileSystemError);
  }

  return retVal;
}

FSReturnCode_t LoadGameInfoFile(const ch *pDirectoryName, KeyValues *&pMainFile,
                                KeyValues *&pFileSystemInfo,
                                KeyValues *&pSearchPaths) {
  // If GameInfo.txt exists under pBaseDir, then this is their game directory.
  // All the filesystem mappings will be in this file.
  ch gameinfoFilename[SOURCE_MAX_PATH];
  strcpy_s(gameinfoFilename, pDirectoryName);

  Q_AppendSlash(gameinfoFilename, std::size(gameinfoFilename));
  strcat_s(gameinfoFilename, GAMEINFO_FILENAME);
  Q_FixSlashes(gameinfoFilename);

  KeyValues *kv = ReadKeyValuesFile(gameinfoFilename);
  if (!kv) {
    return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                "%s is missing.", gameinfoFilename);
  }

  KeyValues *kvfs = kv->FindKey("FileSystem");
  if (!kvfs) {
    kv->deleteThis();
    return SetupFileSystemError(true, FS_INVALID_GAMEINFO_FILE,
                                "%s is not a valid format.", gameinfoFilename);
  }

  // Now read in all the search paths.
  pSearchPaths = kvfs->FindKey("SearchPaths");
  if (!pSearchPaths) {
    kv->deleteThis();
    return SetupFileSystemError(true, FS_INVALID_GAMEINFO_FILE,
                                "%s is not a valid format.", gameinfoFilename);
  }

  pMainFile = kv;
  pFileSystemInfo = kvfs;

  return FS_OK;
}

static void FileSystem_AddLoadedSearchPath(CFSSearchPathsInit &initInfo,
                                           const ch *pPathID,
                                           bool *bFirstGamePath,
                                           const ch *pBaseDir,
                                           const ch *pLocation) {
  ch fullLocationPath[SOURCE_MAX_PATH];
  Q_MakeAbsolutePath(fullLocationPath, std::size(fullLocationPath), pLocation,
                     pBaseDir);

  // Now resolve any ./'s.
  V_FixSlashes(fullLocationPath);
  if (!V_RemoveDotSlashes(fullLocationPath))
    Error("FileSystem_AddLoadedSearchPath - Can't resolve pathname for '%s'",
          fullLocationPath);

  // Add language, mod, and gamebin search paths automatically.
  if (_stricmp(pPathID, "game") == 0) {
    // add the language path
    if (initInfo.m_pLanguage) {
      AddLanguageGameDir(initInfo.m_pFileSystem, fullLocationPath,
                         initInfo.m_pLanguage);
    }

    if (CommandLine()->FindParm("-tempcontent") != 0) {
      ch szPath[SOURCE_MAX_PATH];
      sprintf_s(szPath, "%s_tempcontent", fullLocationPath);
      initInfo.m_pFileSystem->AddSearchPath(szPath, pPathID, PATH_ADD_TO_TAIL);
    }

    // mark the first "game" dir as the "MOD" dir
    if (*bFirstGamePath) {
      *bFirstGamePath = false;
      initInfo.m_pFileSystem->AddSearchPath(fullLocationPath, "MOD",
                                            PATH_ADD_TO_TAIL);
      strcpy_s(initInfo.m_ModPath, fullLocationPath);
    }

    // add the game bin
    AddGameBinDir(initInfo.m_pFileSystem, fullLocationPath);
  }

  initInfo.m_pFileSystem->AddSearchPath(fullLocationPath, pPathID,
                                        PATH_ADD_TO_TAIL);
}

bool FileSystem_IsHldsUpdateToolDedicatedServer() {
  // To determine this, we see if the directory our executable was launched from
  // is "orangebox". We only are under "orangebox" if we're run from
  // hldsupdatetool.
  ch baseDir[SOURCE_MAX_PATH];
  if (!FileSystem_GetBaseDir(baseDir)) return false;

  V_FixSlashes(baseDir);
  V_StripTrailingSlash(baseDir);
  const ch *pLastDir = V_UnqualifiedFileName(baseDir);

  return pLastDir && _stricmp(pLastDir, "orangebox") == 0;
}

FSReturnCode_t FileSystem_LoadSearchPaths(CFSSearchPathsInit &initInfo) {
  if (!initInfo.m_pFileSystem || !initInfo.m_pDirectoryName)
    return SetupFileSystemError(
        false, FS_INVALID_PARAMETERS,
        "FileSystem_LoadSearchPaths: Invalid parameters specified.");

  KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
  FSReturnCode_t retVal = LoadGameInfoFile(initInfo.m_pDirectoryName, pMainFile,
                                           pFileSystemInfo, pSearchPaths);
  if (retVal != FS_OK) return retVal;

  // All paths except those marked with |gameinfo_path| are relative to the base
  // dir.
  ch baseDir[SOURCE_MAX_PATH];
  if (!FileSystem_GetBaseDir(baseDir))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetBaseDir failed.");

  initInfo.m_ModPath[0] = 0;

#define GAMEINFOPATH_TOKEN "|gameinfo_path|"
#define BASESOURCEPATHS_TOKEN "|all_source_engine_paths|"

  bool bFirstGamePath = true;
  usize game_info_path_token_len = strlen(GAMEINFOPATH_TOKEN);
  usize base_source_paths_token_len = strlen(BASESOURCEPATHS_TOKEN);

  for (KeyValues *pCur = pSearchPaths->GetFirstValue(); pCur;
       pCur = pCur->GetNextValue()) {
    const ch *pPathID = pCur->GetName();
    const ch *pLocation = pCur->GetString();

    if (Q_stristr(pLocation, GAMEINFOPATH_TOKEN) == pLocation) {
      pLocation += game_info_path_token_len;
      FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                     initInfo.m_pDirectoryName, pLocation);
    } else if (Q_stristr(pLocation, BASESOURCEPATHS_TOKEN) == pLocation) {
      // This is a special identifier that tells it to add the specified path
      // for all source engine versions equal to or prior to this version. So in
      // Orange Box, if they specified:
      //		|all_source_engine_paths|hl2
      // it would add the ep2\hl2 folder and the base (ep1-era) hl2 folder.
      //
      // We need a special identifier in the gameinfo.txt here because the base
      // hl2 folder exists in different places. In the case of a game or a
      // Steam-launched dedicated server, all the necessary prior engine content
      // is mapped in with the Steam depots, so we can just use the path as-is.

      // In the case of an hldsupdatetool dedicated server, the base hl2 folder
      // is "..\..\hl2" (since we're up in the 'orangebox' folder).

      pLocation += base_source_paths_token_len;

      // Add the Orange-box path (which also will include whatever the depots
      // mapped in as well if we're running a Steam-launched app).
      FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                     baseDir, pLocation);

      if (FileSystem_IsHldsUpdateToolDedicatedServer()) {
        // If we're using the hldsupdatetool dedicated server, then go up a
        // directory to get the ep1-era files too.
        ch ep1EraPath[SOURCE_MAX_PATH];
        sprintf_s(ep1EraPath, "..%c%s", CORRECT_PATH_SEPARATOR, pLocation);
        FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                       baseDir, ep1EraPath);
      }
    } else {
      FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                     baseDir, pLocation);
    }
  }

  pMainFile->deleteThis();

  // Also, mark specific path IDs as "by request only". That way, we won't waste
  // time searching in them when people forget to specify a search path.
  initInfo.m_pFileSystem->MarkPathIDByRequestOnly("executable_path", true);
  initInfo.m_pFileSystem->MarkPathIDByRequestOnly("gamebin", true);
  initInfo.m_pFileSystem->MarkPathIDByRequestOnly("mod", true);
  if (initInfo.m_ModPath[0] != 0) {
    // Add the write path last.
    initInfo.m_pFileSystem->AddSearchPath(
        initInfo.m_ModPath, "DEFAULT_WRITE_PATH", PATH_ADD_TO_TAIL);
  }

#if !defined(NDEBUG)
  initInfo.m_pFileSystem->PrintSearchPaths();
#endif

  return FS_OK;
}

bool DoesFileExistIn(const ch *pDirectoryName, const ch *pFilename) {
  ch filename[SOURCE_MAX_PATH];

  strcpy_s(filename, pDirectoryName);
  Q_AppendSlash(filename, std::size(filename));
  strcat_s(filename, pFilename);
  Q_FixSlashes(filename);

  return _access(filename, 0) == 0;
}

namespace {
SuggestGameInfoDirFn_t &GetSuggestGameInfoDirFn() {
  static SuggestGameInfoDirFn_t s_pfnSuggestGameInfoDir = nullptr;
  return s_pfnSuggestGameInfoDir;
}
};  // namespace

SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn(SuggestGameInfoDirFn_t new_fn) {
  return std::exchange(GetSuggestGameInfoDirFn(), new_fn);
}

template <usize outDirLen>
static FSReturnCode_t TryLocateGameInfoFile(ch (&pOutDir)[outDirLen],
                                            bool bBubbleDir) {
  // Retain a copy of suggested path for further attempts
  CArrayAutoPtr<ch> copy_name_buffer(new ch[outDirLen]);
  strcpy_s(copy_name_buffer.Get(), outDirLen, pOutDir);

  // Make appropriate slashes ('/' - Linux style)
  for (ch *pchFix = copy_name_buffer.Get(), *pchEnd = pchFix + outDirLen;
       pchFix < pchEnd; ++pchFix) {
    if ('\\' == *pchFix) {
      *pchFix = '/';
    }
  }

  // Have a look in supplied path
  do {
    if (DoesFileExistIn(pOutDir, GAMEINFO_FILENAME)) {
      return FS_OK;
    }
  } while (bBubbleDir && Q_StripLastDir(pOutDir, outDirLen));

  // Make an attempt to resolve from "content -> game" directory
  strcpy_s(pOutDir, copy_name_buffer.Get());

  if (ch *pchContentFix = Q_stristr(pOutDir, "/content/")) {
    sprintf_s(pchContentFix, outDirLen - (pchContentFix - pOutDir), "/game/");
    memmove(pchContentFix + 6, pchContentFix + 9,
            pOutDir + outDirLen - (pchContentFix + 9));

    // Try in the mapped "game" directory
    do {
      if (DoesFileExistIn(pOutDir, GAMEINFO_FILENAME)) {
        return FS_OK;
      }
    } while (bBubbleDir && Q_StripLastDir(pOutDir, outDirLen));
  }

  // Could not find it here
  return FS_MISSING_GAMEINFO_FILE;
}

template <usize out_dir_size>
FSReturnCode_t LocateGameInfoFile(const CFSSteamSetupInfo &fsInfo,
                                  ch (&pOutDir)[out_dir_size]) {
  // Engine and Hammer don't want to search around for it.
  if (fsInfo.m_bOnlyUseDirectoryName) {
    if (!fsInfo.m_pDirectoryName)
      return SetupFileSystemError(
          false, FS_MISSING_GAMEINFO_FILE,
          "bOnlyUseDirectoryName=1 and pDirectoryName=nullptr.");

    bool bExists = DoesFileExistIn(fsInfo.m_pDirectoryName, GAMEINFO_FILENAME);
    if (!bExists) {
      return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                  "Setup file '%s' doesn't exist in "
                                  "subdirectory '%s'.\nCheck your -game "
                                  "parameter or VCONFIG setting.",
                                  GAMEINFO_FILENAME, fsInfo.m_pDirectoryName);
    }

    strcpy_s(pOutDir, fsInfo.m_pDirectoryName);
    return FS_OK;
  }

  // First, check for overrides on the command line or environment variables.
  const ch *pProject = GetVProjectCmdLineValue();

  if (pProject) {
    if (DoesFileExistIn(pProject, GAMEINFO_FILENAME)) {
      Q_MakeAbsolutePath(pOutDir, out_dir_size, pProject);
      return FS_OK;
    }

    if (fsInfo.m_bNoGameInfo) {
      // fsInfo.m_bNoGameInfo is set by the Steam dedicated server, before it
      // knows which mod to use. Steam dedicated server doesn't need a
      // gameinfo.txt, because we'll ask which mod to use, even if -game is
      // supplied on the command line.
      strcpy_s(pOutDir, "");
      return FS_OK;
    }

    // They either specified vproject on the command line or it's in their
    // registry. Either way, we don't want to continue if they've specified it
    // but it's not valid.
    goto ShowError;
  }

  if (fsInfo.m_bNoGameInfo) {
    strcpy_s(pOutDir, "");
    return FS_OK;
  }

  // Ask the application if it can provide us with a game info directory
  {
    bool should_bubble_dir{true};
    const SuggestGameInfoDirFn_t suggest_gaminfo_dir_fn{
        GetSuggestGameInfoDirFn()};
    if (suggest_gaminfo_dir_fn &&
        (*suggest_gaminfo_dir_fn)(&fsInfo, pOutDir, out_dir_size,
                                  &should_bubble_dir) &&
        FS_OK == TryLocateGameInfoFile(pOutDir, should_bubble_dir)) {
      return FS_OK;
    }
  }

  // Check their registry.
  ch env_token[SOURCE_MAX_PATH];
  size_t env_token_size;

  // Try to use the environment variable / registry
  if (!getenv_s(&env_token_size, env_token, GAMEDIR_TOKEN) &&
      env_token_size > 0 &&
      (Q_MakeAbsolutePath(pOutDir, out_dir_size, env_token), 1) &&
      FS_OK == TryLocateGameInfoFile(pOutDir, false)) {
    return FS_OK;
  }

  Warning("Warning: falling back to auto detection of vproject directory.\n");

  // Now look for it in the directory they passed in.
  Q_MakeAbsolutePath(pOutDir, out_dir_size,
                     fsInfo.m_pDirectoryName ? fsInfo.m_pDirectoryName : ".");

  if (FS_OK == TryLocateGameInfoFile(pOutDir, true)) return FS_OK;

  // Use the CWD
  if (!Q_getwd(pOutDir))
    return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                "Unable to get current directory.\n");

  if (FS_OK == TryLocateGameInfoFile(pOutDir, true)) return FS_OK;

ShowError:
  return SetupFileSystemError(
      true, FS_MISSING_GAMEINFO_FILE,
      "Unable to find %s. Solutions:\n\n"
      "1. Read http://www.valve-erc.com/srcsdk/faq.html#NoGameDir\n"
      "2. Run vconfig to specify which game you're working on.\n"
      "3. Add -game <path> on the command line where <path> is the directory "
      "that %s is in.\n",
      GAMEINFO_FILENAME, GAMEINFO_FILENAME);
}

bool DoesPathExistAlready(const ch *pPathEnvVar, const ch *pTestPath) {
  // Fix the slashes in the input arguments.
  ch correctedPathEnvVar[8192], correctedTestPath[SOURCE_MAX_PATH];
  strcpy_s(correctedPathEnvVar, pPathEnvVar);
  Q_FixSlashes(correctedPathEnvVar);
  pPathEnvVar = correctedPathEnvVar;

  strcpy_s(correctedTestPath, pTestPath);
  Q_FixSlashes(correctedTestPath);

  const usize correctedTestPathLength = strlen(correctedTestPath);
  if (correctedTestPathLength > 0 &&
      PATHSEPARATOR(correctedTestPath[correctedTestPathLength - 1]))
    correctedTestPath[correctedTestPathLength - 1] = 0;

  pTestPath = correctedTestPath;
  usize test_path_len = strlen(pTestPath);

  const ch *pCurPos = pPathEnvVar;
  while (true) {
    const ch *pTestPos = Q_stristr(pCurPos, pTestPath);
    if (!pTestPos) return false;

    // Ok, we found pTestPath in the path, but it's only valid if it's followed
    // by an optional slash and a semicolon.
    pTestPos += test_path_len;
    if (pTestPos[0] == 0 || pTestPos[0] == ';' ||
        (PATHSEPARATOR(pTestPos[0]) && pTestPos[1] == ';'))
      return true;

    // Advance our marker..
    pCurPos = pTestPos;
  }
}

template <usize steamInstallPathLen>
FSReturnCode_t SetSteamInstallPath(ch (&steamInstallPath)[steamInstallPathLen],
                                   SteamEnvVariables &steamEnvVars,
                                   bool bErrorsAsWarnings) {
  // Start at our bin directory and move up until we find a directory with
  // steam.dll in it.
  ch exe_path[SOURCE_MAX_PATH];
  if (!FileSystem_GetExecutableDir(exe_path)) {
    if (bErrorsAsWarnings) {
      Warning("SetSteamInstallPath: FileSystem_GetExecutableDir failed.\n");
      return FS_INVALID_PARAMETERS;
    }

    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");
  }

  strcpy_s(steamInstallPath, exe_path);
  if (!Q_StripLastDir(steamInstallPath, steamInstallPathLen)) {
    if (bErrorsAsWarnings) {
      Warning("Can't find steam.dll relative to executable path: %s.\n",
              exe_path);
      return FS_MISSING_STEAM_DLL;
    }

    return SetupFileSystemError(
        false, FS_MISSING_STEAM_DLL,
        "Can't find steam.dll relative to executable path: %s.", exe_path);
  }

  // Also, add the install path to their PATH environment variable, so
  // filesystem_steam.dll can get to steam.dll.
  ch szPath[8192];
  steamEnvVars.Path.GetValue(szPath);

  if (!DoesPathExistAlready(szPath, steamInstallPath)) {
    steamEnvVars.Path.SetValue("%s;%s", szPath, steamInstallPath);
  }

  return FS_OK;
}

template <usize steamCfgPathLen>
FSReturnCode_t GetSteamCfgPath(ch (&steamCfgPath)[steamCfgPathLen]) {
  steamCfgPath[0] = 0;
  ch executablePath[SOURCE_MAX_PATH];

  if (!FileSystem_GetExecutableDir(executablePath)) {
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");
  }

  strcpy_s(steamCfgPath, executablePath);
  while (true) {
    if (DoesFileExistIn(steamCfgPath, "steam.cfg")) break;

    if (!Q_StripLastDir(steamCfgPath, steamCfgPathLen)) {
      // the file isnt found, thats ok, its not mandatory
      return FS_OK;
    }
  }
  Q_AppendSlash(steamCfgPath, steamCfgPathLen);
  strcat_s(steamCfgPath, "steam.cfg");

  return FS_OK;
}

void SetSteamAppUser(KeyValues *pSteamInfo, const ch *steamInstallPath,
                     SteamEnvVariables &steamEnvVars) {
  // Always inherit the Steam user if it's already set, since it probably means
  // we (or the the app that launched us) were launched from Steam.
  ch appUser[SOURCE_MAX_PATH];
  if (steamEnvVars.SteamAppUser.GetValue(appUser)) return;

  const ch *pTempAppUser = nullptr;
  if (pSteamInfo && (pTempAppUser = pSteamInfo->GetString(
                         "SteamAppUser", nullptr)) != nullptr) {
    strcpy_s(appUser, pTempAppUser);
  } else {
    // They don't have SteamInfo.txt, or it's missing SteamAppUser. Try to
    // figure out the user by looking in <steam install
    // path>\config\SteamAppData.vdf.
    ch fullFilename[SOURCE_MAX_PATH];
    strcpy_s(fullFilename, steamInstallPath);
    Q_AppendSlash(fullFilename, sizeof(fullFilename));
    strcat_s(fullFilename, "config\\SteamAppData.vdf");

    KeyValues *pSteamAppData = ReadKeyValuesFile(fullFilename);
    if (!pSteamAppData || (pTempAppUser = pSteamAppData->GetString(
                               "AutoLoginUser", nullptr)) == nullptr) {
      Error("Can't find steam app user info.");
    }
    strcpy_s(appUser, pTempAppUser);

    if (pSteamAppData) pSteamAppData->deleteThis();
  }

  Q_strlower(appUser);
  steamEnvVars.SteamAppUser.SetValue("%s", appUser);
}

void SetSteamUserPassphrase(KeyValues *pSteamInfo,
                            SteamEnvVariables &steamEnvVars) {
  // Always inherit the passphrase if it's already set, since it probably means
  // we (or the the app that launched us) were launched from Steam.
  ch szPassPhrase[SOURCE_MAX_PATH];
  if (steamEnvVars.SteamUserPassphrase.GetValue(szPassPhrase)) return;

  // SteamUserPassphrase.
  const ch *pStr;
  if (pSteamInfo && (pStr = pSteamInfo->GetString("SteamUserPassphrase",
                                                  nullptr)) != nullptr) {
    steamEnvVars.SteamUserPassphrase.SetValue("%s", pStr);
  }
}

void SetSteamAppId(KeyValues *pFileSystemInfo, const ch *pGameInfoDirectory,
                   SteamEnvVariables &steamEnvVars) {
  // SteamAppId is in
  // gameinfo.txt->FileSystem->FileSystemInfo_Steam->SteamAppId.
  i32 iAppId = pFileSystemInfo->GetInt("SteamAppId", -1);
  if (iAppId == -1)
    Error("Missing SteamAppId in %s\\%s.", pGameInfoDirectory,
          GAMEINFO_FILENAME);

  steamEnvVars.SteamAppId.SetValue("%d", iAppId);
}

FSReturnCode_t SetupSteamStartupEnvironment(KeyValues *pFileSystemInfo,
                                            const ch *pGameInfoDirectory,
                                            SteamEnvVariables &steamEnvVars) {
  // Ok, we're going to run Steam. See if they have SteamInfo.txt. If not, we'll
  // try to deduce what we can.
  ch steamInfoFile[SOURCE_MAX_PATH];
  strcpy_s(steamInfoFile, pGameInfoDirectory);

  Q_AppendSlash(steamInfoFile, sizeof(steamInfoFile));
  strcat_s(steamInfoFile, "steaminfo.txt");
  KeyValues *pSteamInfo = ReadKeyValuesFile(steamInfoFile);

  ch steamInstallPath[SOURCE_MAX_PATH];
  FSReturnCode_t ret =
      SetSteamInstallPath(steamInstallPath, steamEnvVars, false);
  if (ret != FS_OK) return ret;

  SetSteamAppUser(pSteamInfo, steamInstallPath, steamEnvVars);
  SetSteamUserPassphrase(pSteamInfo, steamEnvVars);
  SetSteamAppId(pFileSystemInfo, pGameInfoDirectory, steamEnvVars);

  if (pSteamInfo) pSteamInfo->deleteThis();

  return FS_OK;
}

FSReturnCode_t GetSteamExtraAppId(const ch *pDirectoryName, i32 *nExtraAppId) {
  // Now, load gameinfo.txt (to make sure it's there)
  KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
  FSReturnCode_t ret = LoadGameInfoFile(pDirectoryName, pMainFile,
                                        pFileSystemInfo, pSearchPaths);
  if (ret != FS_OK) return ret;

  *nExtraAppId = pFileSystemInfo->GetInt("ToolsAppId", -1);
  pMainFile->deleteThis();
  return FS_OK;
}

FSReturnCode_t FileSystem_SetBasePaths(IFileSystem *pFileSystem) {
  pFileSystem->RemoveSearchPaths("EXECUTABLE_PATH");

  ch executablePath[SOURCE_MAX_PATH];
  if (!FileSystem_GetExecutableDir(executablePath))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");

  pFileSystem->AddSearchPath(executablePath, "EXECUTABLE_PATH");
  return FS_OK;
}

// Returns the name of the file system DLL to use
FSReturnCode_t FileSystem_GetFileSystemDLLName(ch *pFileSystemDLL,
                                               usize nMaxLen, bool &bSteam) {
  bSteam = false;

  // Inside of here, we don't have a filesystem yet, so we have to assume that
  // the filesystem_stdio is in this same directory with us.
  ch exe_path[SOURCE_MAX_PATH];
  if (!FileSystem_GetExecutableDir(exe_path))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");

#ifdef OS_WIN
  sprintf_s(pFileSystemDLL, nMaxLen, "%s%cfilesystem_stdio.dll", exe_path,
            CORRECT_PATH_SEPARATOR);
#elif defined(OS_POSIX)
  sprintf_s(pFileSystemDLL, nMaxLen, "%s%cfilesystem.so.1", executablePath,
            CORRECT_PATH_SEPARATOR);
#else
#error Please, define a filesystem dll name
#endif

  return FS_OK;
}

// Sets up the steam.dll install path in our PATH env var (so you can then just
// LoadLibrary() on filesystem_steam.dll without having to copy steam.dll
// anywhere special)
FSReturnCode_t FileSystem_SetupSteamInstallPath() {
  SteamEnvVariables steamEnvVars;
  ch steamInstallPath[SOURCE_MAX_PATH];
  FSReturnCode_t ret =
      SetSteamInstallPath(steamInstallPath, steamEnvVars, true);
  // We want to keep the change to the path going forward.
  steamEnvVars.Path.SetRestoreOriginalValue(false);
  return ret;
}

// Sets up the steam environment + gets back the gameinfo.txt path
FSReturnCode_t FileSystem_SetupSteamEnvironment(CFSSteamSetupInfo &fsInfo) {
  // First, locate the directory with gameinfo.txt.
  FSReturnCode_t ret = LocateGameInfoFile(fsInfo, fsInfo.m_GameInfoPath);
  if (ret != FS_OK) return ret;

  // This is so that processes spawned by this application will have the same
  // VPROJECT
  ch env[SOURCE_MAX_PATH + 32];
  sprintf_s(env, "%s=%s", GAMEDIR_TOKEN, fsInfo.m_GameInfoPath);
  if (_putenv(env) != 0) {
    return SetupFileSystemError(
        true, FS_UNABLE_TO_INIT,
        "Unable to set " GAMEDIR_TOKEN
        " environment variable to %s in the file system",
        fsInfo.m_GameInfoPath);
  }

  SteamEnvVariables steamEnvVars;
  if (fsInfo.m_bSteam) {
    if (fsInfo.m_bToolsMode) {
      // Now, load gameinfo.txt (to make sure it's there)
      KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
      ret = LoadGameInfoFile(fsInfo.m_GameInfoPath, pMainFile, pFileSystemInfo,
                             pSearchPaths);
      if (ret != FS_OK) return ret;

      // If filesystem_stdio.dll is missing or -steam is specified, then load
      // filesystem_steam.dll. There are two command line parameters for Steam:
      //		1) -steam (runs Steam in remote filesystem mode;
      // requires Steam backend) 		2) -steamlocal (runs Steam in
      // local filesystem mode (all content off HDD)

      // Setup all the environment variables related to Steam so
      // filesystem_steam.dll knows how to initialize Steam.
      ret = SetupSteamStartupEnvironment(pFileSystemInfo, fsInfo.m_GameInfoPath,
                                         steamEnvVars);
      if (ret != FS_OK) return ret;

      steamEnvVars.SteamAppId.SetRestoreOriginalValue(
          false);  // We want to keep the change to the path going forward.

      // We're done with main file
      pMainFile->deleteThis();
    } else if (fsInfo.m_bSetSteamDLLPath) {
      // This is used by the engine to automatically set the path to their
      // steam.dll when running the engine, so they can debug it without having
      // to copy steam.dll up into their hl2.exe folder.
      ch steamInstallPath[SOURCE_MAX_PATH];
      ret = SetSteamInstallPath(steamInstallPath, steamEnvVars, true);
      steamEnvVars.Path.SetRestoreOriginalValue(
          false);  // We want to keep the change to the path going forward.
    }
  }

  return FS_OK;
}

// Loads the file system module
FSReturnCode_t FileSystem_LoadFileSystemModule(CFSLoadModuleInfo &fsInfo) {
  // First, locate the directory with gameinfo.txt.
  FSReturnCode_t ret = FileSystem_SetupSteamEnvironment(fsInfo);
  if (ret != FS_OK) return ret;

  // Now that the environment is setup, load the filesystem module.
  if (!Sys_LoadInterface(fsInfo.m_pFileSystemDLLName,
                         FILESYSTEM_INTERFACE_VERSION, &fsInfo.m_pModule,
                         (void **)&fsInfo.m_pFileSystem)) {
    return SetupFileSystemError(false, FS_UNABLE_TO_INIT, "Can't load %s.",
                                fsInfo.m_pFileSystemDLLName);
  }

  if (!fsInfo.m_pFileSystem->Connect(fsInfo.m_ConnectFactory))
    return SetupFileSystemError(false, FS_UNABLE_TO_INIT,
                                "%s IFileSystem::Connect failed.",
                                fsInfo.m_pFileSystemDLLName);

  if (fsInfo.m_pFileSystem->Init() != INIT_OK)
    return SetupFileSystemError(false, FS_UNABLE_TO_INIT,
                                "%s IFileSystem::Init failed.",
                                fsInfo.m_pFileSystemDLLName);

  return FS_OK;
}

// Mounds a particular steam cache
FSReturnCode_t FileSystem_MountContent(CFSMountContentInfo &mountContentInfo) {
  // This part is Steam-only.
  if (mountContentInfo.m_pFileSystem->IsSteam()) {
    // Find out the "extra app id". This is for tools, which want to mount a
    // base app's filesystem like HL2, then mount the SDK content (tools
    // materials and models, etc) in addition.
    i32 nExtraAppId = -1;
    if (mountContentInfo.m_bToolsMode) {
      FSReturnCode_t ret =
          GetSteamExtraAppId(mountContentInfo.m_pDirectoryName, &nExtraAppId);
      if (ret != FS_OK) return ret;
    }

    // Set our working directory temporarily so Steam can remember it.
    // This is what Steam strips off absolute filenames like c:\program
    // files\valve\steam\steamapps\username\sourcesdk to get to the relative
    // part of the path.
    ch baseDir[SOURCE_MAX_PATH], oldWorkingDir[SOURCE_MAX_PATH];
    if (!FileSystem_GetBaseDir(baseDir))
      return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                  "FileSystem_GetBaseDir failed.");

    if (!Q_getwd(oldWorkingDir)) {
      return SetupFileSystemError(true, FS_UNABLE_TO_INIT,
                                  "Unable to get current directory.\n");
    }

    if (_chdir(baseDir) != 0) {
      return SetupFileSystemError(true, FS_UNABLE_TO_INIT,
                                  "Unable to set current directory to %s.\n",
                                  baseDir);
    }

    // Filesystem_tools needs to add dependencies in here beforehand.
    FilesystemMountRetval_t retVal =
        mountContentInfo.m_pFileSystem->MountSteamContent(nExtraAppId);

    if (_chdir(oldWorkingDir) != 0) {
      return SetupFileSystemError(true, FS_UNABLE_TO_INIT,
                                  "Unable to set current directory to %s.\n",
                                  oldWorkingDir);
    }

    if (retVal != FILESYSTEM_MOUNT_OK)
      return SetupFileSystemError(
          true, FS_UNABLE_TO_INIT,
          "Unable to mount Steam content in the file system");
  }

  return FileSystem_SetBasePaths(mountContentInfo.m_pFileSystem);
}

void FileSystem_ClearSteamEnvVars() {
  SteamEnvVariables envVars;

  // Change the values and don't restore the originals.
  envVars.SteamAppId.SetValue("");
  envVars.SteamUserPassphrase.SetValue("");
  envVars.SteamAppUser.SetValue("");

  envVars.SetRestoreOriginalValue_ALL(false);
}

// Adds the platform folder to the search path.
void FileSystem_AddSearchPath_Platform(IFileSystem *pFileSystem,
                                       const ch *szGameInfoPath) {
  ch platform[SOURCE_MAX_PATH];
  if (pFileSystem->IsSteam()) {
    // Steam doesn't support relative paths
    strcpy_s(platform, "platform");
  } else {
    strcpy_s(platform, szGameInfoPath);
    Q_StripTrailingSlash(platform);
    strcat_s(platform, "/../platform");
  }

  pFileSystem->AddSearchPath(platform, "PLATFORM");
}
