// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "common.h"

#include <ctype.h>
#include <malloc.h>
#include <cassert>

#include "bitbuf.h"
#include "characterset.h"
#include "convar.h"
#include "coordsize.h"
#include "datacache/idatacache.h"
#include "draw.h"
#include "edict.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "filesystem_init.h"
#include "gl_matsysiface.h"
#include "host.h"
#include "language.h"
#include "matchmaking.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "sys.h"
#include "sys_dll.h"
#include "tier0/include/icommandline.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
#include "traceinit.h"
#include "vstdlib/random.h"
#include "zone.h"

#ifdef OS_WIN
#include "vgui_baseui_interface.h"
#endif

#include "tier0/include/memdbgon.h"

// Things in other C files.
#define MAX_LOG_DIRECTORIES 10000

bool com_ignorecolons = false;

// wordbreak parsing set
static characterset_t g_BreakSet, g_BreakSetIncludingColons;

ch com_token[1024];

/*
All of Quake's data access is through a hierarchical file system, but the
contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories.  The sys_* files pass this to host_init in
engineparms->basedir. This can be overridden with the "-basedir" command line
parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory
that all generated files (savegames, screenshots, demos, config files) will
be saved to.  This can be overridden with the "-game" command line parameter.
The game directory can never be changed while quake is executing.
This is a precacution against having a malicious server instruct clients
to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.

TODO(d.rattman):
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently.  This could be used to add a "-sspeed 22050" for the high
quality sound edition.  Because they are added at the end, they will not
override an explicit setting on the original command line.
*/

void COM_ExplainDisconnection(bool bPrint, const ch *fmt, ...) {
  va_list argptr;
  ch string[std::size(gszDisconnectReason)];

  va_start(argptr, fmt);
  vsprintf_s(string, fmt, argptr);
  va_end(argptr);

  strcpy_s(gszDisconnectReason, string);
  gfExtendedError = true;

  if (bPrint) {
    ConMsg("%s\n", gszDisconnectReason);
  }
}

void COM_ExtendedExplainDisconnection(bool bPrint, ch *fmt, ...) {
  va_list argptr;
  ch string[std::size(gszExtendedDisconnectReason)];

  va_start(argptr, fmt);
  vsprintf_s(string, fmt, argptr);
  va_end(argptr);

  strcpy_s(gszExtendedDisconnectReason, string);

  if (bPrint) {
    ConMsg("%s\n", gszExtendedDisconnectReason);
  }
}

// Parse a token out of a string.
const ch *COM_Parse(const ch *data) {
  com_token[0] = '\0';

  if (!data) return nullptr;

  characterset_t *breaks{com_ignorecolons ? &g_BreakSet
                                          : &g_BreakSetIncludingColons};
  unsigned char c;

// skip whitespace
skipwhite:
  while ((c = *data) <= ' ') {
    if (c == 0) return nullptr;  // end of file;
    data++;
  }

  // skip // comments
  if (c == '/' && data[1] == '/') {
    while (*data && *data != '\n') data++;
    goto skipwhite;
  }

  usize length{0};

  // handle quoted strings specially
  if (c == '\"') {
    data++;
    while (length < std::size(com_token)) {
      c = *data++;

      if (c == '\"' || !c) {
        com_token[length] = '\0';
        return data;
      }

      com_token[length++] = c;
    }
  }

  // parse single characters
  if (IN_CHARACTERSET(*breaks, c)) {
    com_token[length++] = c;
    com_token[std::min(length, std::size(com_token) - 1)] = '\0';
    return data + 1;
  }

  // parse a regular word
  do {
    com_token[length++] = c;
    data++;
    c = *data;
    if (IN_CHARACTERSET(*breaks, c)) break;
  } while (c > 32 && length < std::size(com_token));

  com_token[std::min(length, std::size(com_token) - 1)] = '\0';

  return data;
}

// Parse a line out of a string
ch *COM_ParseLine(ch *data) {
  com_token[0] = '\0';

  if (!data) return nullptr;

  usize len = 0;
  ch c = *data;

  // parse a line out of the data
  do {
    com_token[len++] = c;
    data++;
    c = *data;
  } while ((c >= ' ' || c == '\t') && (len < std::size(com_token) - 1));

  com_token[std::min(len, std::size(com_token) - 1)] = '\0';

  // end of file
  if (c == 0) return nullptr;

  // eat whitespace (LF,CR,etc.) at the end of this line
  while ((c = *data) < ' ') {
    if (c == 0) return nullptr;  // end of file
    data++;
  }

  return data;
}

// Returns 1 if additional data is waiting to be processed on this line
int COM_TokenWaiting(const ch *buffer) {
  const ch *p = buffer;

  while (*p && *p != '\n') {
    if (!isspace(*p) || isalnum(*p)) return 1;

    p++;
  }

  return 0;
}

// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
ch *va(const ch *format, ...) {
  va_list argptr;
  static ch string[8][512];
  static int curstring = 0;

  curstring = (curstring + 1) % 8;

  va_start(argptr, format);
  vsprintf_s(string[curstring], format, argptr);
  va_end(argptr);

  return string[curstring];
}

// Prints a vector into a temporary string bufffer.
ch *vstr(Vector &v) {
  static int idx = 0;
  static ch string[16][1024];

  idx++;
  idx &= 15;

  sprintf_s(string[idx], "%.2f %.2f %.2f", v[0], v[1], v[2]);

  return string[idx];
}

ch com_basedir[MAX_OSPATH];
ch com_gamedir[MAX_OSPATH];

// Client side game directory change.
bool COM_CheckGameDirectory(const ch *gamedir) {
  // Switch game directories if needed, or abort if it's not good.
  ch szGD[SOURCE_MAX_PATH];

  if (!gamedir || !gamedir[0]) {
    ConMsg("Server didn't specify a gamedir, assuming no change\n");
    return true;
  }

  // Rip out the current gamedir.
  Q_FileBase(com_gamedir, szGD, sizeof(szGD));

  if (_stricmp(szGD, gamedir)) {
    // Changing game directories without restarting is not permitted any more
    ConMsg("COM_CheckGameDirectory: game directories don't match (%s / %s)\n",
           szGD, gamedir);
    return false;
  }

  return true;
}

// Finds the file in the search path.
int COM_FindFile(const ch *filename, FileHandle_t *file) {
  Assert(file);
  *file = g_pFileSystem->Open(filename, "rb");

  return *file ? g_pFileSystem->Size(*file) : -1;
}

int COM_OpenFile(const ch *filename, FileHandle_t *file) {
  return COM_FindFile((ch *)filename, file);
}

// The filename will be prefixed by the current game directory
void COM_WriteFile(const ch *filename, void *data, int len) {
  usize nameLen = strlen(filename) + 2;
  ch *pName = (ch *)_alloca(nameLen);

  sprintf_s(pName, nameLen, "%s", filename);

  Q_FixSlashes(pName);
  COM_CreatePath(pName);

  FileHandle_t handle = g_pFileSystem->Open(pName, "wb");
  if (!handle) {
    Warning("COM_WriteFile: failed on %s\n", pName);
    return;
  }

  g_pFileSystem->Write(data, len, handle);
  g_pFileSystem->Close(handle);
}

// Only used for CopyFile
void COM_CreatePath(const ch *path) {
  ch temppath[SOURCE_MAX_PATH];
  strcpy_s(temppath, path);

  for (ch *ofs = temppath + 1; *ofs; ofs++) {
    if (*ofs == '/' || *ofs == '\\') {  // create the directory
      ch old = *ofs;
      *ofs = 0;
      Sys_mkdir(temppath);
      *ofs = old;
    }
  }
}

// Copies a file over from the net to the local cache, creating any directories
// needed.  This is for the convenience of developers using ISDN from home.
bool COM_CopyFile(const ch *netpath, const ch *cachepath) {
  FileHandle_t in = g_pFileSystem->Open(netpath, "rb");
  if (in == FILESYSTEM_INVALID_HANDLE) return false;

  // create directories up to the cache file
  COM_CreatePath(cachepath);

  FileHandle_t out = g_pFileSystem->Open(cachepath, "wb");
  if (out == FILESYSTEM_INVALID_HANDLE) {
    g_pFileSystem->Close(in);
    return false;
  }

  ch buf[4096];

  long long remaining = g_pFileSystem->Size(in);
  while (remaining > 0) {
    int count = remaining < sizeof(buf) ? remaining : sizeof(buf);

    g_pFileSystem->Read(buf, count, in);
    g_pFileSystem->Write(buf, count, out);

    remaining -= count;
  }

  g_pFileSystem->Close(in);
  g_pFileSystem->Close(out);

  return true;
}

// Finds the file in the search path, copies over the name with the full path
// name. This doesn't search in the pak file.
int COM_ExpandFilename(ch *filename, int maxlength) {
  ch expanded[SOURCE_MAX_PATH];

  if (g_pFileSystem->GetLocalPath(filename, expanded, sizeof(expanded)) !=
      nullptr) {
    Q_strncpy(filename, expanded, maxlength);
    return 1;
  }

  if (filename && filename[0] != '*') {
    Warning("COM_ExpandFilename: can't find %s\n", filename);
  }

  return 0;
}

// Returns the size of the file only.
int COM_FileSize(const ch *filename) { return g_pFileSystem->Size(filename); }

// Close file handle
void COM_CloseFile(FileHandle_t hFile) { g_pFileSystem->Close(hFile); }

// Filename are reletive to the quake directory. Always appends a 0 byte.

cache_user_t *loadcache;
u8 *loadbuf;
int loadsize;

u8 *COM_LoadFile(const ch *path, int usehunk, int *pLength) {
  if (pLength) *pLength = 0;

  // look for it in the filesystem or pack files
  FileHandle_t hFile;
  int len = COM_OpenFile(path, &hFile);
  if (!hFile) return nullptr;

  // Extract the filename base name for hunk tag
  ch base[128];
  Q_FileBase(path, base, sizeof(base));

  usize bufSize = len + 1;
  byte *buf = nullptr;

  switch (usehunk) {
    case 1:
      buf = (u8 *)Hunk_AllocName(bufSize, base);
      break;
    case 2:
      AssertMsg(0, "Temp alloc no longer supported\n");
      break;
    case 3:
      AssertMsg(0, "Cache alloc no longer supported\n");
      break;
    case 4: {
      if (len + 1 > loadsize)
        buf = (u8 *)malloc(bufSize);
      else
        buf = loadbuf;
    } break;
    case 5:
      buf = (u8 *)malloc(bufSize);  // TODO(d.rattman): This is evil.
      break;
    default:
      Sys_Error("COM_LoadFile: bad usehunk");
  }

  if (!buf) {
    Sys_Error("COM_LoadFile: not enough space for %s", path);
    COM_CloseFile(hFile);  // exit here to prevent fault on oom (kdb)
    return nullptr;
  }

  g_pFileSystem->ReadEx(buf, bufSize, len, hFile);
  COM_CloseFile(hFile);

  ((u8 *)buf)[len] = 0;

  if (pLength) *pLength = len;

  return buf;
}

void COM_CopyFileChunk(FileHandle_t dst, FileHandle_t src, int nSize) {
  int copysize = nSize;
  ch copybuf[COM_COPY_CHUNK_SIZE];

  while (copysize > COM_COPY_CHUNK_SIZE) {
    g_pFileSystem->Read(copybuf, COM_COPY_CHUNK_SIZE, src);
    g_pFileSystem->Write(copybuf, COM_COPY_CHUNK_SIZE, dst);
    copysize -= COM_COPY_CHUNK_SIZE;
  }

  g_pFileSystem->Read(copybuf, copysize, src);
  g_pFileSystem->Write(copybuf, copysize, dst);

  g_pFileSystem->Flush(src);
  g_pFileSystem->Flush(dst);
}

// Uses malloc if larger than bufsize
u8 *COM_LoadStackFile(const ch *path, void *buffer, int bufsize,
                      int &filesize) {
  loadbuf = (u8 *)buffer;
  loadsize = bufsize;
  u8 *buf = COM_LoadFile(path, 4, &filesize);
  return buf;
}

void COM_ShutdownFileSystem() {}

// Remove the searchpaths
void COM_Shutdown() {}

// Allocates memory and copys source text.
ch *COM_StringCopy(const ch *in) {
  usize len = strlen(in) + 1;
  ch *out = new ch[len];
  strcpy_s(out, len, in);

  return out;
}

void COM_StringFree(const ch *in) { delete[] in; }

void COM_SetupLogDir(const ch *mapname) {
  ch gameDir[SOURCE_MAX_PATH];
  COM_GetGameDir(gameDir, std::size(gameDir));

  // Blat out the all directories in the LOGDIR path
  g_pFileSystem->RemoveSearchPath(nullptr, "LOGDIR");

  // set the log directory
  if (mapname && CommandLine()->FindParm("-uselogdir")) {
    ch sRelativeLogDir[SOURCE_MAX_PATH];
    int i;

    for (i = 0; i < MAX_LOG_DIRECTORIES; i++) {
      sprintf_s(sRelativeLogDir, "logs/%s/%04i", mapname, i);
      if (!g_pFileSystem->IsDirectory(sRelativeLogDir, "GAME")) break;
    }

    // Loop at max
    if (i == MAX_LOG_DIRECTORIES) {
      i = 0;
      sprintf_s(sRelativeLogDir, "logs/%s/%04i", mapname, i);
    }

    // Make sure the directories we need exist.
    g_pFileSystem->CreateDirHierarchy(sRelativeLogDir, "GAME");

    {
      static bool pathsetup = false;

      if (!pathsetup) {
        pathsetup = true;

        // Set the search path
        ch sLogDir[SOURCE_MAX_PATH];
        sprintf_s(sLogDir, "%s/%s", gameDir, sRelativeLogDir);
        g_pFileSystem->AddSearchPath(sLogDir, "LOGDIR");
      }
    }
  } else {
    // Default to the base game directory for logs.
    g_pFileSystem->AddSearchPath(gameDir, "LOGDIR");
  }
}

// Return the final directory in the game dir (i.e "cstrike", "hl2", rather than
// c:\blah\cstrike).
const ch *COM_GetModDirectory() {
  static ch modDir[SOURCE_MAX_PATH];

  if (modDir[0] == '\0') {
    const ch *gamedir = CommandLine()->ParmValue(
        source::tier0::command_line_switches::kGamePath,
        CommandLine()->ParmValue("-defaultgamedir", "hl2"));

    strcpy_s(modDir, gamedir);

    if (strchr(modDir, '/') || strchr(modDir, '\\')) {
      Q_StripLastDir(modDir, sizeof(modDir));

      usize dirlen = strlen(modDir);
      strncpy_s(modDir, gamedir + dirlen, sizeof(modDir) - dirlen);
    }
  }

  return modDir;
}

void COM_InitFilesystem(const ch *pFullModPath) {
  CFSSearchPathsInit initInfo;

#ifndef SWDS
  // get Steam client language
  ch language[128];
  language[0] = '\0';
  Sys_GetRegKeyValue("Software\\Valve\\Steam", "Language", language,
                     sizeof(language), "");
  if (strlen(language) > 0 && _stricmp(language, "english")) {
    initInfo.m_pLanguage = language;
  }
#endif

  initInfo.m_pFileSystem = g_pFileSystem;
  initInfo.m_pDirectoryName = pFullModPath;
  if (!initInfo.m_pDirectoryName) {
    initInfo.m_pDirectoryName = GetCurrentGame();
  }

  // Load gameinfo.txt and setup all the search paths, just like the tools do.
  FileSystem_LoadSearchPaths(initInfo);

  // Enable file
  KeyValues *modinfo = new KeyValues("ModInfo");
  if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt")) {
    // If it's not singleplayer_only
    if (V_stricmp(modinfo->GetString("type", "singleplayer_only"),
                  "singleplayer_only") == 0) {
      DevMsg("Disabling whitelist file tracking in filesystem...\n");
      g_pFileSystem->EnableWhitelistFileTracking(false);
    } else {
      DevMsg("Enabling whitelist file tracking in filesystem...\n");
      g_pFileSystem->EnableWhitelistFileTracking(true);
    }
  }
  modinfo->deleteThis();

  // The mod path becomes com_gamedir.
  Q_MakeAbsolutePath(com_gamedir, sizeof(com_gamedir), initInfo.m_ModPath);

  // Set com_basedir.
  // the "root" directory where hl2.exe is
  strcpy_s(com_basedir, GetBaseDirectory());
  Q_strlower(com_basedir);
  Q_FixSlashes(com_basedir);

#ifndef SWDS
  EngineVGui()->SetVGUIDirectories();
#endif

  // Set LOGDIR to be something reasonable
  COM_SetupLogDir(nullptr);

#ifndef SWDS
  // Get App's Audio Language
  //
  // We need to read the GameInfo.txt file to get the app being run. Then we
  // need to look in the registry for the audio language for this game. If the
  // games language is non-English then we add a search path entry for this
  // language that looks like this "game\<GAME>_<AUDIOLANG>\sound"
  modinfo = new KeyValues("ModInfo");  //-V773
  if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt")) {
    const int nSteamAppId =
        modinfo->FindKey("FileSystem")->GetInt("SteamAppId", 215);
    ELanguage eAudioLanguage = k_Lang_English;
    ch szAudioLanguageRegKey[SOURCE_MAX_PATH];
    long lRegValue;

    // Construct registry key path
    sprintf_s(szAudioLanguageRegKey, "Software\\Valve\\Steam\\Apps\\%d",
              nSteamAppId);
    Sys_GetRegKeyValueInt(szAudioLanguageRegKey, "language", &lRegValue,
                          k_Lang_English);
    eAudioLanguage = (ELanguage)lRegValue;

    // If audio language is not English and audio language is different from
    // Steam client language then add foreign language audio to the top of the
    // search path k_Lang_None means the game will follow the language of the
    // Steam client
    if ((k_Lang_English != eAudioLanguage && k_Lang_None != eAudioLanguage) &&
        (PchLanguageToELanguage(initInfo.m_pLanguage) != eAudioLanguage)) {
      // Now add all of the foreign language paths to the top of the
      // searchpaths list for
      KeyValues *pSearchPaths =
          modinfo->FindKey("FileSystem")->FindKey("SearchPaths");
      CUtlVector<ch *> vSearchPaths;
      for (KeyValues *pCur = pSearchPaths->GetFirstValue(); pCur;
           pCur = pCur->GetNextValue()) {
        const ch *pPathID = pCur->GetName();
        const ch *pLocation = pCur->GetString();

        if (V_stricmp(pPathID, "game") == 0 &&
            (nullptr == V_strstr(pLocation, "|gameinfo_path|"))) {
          ch *szFullSearchPath = new ch[SOURCE_MAX_PATH];

          sprintf_s(szFullSearchPath, SOURCE_MAX_PATH, "%s%c%s_%s%c",
                    com_basedir, CORRECT_PATH_SEPARATOR, pLocation,
                    GetLanguageShortName(eAudioLanguage),
                    CORRECT_PATH_SEPARATOR);
          vSearchPaths.AddToHead(szFullSearchPath);
        }
      }

      // Add each of the search paths in the same order that they occur in
      // GameInfo text. We need to reverse the list because the AddSearchPath
      // can only add singly to the head or tail of the list
      for (int i = 0; i < vSearchPaths.Count(); i++) {
        ch *pcFirstElem = vSearchPaths[i];
        g_pFileSystem->AddSearchPath(pcFirstElem, "game", PATH_ADD_TO_HEAD);
        delete[] vSearchPaths[i];
      }
#ifdef _DEBUG
      g_pFileSystem->PrintSearchPaths();
#endif
    }
    modinfo->deleteThis();
  }
#endif
}  //-V773

const ch *COM_DXLevelToString(int dxlevel) {
  bool bHalfPrecision = false;

  const ch *pShaderDLLName =
      g_pMaterialSystemHardwareConfig->GetShaderDLLName();
  if (pShaderDLLName && Q_stristr(pShaderDLLName, "nvfx")) {
    bHalfPrecision = true;
  }

  if (CommandLine()->CheckParm("-dxlevel")) {
    switch (dxlevel) {
      case 0:
        return "default";
      case 60:
        return "6.0";
      case 70:
        return "7.0";
      case 80:
        return "8.0";
      case 81:
        return "8.1";
      case 82:
        if (bHalfPrecision) {
          return "8.1 with some 9.0 (half-precision)";
        } else {
          return "8.1 with some 9.0 (full-precision)";
        }
      case 90:
        if (bHalfPrecision) {
          return "9.0 (half-precision)";
        } else {
          return "9.0 (full-precision)";
        }
      case 95:
        if (bHalfPrecision) {
          return "9.0+ (half-precision)";
        } else {
          return "9.0+ (full-precision)";
        }
      default:
        return "UNKNOWN";
    }
  } else {
    switch (dxlevel) {
      case 60:
        return "gamemode - 6.0";
      case 70:
        return "gamemode - 7.0";
      case 80:
        return "gamemode - 8.0";
      case 81:
        return "gamemode - 8.1";
      case 82:
        if (bHalfPrecision) {
          return "gamemode - 8.1 with some 9.0 (half-precision)";
        } else {
          return "gamemode - 8.1 with some 9.0 (full-precision)";
        }
      case 90:
        if (bHalfPrecision) {
          return "gamemode - 9.0 (half-precision)";
        } else {
          return "gamemode - 9.0 (full-precision)";
        }
      case 95:
        if (bHalfPrecision) {
          return "gamemode - 9.0+ (half-precision)";
        } else {
          return "gamemode - 9.0+ (full-precision)";
        }
      default:
        return "gamemode";
    }
  }
}

const ch *COM_FormatSeconds(int seconds) {
  static ch string[64];

  int hours = 0;
  int minutes = seconds / 60;

  if (minutes > 0) {
    seconds -= (minutes * 60);
    hours = minutes / 60;

    if (hours > 0) {
      minutes -= (hours * 60);
    }
  }

  if (hours > 0) {
    sprintf_s(string, "%2i:%02i:%02i", hours, minutes, seconds);
  } else {
    sprintf_s(string, "%02i:%02i", minutes, seconds);
  }

  return string;
}

// Non-VarArgs version
void COM_LogString(ch const *pchFile, ch const *pchString) {
  if (!g_pFileSystem) {
    Assert(0);
    return;
  }

  const ch *pfilename = !pchFile ? "hllog.txt" : pchFile;
  FileHandle_t fp = g_pFileSystem->Open(pfilename, "a+t");

  if (fp) {
    g_pFileSystem->FPrintf(fp, "%s", pchString);
    g_pFileSystem->Close(fp);
  }
}

void COM_Log(ch *pszFile, ch *fmt, ...) {
  if (!g_pFileSystem) {
    Assert(0);
    return;
  }

  va_list argptr;
  ch string[8192];

  va_start(argptr, fmt);
  vsprintf_s(string, fmt, argptr);
  va_end(argptr);

  COM_LogString(pszFile, string);
}

int COM_CompareFileTime(const ch *filename1, const ch *filename2,
                        int *iCompare) {
  int bRet = 0;
  if (iCompare) {
    *iCompare = 0;
  }

  if (filename1 && filename2) {
    long ft1 = g_pFileSystem->GetFileTime(filename1);
    long ft2 = g_pFileSystem->GetFileTime(filename2);

    if (iCompare) {
      *iCompare = Sys_CompareFileTime(ft1, ft2);
    }
    bRet = 1;
  }

  return bRet;
}

void COM_GetGameDir(ch *szGameDir, usize maxlen) {
  if (szGameDir) strcpy_s(szGameDir, maxlen, com_gamedir);
}

// Parse a token from a file stream
const ch *COM_ParseFile(const ch *data, ch *token, usize maxtoken) {
  const ch *return_data = COM_Parse(data);
  strcpy_s(token, maxtoken, com_token);
  return return_data;
}

void COM_Init() {
  CharacterSetBuild(&g_BreakSet, "{}()'");
  CharacterSetBuild(&g_BreakSetIncludingColons, "{}()':");
}

bool COM_IsValidPath(const ch *pszFilename) {
  return pszFilename && pszFilename[0] != '\0' &&
         !(Q_strstr(pszFilename, "\\\\") ||  // to protect network paths
           Q_strstr(pszFilename, ":") ||     // to protect absolute paths
           Q_strstr(pszFilename, ".."));     // to protect relative paths
}
