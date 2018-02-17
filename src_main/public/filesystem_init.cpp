// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#undef PROTECTED_THINGS_ENABLE
#undef PROTECT_FILEIO_FUNCTIONS

#ifndef _LINUX
#undef fopen
#endif

#include "filesystem_init.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <cstdio>
#include "base/include/windows/windows_light.h"
#elif _LINUX
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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

#define GAMEINFO_FILENAME "gameinfo.txt"
#define GAMEINFO_FILENAME_ALTERNATE "gameinfo.txt"

static char g_FileSystemError[256];
static bool s_bUseVProjectBinDir = false;
static FSErrorMode_t g_FileSystemErrorMode = FS_ERRORMODE_VCONFIG;

// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir(bool bEnable) {
  s_bUseVProjectBinDir = bEnable;
}

// This class lets you modify environment variables, and it restores the
// original value when it goes out of scope.
class CTempEnvVar {
 public:
  CTempEnvVar(const char *pVarName) {
    m_bRestoreOriginalValue = true;
    m_pVarName = pVarName;

    const char *pValue = NULL;

#ifdef _WIN32
    // Use GetEnvironmentVariable instead of getenv because getenv doesn't pick
    // up changes to the process environment after the DLL was loaded.
    char szBuf[4096];
    if (GetEnvironmentVariable(m_pVarName, szBuf, sizeof(szBuf)) != 0) {
      pValue = szBuf;
    }
#else
    // LINUX BUG: see above
    pValue = getenv(pVarName);
#endif

    if (pValue) {
      m_bExisted = true;
      m_OriginalValue.SetSize(strlen(pValue) + 1);
      memcpy(m_OriginalValue.Base(), pValue, m_OriginalValue.Count());
    } else {
      m_bExisted = false;
    }
  }

  ~CTempEnvVar() {
    if (m_bRestoreOriginalValue) {
      // Restore the original value.
      if (m_bExisted) {
        SetValue("%s", m_OriginalValue.Base());
      } else {
        ClearValue();
      }
    }
  }

  void SetRestoreOriginalValue(bool bRestore) {
    m_bRestoreOriginalValue = bRestore;
  }

  int GetValue(char *pszBuf, int nBufSize) {
    if (!pszBuf || (nBufSize <= 0)) return 0;

#ifdef _WIN32
    // Use GetEnvironmentVariable instead of getenv because getenv doesn't pick
    // up changes to the process environment after the DLL was loaded.
    return GetEnvironmentVariable(m_pVarName, pszBuf, nBufSize);
#else
    // LINUX BUG: see above
    const char *pszOut = getenv(m_pVarName);
    if (!pszOut) {
      *pszBuf = '\0';
      return 0;
    }

    Q_strncpy(pszBuf, pszOut, nBufSize);
    return Q_strlen(pszBuf);
#endif
  }

  bool SetValue(const char *pValue, ...) {
    char valueString[4096];
    va_list marker;
    va_start(marker, pValue);
    Q_vsnprintf(valueString, sizeof(valueString), pValue, marker);
    va_end(marker);

    char str[4096];
    Q_snprintf(str, sizeof(str), "%s=%s", m_pVarName, valueString);
    return _putenv(str) == 0;
  }

  bool ClearValue() {
    char str[512];
    Q_snprintf(str, sizeof(str), "%s=", m_pVarName);
    return _putenv(str) == 0;
  }

 private:
  bool m_bRestoreOriginalValue;
  const char *m_pVarName;
  bool m_bExisted;
  CUtlVector<char> m_OriginalValue;
};

class CSteamEnvVars {
 public:
  CSteamEnvVars()
      : m_SteamAppId("SteamAppId"),
        m_SteamUserPassphrase("SteamUserPassphrase"),
        m_SteamAppUser("SteamAppUser"),
        m_Path("path") {}

  void SetRestoreOriginalValue_ALL(bool bRestore) {
    m_SteamAppId.SetRestoreOriginalValue(bRestore);
    m_SteamUserPassphrase.SetRestoreOriginalValue(bRestore);
    m_SteamAppUser.SetRestoreOriginalValue(bRestore);
    m_Path.SetRestoreOriginalValue(bRestore);
  }

  CTempEnvVar m_SteamAppId;
  CTempEnvVar m_SteamUserPassphrase;
  CTempEnvVar m_SteamAppUser;
  CTempEnvVar m_Path;
};

// ----------------------------------------------------------------------------------------------------
// // Helpers.
// ----------------------------------------------------------------------------------------------------
// //
bool Q_getwd(char *out, int outSize) {
#if defined(_WIN32)
  char *cwd = _getcwd(out, outSize);
  if (cwd) Q_strncat(out, "\\", outSize, COPY_ALL_CHARACTERS);
#else
  char *cwd = getcwd(out, outSize);
  if (cwd) strcat(out, "/");
#endif
  if (cwd) Q_FixSlashes(out);
  return cwd != nullptr;
}

// ----------------------------------------------------------------------------------------------------
// // Module interface.
// ----------------------------------------------------------------------------------------------------
// //

CFSSearchPathsInit::CFSSearchPathsInit() {
  m_pDirectoryName = nullptr;
  m_pLanguage = nullptr;
  m_pFileSystem = nullptr;
  m_ModPath[0] = 0;
}

CFSSteamSetupInfo::CFSSteamSetupInfo() {
  m_pDirectoryName = nullptr;
  m_bOnlyUseDirectoryName = false;
  m_bSteam = false;
  m_bToolsMode = true;
  m_bSetSteamDLLPath = false;
  m_bNoGameInfo = false;
  m_GameInfoPath[0] = '\0';
}

CFSLoadModuleInfo::CFSLoadModuleInfo() {
  m_pFileSystemDLLName = nullptr;
  m_ConnectFactory = nullptr;
  m_pFileSystem = nullptr;
  m_pModule = nullptr;
}

CFSMountContentInfo::CFSMountContentInfo() {
  m_bToolsMode = true;
  m_pDirectoryName = nullptr;
  m_pFileSystem = nullptr;
}

const char *FileSystem_GetLastErrorString() { return g_FileSystemError; }

void AddLanguageGameDir(IFileSystem *pFileSystem, const char *pLocation,
                        const char *pLanguage) {
#if !defined(SWDS)
  char temp[MAX_PATH];
  Q_snprintf(temp, sizeof(temp), "%s_%s", pLocation, pLanguage);
  pFileSystem->AddSearchPath(temp, "GAME", PATH_ADD_TO_TAIL);

  if (!pFileSystem->IsSteam()) {
    // also look in "..\localization\<folder>" if not running Steam
    char baseDir[MAX_PATH];
    Q_strncpy(baseDir, pLocation, sizeof(baseDir));
    char *tempPtr = Q_strstr(baseDir, "\\game\\");

    if (tempPtr) {
      char *gameDir = tempPtr + Q_strlen("\\game\\");
      *tempPtr = 0;
      Q_snprintf(temp, sizeof(temp), "%s%clocalization%c%s_%s", baseDir,
                 CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR, gameDir,
                 pLanguage);
      pFileSystem->AddSearchPath(temp, "GAME", PATH_ADD_TO_TAIL);
    }
  }
#endif
}

void AddGameBinDir(IFileSystem *pFileSystem, const char *pLocation) {
  char temp[MAX_PATH];
  Q_snprintf(temp, sizeof(temp), "%s%cbin", pLocation, CORRECT_PATH_SEPARATOR);
  pFileSystem->AddSearchPath(temp, "GAMEBIN", PATH_ADD_TO_TAIL);
}

KeyValues *ReadKeyValuesFile(const char *pFilename) {
  // Read in the gameinfo.txt file and 0-terminate it.
  FILE *fp = fopen(pFilename, "rb");
  if (!fp) return NULL;

  CUtlVector<char> buf;
  fseek(fp, 0, SEEK_END);
  buf.SetSize(ftell(fp) + 1);
  fseek(fp, 0, SEEK_SET);
  fread(buf.Base(), 1, buf.Count() - 1, fp);

  fclose(fp);
  buf[buf.Count() - 1] = 0;

  KeyValues *kv = new KeyValues("");
  if (!kv->LoadFromBuffer(pFilename, buf.Base())) {
    kv->deleteThis();
    return NULL;  //-V773
  }

  return kv;
}

static bool Sys_GetExecutableName(char *out, int len) {
#if defined(_WIN32)
  if (!::GetModuleFileName((HINSTANCE)GetModuleHandle(NULL), out, len)) {
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

bool FileSystem_GetExecutableDir(char *exedir, int exeDirLen) {
  exedir[0] = 0;

  if (s_bUseVProjectBinDir) {
    const char *pProject = GetVProjectCmdLineValue();
    if (!pProject) {
      // Check their registry.
      pProject = getenv(GAMEDIR_TOKEN);
    }
    if (pProject) {
      Q_snprintf(exedir, exeDirLen, "%s%c..%cbin", pProject,
                 CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR);
      return true;
    }
    return false;
  }

  if (!Sys_GetExecutableName(exedir, exeDirLen)) return false;

  Q_StripFilename(exedir);
  Q_FixSlashes(exedir);

  // Return the bin directory as the executable dir if it's not in there
  // because that's really where we're running from...
  char ext[MAX_PATH];
  Q_StrRight(exedir, 4, ext, sizeof(ext));
  if (ext[0] != CORRECT_PATH_SEPARATOR || Q_stricmp(ext + 1, "bin") != 0) {
    Q_strncat(exedir, "\\bin", exeDirLen, COPY_ALL_CHARACTERS);
    Q_FixSlashes(exedir);
  }

  return true;
}

static bool FileSystem_GetBaseDir(char *baseDir, int baseDirLen) {
  if (FileSystem_GetExecutableDir(baseDir, baseDirLen)) {
    Q_StripFilename(baseDir);
    return true;
  }

  return false;
}

void LaunchVConfig() {
#if defined(_WIN32)
  char vconfigExe[MAX_PATH];
  FileSystem_GetExecutableDir(vconfigExe, ARRAYSIZE(vconfigExe));

  Q_AppendSlash(vconfigExe, ARRAYSIZE(vconfigExe));
  Q_strncat(vconfigExe, "vconfig.exe", ARRAYSIZE(vconfigExe),
            COPY_ALL_CHARACTERS);

  const char *const argv[] = {vconfigExe, "-allowdebug", nullptr};
  _spawnv(_P_NOWAIT, vconfigExe, argv);
#endif
}

const char *GetVProjectCmdLineValue() {
  return CommandLine()->ParmValue("-vproject",
                                  CommandLine()->ParmValue("-game"));
}

FSReturnCode_t SetupFileSystemError(bool bRunVConfig, FSReturnCode_t retVal,
                                    const char *pMsg, ...) {
  va_list marker;
  va_start(marker, pMsg);
  Q_vsnprintf(g_FileSystemError, ARRAYSIZE(g_FileSystemError), pMsg, marker);
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

FSReturnCode_t LoadGameInfoFile(const char *pDirectoryName,
                                KeyValues *&pMainFile,
                                KeyValues *&pFileSystemInfo,
                                KeyValues *&pSearchPaths) {
  // If GameInfo.txt exists under pBaseDir, then this is their game directory.
  // All the filesystem mappings will be in this file.
  char gameinfoFilename[MAX_PATH];
  Q_strncpy(gameinfoFilename, pDirectoryName, ARRAYSIZE(gameinfoFilename));
  Q_AppendSlash(gameinfoFilename, ARRAYSIZE(gameinfoFilename));
  Q_strncat(gameinfoFilename, GAMEINFO_FILENAME, ARRAYSIZE(gameinfoFilename),
            COPY_ALL_CHARACTERS);
  Q_FixSlashes(gameinfoFilename);

  pMainFile = ReadKeyValuesFile(gameinfoFilename);
  if (!pMainFile) {
    return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                "%s is missing.", gameinfoFilename);
  }

  pFileSystemInfo = pMainFile->FindKey("FileSystem");
  if (!pFileSystemInfo) {
    pMainFile->deleteThis();
    return SetupFileSystemError(true, FS_INVALID_GAMEINFO_FILE,
                                "%s is not a valid format.", gameinfoFilename);
  }

  // Now read in all the search paths.
  pSearchPaths = pFileSystemInfo->FindKey("SearchPaths");
  if (!pSearchPaths) {
    pMainFile->deleteThis();
    return SetupFileSystemError(true, FS_INVALID_GAMEINFO_FILE,
                                "%s is not a valid format.", gameinfoFilename);
  }
  return FS_OK;
}

// checks the registry for the low violence setting
// Check "HKEY_CURRENT_USER\Software\Valve\Source\Settings" and "User Token 2"
// or "User Token 3"
bool IsLowViolenceBuild() {
#if defined(_WIN32)
  HKEY hKey;
  char szValue[64];
  unsigned long len = ARRAYSIZE(szValue) - 1;
  bool retVal = false;

  if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Source\\Settings", NULL,
                   KEY_READ, &hKey) == ERROR_SUCCESS) {
    // User Token 2
    if (RegQueryValueEx(hKey, "User Token 2", NULL, NULL,
                        (unsigned char *)szValue, &len) == ERROR_SUCCESS) {
      if (Q_strlen(szValue) > 0) {
        retVal = true;
      }
    }

    if (!retVal) {
      // reset "len" for the next check
      len = sizeof(szValue) - 1;

      // User Token 3
      if (RegQueryValueEx(hKey, "User Token 3", NULL, NULL,
                          (unsigned char *)szValue, &len) == ERROR_SUCCESS) {
        if (Q_strlen(szValue) > 0) {
          retVal = true;
        }
      }
    }

    RegCloseKey(hKey);
  }

  return retVal;
#elif _LINUX
  return false;
#elif
#error "Fix me"
#endif
}

static void FileSystem_AddLoadedSearchPath(
    CFSSearchPathsInit &initInfo, const char *pPathID, bool *bFirstGamePath,
    const char *pBaseDir, const char *pLocation, bool bLowViolence) {
  char fullLocationPath[MAX_PATH];
  Q_MakeAbsolutePath(fullLocationPath, ARRAYSIZE(fullLocationPath), pLocation,
                     pBaseDir);

  // Now resolve any ./'s.
  V_FixSlashes(fullLocationPath);
  if (!V_RemoveDotSlashes(fullLocationPath))
    Error("FileSystem_AddLoadedSearchPath - Can't resolve pathname for '%s'",
          fullLocationPath);

  // Add language, mod, and gamebin search paths automatically.
  if (Q_stricmp(pPathID, "game") == 0) {
    // add the low violence path
    if (bLowViolence) {
      char szPath[MAX_PATH];
      Q_snprintf(szPath, ARRAYSIZE(szPath), "%s_lv", fullLocationPath);
      initInfo.m_pFileSystem->AddSearchPath(szPath, pPathID, PATH_ADD_TO_TAIL);
    }

    // add the language path
    if (initInfo.m_pLanguage) {
      AddLanguageGameDir(initInfo.m_pFileSystem, fullLocationPath,
                         initInfo.m_pLanguage);
    }

    if (CommandLine()->FindParm("-tempcontent") != 0) {
      char szPath[MAX_PATH];
      Q_snprintf(szPath, ARRAYSIZE(szPath), "%s_tempcontent", fullLocationPath);
      initInfo.m_pFileSystem->AddSearchPath(szPath, pPathID, PATH_ADD_TO_TAIL);
    }

    // mark the first "game" dir as the "MOD" dir
    if (*bFirstGamePath) {
      *bFirstGamePath = false;
      initInfo.m_pFileSystem->AddSearchPath(fullLocationPath, "MOD",
                                            PATH_ADD_TO_TAIL);
      Q_strncpy(initInfo.m_ModPath, fullLocationPath,
                ARRAYSIZE(initInfo.m_ModPath));
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
  char baseDir[MAX_PATH];
  if (!FileSystem_GetBaseDir(baseDir, ARRAYSIZE(baseDir))) return false;

  V_FixSlashes(baseDir);
  V_StripTrailingSlash(baseDir);
  const char *pLastDir = V_UnqualifiedFileName(baseDir);
  return (pLastDir && V_stricmp(pLastDir, "orangebox") == 0);
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
  char baseDir[MAX_PATH];
  if (!FileSystem_GetBaseDir(baseDir, ARRAYSIZE(baseDir)))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetBaseDir failed.");

  initInfo.m_ModPath[0] = 0;

#define GAMEINFOPATH_TOKEN "|gameinfo_path|"
#define BASESOURCEPATHS_TOKEN "|all_source_engine_paths|"

  bool bLowViolence = IsLowViolenceBuild();
  bool bFirstGamePath = true;
  size_t game_info_path_token_len = strlen(GAMEINFOPATH_TOKEN);
  size_t base_source_paths_token_len = strlen(BASESOURCEPATHS_TOKEN);

  for (KeyValues *pCur = pSearchPaths->GetFirstValue(); pCur;
       pCur = pCur->GetNextValue()) {
    const char *pPathID = pCur->GetName();
    const char *pLocation = pCur->GetString();

    if (Q_stristr(pLocation, GAMEINFOPATH_TOKEN) == pLocation) {
      pLocation += game_info_path_token_len;
      FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                     initInfo.m_pDirectoryName, pLocation,
                                     bLowViolence);
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
                                     baseDir, pLocation, bLowViolence);

      if (FileSystem_IsHldsUpdateToolDedicatedServer()) {
        // If we're using the hldsupdatetool dedicated server, then go up a
        // directory to get the ep1-era files too.
        char ep1EraPath[MAX_PATH];
        V_snprintf(ep1EraPath, sizeof(ep1EraPath), "..%c%s",
                   CORRECT_PATH_SEPARATOR, pLocation);
        FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                       baseDir, ep1EraPath, bLowViolence);
      }
    } else {
      FileSystem_AddLoadedSearchPath(initInfo, pPathID, &bFirstGamePath,
                                     baseDir, pLocation, bLowViolence);
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

#ifdef _DEBUG
  initInfo.m_pFileSystem->PrintSearchPaths();
#endif

  return FS_OK;
}

bool DoesFileExistIn(const char *pDirectoryName, const char *pFilename) {
  char filename[MAX_PATH];

  Q_strncpy(filename, pDirectoryName, ARRAYSIZE(filename));
  Q_AppendSlash(filename, ARRAYSIZE(filename));
  Q_strncat(filename, pFilename, ARRAYSIZE(filename), COPY_ALL_CHARACTERS);
  Q_FixSlashes(filename);
  const bool bExist = (_access(filename, 0) == 0);

  return bExist;
}

namespace {
SuggestGameInfoDirFn_t &GetSuggestGameInfoDirFn() {
  static SuggestGameInfoDirFn_t s_pfnSuggestGameInfoDir = NULL;
  return s_pfnSuggestGameInfoDir;
}
};  // namespace

SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn(
    SuggestGameInfoDirFn_t pfnNewFn) {
  SuggestGameInfoDirFn_t &rfn = GetSuggestGameInfoDirFn();
  SuggestGameInfoDirFn_t pfnOldFn = rfn;
  rfn = pfnNewFn;
  return pfnOldFn;
}

static FSReturnCode_t TryLocateGameInfoFile(char *pOutDir, int outDirLen,
                                            bool bBubbleDir) {
  // Retain a copy of suggested path for further attempts
  CArrayAutoPtr<char> spchCopyNameBuffer(new char[outDirLen]);
  Q_strncpy(spchCopyNameBuffer.Get(), pOutDir, outDirLen);
  spchCopyNameBuffer[outDirLen - 1] = 0;

  // Make appropriate slashes ('/' - Linux style)
  for (char *pchFix = spchCopyNameBuffer.Get(), *pchEnd = pchFix + outDirLen;
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
  Q_strncpy(pOutDir, spchCopyNameBuffer.Get(), outDirLen);
  pOutDir[outDirLen - 1] = 0;
  if (char *pchContentFix = Q_stristr(pOutDir, "/content/")) {
    sprintf(pchContentFix, "/game/");
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

FSReturnCode_t LocateGameInfoFile(const CFSSteamSetupInfo &fsInfo,
                                  char *pOutDir, int outDirLen) {
  // Engine and Hammer don't want to search around for it.
  if (fsInfo.m_bOnlyUseDirectoryName) {
    if (!fsInfo.m_pDirectoryName)
      return SetupFileSystemError(
          false, FS_MISSING_GAMEINFO_FILE,
          "bOnlyUseDirectoryName=1 and pDirectoryName=NULL.");

    bool bExists = DoesFileExistIn(fsInfo.m_pDirectoryName, GAMEINFO_FILENAME);
    if (!bExists) {
      return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                  "Setup file '%s' doesn't exist in "
                                  "subdirectory '%s'.\nCheck your -game "
                                  "parameter or VCONFIG setting.",
                                  GAMEINFO_FILENAME, fsInfo.m_pDirectoryName);
    }

    Q_strncpy(pOutDir, fsInfo.m_pDirectoryName, outDirLen);
    return FS_OK;
  }

  // First, check for overrides on the command line or environment variables.
  const char *pProject = GetVProjectCmdLineValue();

  if (pProject) {
    if (DoesFileExistIn(pProject, GAMEINFO_FILENAME)) {
      Q_MakeAbsolutePath(pOutDir, outDirLen, pProject);
      return FS_OK;
    }

    if (fsInfo.m_bNoGameInfo) {
      // fsInfo.m_bNoGameInfo is set by the Steam dedicated server, before it
      // knows which mod to use. Steam dedicated server doesn't need a
      // gameinfo.txt, because we'll ask which mod to use, even if -game is
      // supplied on the command line.
      Q_strncpy(pOutDir, "", outDirLen);
      return FS_OK;
    } else {
      // They either specified vproject on the command line or it's in their
      // registry. Either way, we don't want to continue if they've specified it
      // but it's not valid.
      goto ShowError;
    }
  }

  if (fsInfo.m_bNoGameInfo) {
    Q_strncpy(pOutDir, "", outDirLen);
    return FS_OK;
  }

  // Ask the application if it can provide us with a game info directory
  {
    bool bBubbleDir = true;
    SuggestGameInfoDirFn_t pfnSuggestGameInfoDirFn = GetSuggestGameInfoDirFn();
    if (pfnSuggestGameInfoDirFn &&
        (*pfnSuggestGameInfoDirFn)(&fsInfo, pOutDir, outDirLen, &bBubbleDir) &&
        FS_OK == TryLocateGameInfoFile(pOutDir, outDirLen, bBubbleDir))
      return FS_OK;
  }

  // Try to use the environment variable / registry
  if ((pProject = getenv(GAMEDIR_TOKEN)) != NULL &&
      (Q_MakeAbsolutePath(pOutDir, outDirLen, pProject), 1) &&
      FS_OK == TryLocateGameInfoFile(pOutDir, outDirLen, false))
    return FS_OK;

  Warning("Warning: falling back to auto detection of vproject directory.\n");

  // Now look for it in the directory they passed in.
  if (fsInfo.m_pDirectoryName)
    Q_MakeAbsolutePath(pOutDir, outDirLen, fsInfo.m_pDirectoryName);
  else
    Q_MakeAbsolutePath(pOutDir, outDirLen, ".");

  if (FS_OK == TryLocateGameInfoFile(pOutDir, outDirLen, true)) return FS_OK;

  // Use the CWD
  if (!Q_getwd(pOutDir, outDirLen))
    return SetupFileSystemError(true, FS_MISSING_GAMEINFO_FILE,
                                "Unable to get current directory.\n");

  if (FS_OK == TryLocateGameInfoFile(pOutDir, outDirLen, true)) return FS_OK;

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

bool DoesPathExistAlready(const char *pPathEnvVar, const char *pTestPath) {
  // Fix the slashes in the input arguments.
  char correctedPathEnvVar[8192], correctedTestPath[MAX_PATH];
  Q_strncpy(correctedPathEnvVar, pPathEnvVar, sizeof(correctedPathEnvVar));
  Q_FixSlashes(correctedPathEnvVar);
  pPathEnvVar = correctedPathEnvVar;

  Q_strncpy(correctedTestPath, pTestPath, sizeof(correctedTestPath));
  Q_FixSlashes(correctedTestPath);

  const size_t correctedTestPathLength = strlen(correctedTestPath);
  if (correctedTestPathLength > 0 &&
      PATHSEPARATOR(correctedTestPath[correctedTestPathLength - 1]))
    correctedTestPath[correctedTestPathLength - 1] = 0;

  pTestPath = correctedTestPath;
  size_t test_path_len = strlen(pTestPath);

  const char *pCurPos = pPathEnvVar;
  while (1) {
    const char *pTestPos = Q_stristr(pCurPos, pTestPath);
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

FSReturnCode_t SetSteamInstallPath(char *steamInstallPath,
                                   int steamInstallPathLen,
                                   CSteamEnvVars &steamEnvVars,
                                   bool bErrorsAsWarnings) {
  if (IsConsole()) {
    // consoles don't use steam
    return FS_MISSING_STEAM_DLL;
  }

  // Start at our bin directory and move up until we find a directory with
  // steam.dll in it.
  char executablePath[MAX_PATH];
  if (!FileSystem_GetExecutableDir(executablePath, sizeof(executablePath))) {
    if (bErrorsAsWarnings) {
      Warning("SetSteamInstallPath: FileSystem_GetExecutableDir failed.\n");
      return FS_INVALID_PARAMETERS;
    } else {
      return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                  "FileSystem_GetExecutableDir failed.");
    }
  }

  Q_strncpy(steamInstallPath, executablePath, steamInstallPathLen);
  while (1) {
    // Ignore steamapp.cfg here in case they're debugging. We still need to know
    // the real steam path so we can find their username. find
    if (DoesFileExistIn(steamInstallPath, "steam.dll") &&
        !DoesFileExistIn(steamInstallPath, "steamapp.cfg"))
      break;

    if (!Q_StripLastDir(steamInstallPath, steamInstallPathLen)) {
      if (bErrorsAsWarnings) {
        Warning("Can't find steam.dll relative to executable path: %s.\n",
                executablePath);
        return FS_MISSING_STEAM_DLL;
      } else {
        return SetupFileSystemError(
            false, FS_MISSING_STEAM_DLL,
            "Can't find steam.dll relative to executable path: %s.",
            executablePath);
      }
    }
  }

  // Also, add the install path to their PATH environment variable, so
  // filesystem_steam.dll can get to steam.dll.
  char szPath[8192];
  steamEnvVars.m_Path.GetValue(szPath, sizeof(szPath));
  if (!DoesPathExistAlready(szPath, steamInstallPath)) {
    steamEnvVars.m_Path.SetValue("%s;%s", szPath, steamInstallPath);
  }
  return FS_OK;
}

FSReturnCode_t GetSteamCfgPath(char *steamCfgPath, int steamCfgPathLen) {
  steamCfgPath[0] = 0;
  char executablePath[MAX_PATH];
  if (!FileSystem_GetExecutableDir(executablePath, sizeof(executablePath))) {
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");
  }
  Q_strncpy(steamCfgPath, executablePath, steamCfgPathLen);
  while (1) {
    if (DoesFileExistIn(steamCfgPath, "steam.cfg")) break;

    if (!Q_StripLastDir(steamCfgPath, steamCfgPathLen)) {
      // the file isnt found, thats ok, its not mandatory
      return FS_OK;
    }
  }
  Q_AppendSlash(steamCfgPath, steamCfgPathLen);
  Q_strncat(steamCfgPath, "steam.cfg", steamCfgPathLen, COPY_ALL_CHARACTERS);

  return FS_OK;
}

void SetSteamAppUser(KeyValues *pSteamInfo, const char *steamInstallPath,
                     CSteamEnvVars &steamEnvVars) {
  // Always inherit the Steam user if it's already set, since it probably means
  // we (or the the app that launched us) were launched from Steam.
  char appUser[MAX_PATH];
  if (steamEnvVars.m_SteamAppUser.GetValue(appUser, sizeof(appUser))) return;

  const char *pTempAppUser = NULL;
  if (pSteamInfo &&
      (pTempAppUser = pSteamInfo->GetString("SteamAppUser", NULL)) != NULL) {
    Q_strncpy(appUser, pTempAppUser, sizeof(appUser));
  } else {
    // They don't have SteamInfo.txt, or it's missing SteamAppUser. Try to
    // figure out the user by looking in <steam install
    // path>\config\SteamAppData.vdf.
    char fullFilename[MAX_PATH];
    Q_strncpy(fullFilename, steamInstallPath, sizeof(fullFilename));
    Q_AppendSlash(fullFilename, sizeof(fullFilename));
    Q_strncat(fullFilename, "config\\SteamAppData.vdf", sizeof(fullFilename),
              COPY_ALL_CHARACTERS);

    KeyValues *pSteamAppData = ReadKeyValuesFile(fullFilename);
    if (!pSteamAppData || (pTempAppUser = pSteamAppData->GetString(
                               "AutoLoginUser", NULL)) == NULL) {
      Error("Can't find steam app user info.");
    }
    Q_strncpy(appUser, pTempAppUser, sizeof(appUser));

    if (pSteamAppData) pSteamAppData->deleteThis();
  }

  Q_strlower(appUser);
  steamEnvVars.m_SteamAppUser.SetValue("%s", appUser);
}

void SetSteamUserPassphrase(KeyValues *pSteamInfo,
                            CSteamEnvVars &steamEnvVars) {
  // Always inherit the passphrase if it's already set, since it probably means
  // we (or the the app that launched us) were launched from Steam.
  char szPassPhrase[MAX_PATH];
  if (steamEnvVars.m_SteamUserPassphrase.GetValue(szPassPhrase,
                                                  sizeof(szPassPhrase)))
    return;

  // SteamUserPassphrase.
  const char *pStr;
  if (pSteamInfo &&
      (pStr = pSteamInfo->GetString("SteamUserPassphrase", NULL)) != NULL) {
    steamEnvVars.m_SteamUserPassphrase.SetValue("%s", pStr);
  }
}

void SetSteamAppId(KeyValues *pFileSystemInfo, const char *pGameInfoDirectory,
                   CSteamEnvVars &steamEnvVars) {
  // SteamAppId is in
  // gameinfo.txt->FileSystem->FileSystemInfo_Steam->SteamAppId.
  int iAppId = pFileSystemInfo->GetInt("SteamAppId", -1);
  if (iAppId == -1)
    Error("Missing SteamAppId in %s\\%s.", pGameInfoDirectory,
          GAMEINFO_FILENAME);

  steamEnvVars.m_SteamAppId.SetValue("%d", iAppId);
}

FSReturnCode_t SetupSteamStartupEnvironment(KeyValues *pFileSystemInfo,
                                            const char *pGameInfoDirectory,
                                            CSteamEnvVars &steamEnvVars) {
  // Ok, we're going to run Steam. See if they have SteamInfo.txt. If not, we'll
  // try to deduce what we can.
  char steamInfoFile[MAX_PATH];
  Q_strncpy(steamInfoFile, pGameInfoDirectory, sizeof(steamInfoFile));
  Q_AppendSlash(steamInfoFile, sizeof(steamInfoFile));
  Q_strncat(steamInfoFile, "steaminfo.txt", sizeof(steamInfoFile),
            COPY_ALL_CHARACTERS);
  KeyValues *pSteamInfo = ReadKeyValuesFile(steamInfoFile);

  char steamInstallPath[MAX_PATH];
  FSReturnCode_t ret = SetSteamInstallPath(
      steamInstallPath, sizeof(steamInstallPath), steamEnvVars, false);
  if (ret != FS_OK) return ret;

  SetSteamAppUser(pSteamInfo, steamInstallPath, steamEnvVars);
  SetSteamUserPassphrase(pSteamInfo, steamEnvVars);
  SetSteamAppId(pFileSystemInfo, pGameInfoDirectory, steamEnvVars);

  if (pSteamInfo) pSteamInfo->deleteThis();

  return FS_OK;
}

FSReturnCode_t GetSteamExtraAppId(const char *pDirectoryName,
                                  int *nExtraAppId) {
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

  char executablePath[MAX_PATH];
  if (!FileSystem_GetExecutableDir(executablePath, sizeof(executablePath)))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");

  pFileSystem->AddSearchPath(executablePath, "EXECUTABLE_PATH");
  return FS_OK;
}

//-----------------------------------------------------------------------------
// Returns the name of the file system DLL to use
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_GetFileSystemDLLName(char *pFileSystemDLL,
                                               int nMaxLen, bool &bSteam) {
  bSteam = false;

  // Inside of here, we don't have a filesystem yet, so we have to assume that
  // the filesystem_stdio or filesystem_steam is in this same directory with us.
  char executablePath[MAX_PATH];
  if (!FileSystem_GetExecutableDir(executablePath, sizeof(executablePath)))
    return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                "FileSystem_GetExecutableDir failed.");

#if defined(_WIN32)
  // If filesystem_stdio.dll is missing or -steam is specified, then load
  // filesystem_steam.dll. There are two command line parameters for Steam:
  //		1) -steam (runs Steam in remote filesystem mode; requires Steam
  // backend) 		2) -steamlocal (runs Steam in local filesystem mode (all
  // content off HDD)
  Q_snprintf(pFileSystemDLL, nMaxLen, "%s%cfilesystem_stdio.dll",
             executablePath, CORRECT_PATH_SEPARATOR);
  if (CommandLine()->FindParm("-steam") ||
      CommandLine()->FindParm("-steamlocal") ||
      _access(pFileSystemDLL, 0) != 0) {
    Q_snprintf(pFileSystemDLL, nMaxLen, "%s%cfilesystem_steam.dll",
               executablePath, CORRECT_PATH_SEPARATOR);
    bSteam = true;
  }
#elif defined(_LINUX)
  Q_snprintf(pFileSystemDLL, nMaxLen, "%s%cfilesystem_i486.so", executablePath,
             CORRECT_PATH_SEPARATOR);
#else
#error "define a filesystem dll name"
#endif

  return FS_OK;
}

//-----------------------------------------------------------------------------
// Sets up the steam.dll install path in our PATH env var (so you can then just
// LoadLibrary() on filesystem_steam.dll without having to copy steam.dll
// anywhere special )
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupSteamInstallPath() {
  CSteamEnvVars steamEnvVars;
  char steamInstallPath[MAX_PATH];
  FSReturnCode_t ret = SetSteamInstallPath(
      steamInstallPath, sizeof(steamInstallPath), steamEnvVars, true);
  steamEnvVars.m_Path.SetRestoreOriginalValue(
      false);  // We want to keep the change to the path going forward.
  return ret;
}

//-----------------------------------------------------------------------------
// Sets up the steam environment + gets back the gameinfo.txt path
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupSteamEnvironment(CFSSteamSetupInfo &fsInfo) {
  // First, locate the directory with gameinfo.txt.
  FSReturnCode_t ret = LocateGameInfoFile(fsInfo, fsInfo.m_GameInfoPath,
                                          sizeof(fsInfo.m_GameInfoPath));
  if (ret != FS_OK) return ret;

  // This is so that processes spawned by this application will have the same
  // VPROJECT
  char env[MAX_PATH + 32];
  Q_snprintf(env, sizeof(env), "%s=%s", GAMEDIR_TOKEN, fsInfo.m_GameInfoPath);
  if (_putenv(env) != 0) {
    return SetupFileSystemError(
        true, FS_UNABLE_TO_INIT,
        "Unable to set " GAMEDIR_TOKEN
        " environment variable to %s in the file system",
        fsInfo.m_GameInfoPath);
  }

  CSteamEnvVars steamEnvVars;
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

      steamEnvVars.m_SteamAppId.SetRestoreOriginalValue(
          false);  // We want to keep the change to the path going forward.

      // We're done with main file
      pMainFile->deleteThis();
    } else if (fsInfo.m_bSetSteamDLLPath) {
      // This is used by the engine to automatically set the path to their
      // steam.dll when running the engine, so they can debug it without having
      // to copy steam.dll up into their hl2.exe folder.
      char steamInstallPath[MAX_PATH];
      ret = SetSteamInstallPath(steamInstallPath, sizeof(steamInstallPath),
                                steamEnvVars, true);
      steamEnvVars.m_Path.SetRestoreOriginalValue(
          false);  // We want to keep the change to the path going forward.
    }
  }

  return FS_OK;
}

//-----------------------------------------------------------------------------
// Loads the file system module
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
// Mounds a particular steam cache
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_MountContent(CFSMountContentInfo &mountContentInfo) {
  // This part is Steam-only.
  if (mountContentInfo.m_pFileSystem->IsSteam()) {
    // Find out the "extra app id". This is for tools, which want to mount a
    // base app's filesystem like HL2, then mount the SDK content (tools
    // materials and models, etc) in addition.
    int nExtraAppId = -1;
    if (mountContentInfo.m_bToolsMode) {
      FSReturnCode_t ret =
          GetSteamExtraAppId(mountContentInfo.m_pDirectoryName, &nExtraAppId);
      if (ret != FS_OK) return ret;
    }

    // Set our working directory temporarily so Steam can remember it.
    // This is what Steam strips off absolute filenames like c:\program
    // files\valve\steam\steamapps\username\sourcesdk to get to the relative
    // part of the path.
    char baseDir[MAX_PATH], oldWorkingDir[MAX_PATH];
    if (!FileSystem_GetBaseDir(baseDir, sizeof(baseDir)))
      return SetupFileSystemError(false, FS_INVALID_PARAMETERS,
                                  "FileSystem_GetBaseDir failed.");

    if (!Q_getwd(oldWorkingDir, sizeof(oldWorkingDir))) {
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

void FileSystem_SetErrorMode(FSErrorMode_t errorMode) {
  g_FileSystemErrorMode = errorMode;
}

void FileSystem_ClearSteamEnvVars() {
  CSteamEnvVars envVars;

  // Change the values and don't restore the originals.
  envVars.m_SteamAppId.SetValue("");
  envVars.m_SteamUserPassphrase.SetValue("");
  envVars.m_SteamAppUser.SetValue("");

  envVars.SetRestoreOriginalValue_ALL(false);
}

//-----------------------------------------------------------------------------
// Adds the platform folder to the search path.
//-----------------------------------------------------------------------------
void FileSystem_AddSearchPath_Platform(IFileSystem *pFileSystem,
                                       const char *szGameInfoPath) {
  char platform[MAX_PATH];
  if (pFileSystem->IsSteam()) {
    // Steam doesn't support relative paths
    Q_strncpy(platform, "platform", MAX_PATH);
  } else {
    Q_strncpy(platform, szGameInfoPath, MAX_PATH);
    Q_StripTrailingSlash(platform);
    Q_strncat(platform, "/../platform", MAX_PATH, MAX_PATH);
  }

  pFileSystem->AddSearchPath(platform, "PLATFORM");
}
