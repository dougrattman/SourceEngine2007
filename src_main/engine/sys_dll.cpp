// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "build/include/build_config.h"

#include "sys_dll.h"

#ifdef OS_WIN
#include <io.h>
#include "base/include/windows/windows_light.h"
#endif

#include "DevShotGenerator.h"
#include "MapReslistGenerator.h"
#include "build/include/build_config.h"
#include "cdll_engine_int.h"
#include "cmd.h"
#include "crtmemdebug.h"
#include "dt_send.h"
#include "dt_test.h"
#include "eifaceV21.h"
#include "errno.h"
#include "filesystem_engine.h"
#include "gameui/IGameUI.h"
#include "gl_matsysiface.h"
#include "host.h"
#include "host_cmd.h"
#include "idedicatedexports.h"
#include "igame.h"
#include "ihltvdirector.h"
#include "initmathlib.h"
#include "ivideomode.h"
#include "keys.h"
#include "profiling.h"
#include "quakedef.h"
#include "server.h"
#include "steam/steam_api.h"
#include "sv_log.h"
#include "sv_main.h"
#include "sys.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/minidump.h"
#include "tier0/include/vcrmode.h"
#include "toolframework/itoolframework.h"
#include "traceinit.h"
#include "vengineserver_impl.h"

#ifdef OS_WIN
#include "vgui_baseui_interface.h"
#endif

#include "tier0/include/memdbgon.h"

ConVar mem_min_heapsize("mem_min_heapsize", "48", 0,
                        "Minimum amount of memory to dedicate to engine hunk "
                        "and datacache (in mb)");
ConVar mem_max_heapsize("mem_max_heapsize", "512", 0,
                        "Maximum amount of memory to dedicate to engine hunk "
                        "and datacache (in mb)");
ConVar mem_max_heapsize_dedicated("mem_max_heapsize_dedicated", "128", 0,
                                  "Maximum amount of memory to dedicate to "
                                  "engine hunk and datacache, for dedicated "
                                  "server (in mb)");

inline u32 MinHeapMemoryBytes() {
  return (u32)(mem_min_heapsize.GetInt() * 1024 * 1024);
}

inline u32 MaxHeapMemoryBytes() {
  return std::max((u32)(mem_max_heapsize.GetInt() * 1024 * 1024),
                  MinHeapMemoryBytes());
}

inline u32 MaxDedicatedHeapBytes() {
  return (u32)(mem_max_heapsize_dedicated.GetInt() * 1024 * 1024);
}

ch *CheckParm(const ch *psz, ch **ppszValue = nullptr);
void SeedRandomNumberGenerator(bool random_invariant);
void Con_ColorPrintf(const Color &clr, const ch *fmt, ...);

void COM_ShutdownFileSystem();
void COM_InitFilesystem(const ch *pFullModPath);

modinfo_t gmodinfo;

#ifdef OS_WIN
extern HWND *pmainwindow;
#endif

ch gszDisconnectReason[256];
ch gszExtendedDisconnectReason[256];
bool gfExtendedError{false};
ESteamLoginFailure g_eSteamLoginFailure{STEAMLOGINFAILURE_NONE};
bool g_bV3SteamInterface{false};
CreateInterfaceFn g_AppSystemFactory{nullptr};

static bool s_bIsDedicated{false};
ConVar *sv_noclipduringpause{nullptr};

// Special mode where the client uses a console window and has no graphics.
// Useful for stress-testing a server without having to round up 32 people.
bool g_bTextMode{false};

// Set to true when we exit from an error.
bool g_bInErrorExit{false};

static FileFindHandle_t g_hfind{FILESYSTEM_INVALID_FIND_HANDLE};

// The extension DLL directory--one entry per loaded DLL
CSysModule *g_GameDLL{nullptr};

// Prototype of an global method function
using PFN_GlobalMethod = void(DLLEXPORT *)(edict_t *pEntity);

IServerGameDLL *serverGameDLL{nullptr};
bool g_bServerGameDLLGreaterThanV4;
bool g_bServerGameDLLGreaterThanV5;
IServerGameEnts *serverGameEnts{nullptr};

IServerGameClients *serverGameClients{nullptr};
// This matches the number at the end of the interface name (so for
// "ServerGameClients004", this would be 4).
int g_iServerGameClientsVersion{0};

IHLTVDirector *serverGameDirector{nullptr};

// Purpose: Compare file times.
int Sys_CompareFileTime(long ft1, long ft2) {
  if (ft1 < ft2) return -1;

  if (ft1 > ft2) return 1;

  return 0;
}

// Is slash?
inline bool IsSlash(ch c) { return c == '\\' || c == '/'; }

// Purpose: Create specified directory
void Sys_mkdir(const ch *path) {
  ch test_path[SOURCE_MAX_PATH];
  // Remove any terminal backslash or /
  strcpy_s(test_path, path);

  const usize path_len{strlen(test_path)};
  if (path_len > 0 && IsSlash(test_path[path_len - 1])) {
    test_path[path_len - 1] = '\0';
  }

  // Look for URL
  const ch *path_id{"MOD"};
  if (IsSlash(test_path[0]) && IsSlash(test_path[1])) {
    path_id = nullptr;
  }

  if (g_pFileSystem->FileExists(test_path, path_id)) {
    // if there is a file of the same name as the directory we want to make,
    // just kill it
    if (!g_pFileSystem->IsDirectory(test_path, path_id)) {
      g_pFileSystem->RemoveFile(test_path, path_id);
    }
  }

  g_pFileSystem->CreateDirHierarchy(path, path_id);
}

const ch *Sys_FindFirst(const ch *path, ch *basename, int namelength) {
  if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE) {
    Sys_Error("Sys_FindFirst without close");
    g_pFileSystem->FindClose(g_hfind);
  }

  const ch *psz = g_pFileSystem->FindFirst(path, &g_hfind);
  if (basename && psz) {
    Q_FileBase(psz, basename, namelength);
  }

  return psz;
}

// Purpose: Sys_FindFirst with a path ID filter.
const ch *Sys_FindFirstEx(const ch *pWildcard, const ch *pPathID, ch *basename,
                          int namelength) {
  if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE) {
    Sys_Error("Sys_FindFirst without close");
    g_pFileSystem->FindClose(g_hfind);
  }

  const ch *psz = g_pFileSystem->FindFirstEx(pWildcard, pPathID, &g_hfind);
  if (basename && psz) {
    Q_FileBase(psz, basename, namelength);
  }

  return psz;
}

const ch *Sys_FindNext(ch *basename, int namelength) {
  const ch *psz = g_pFileSystem->FindNext(g_hfind);
  if (basename && psz) {
    Q_FileBase(psz, basename, namelength);
  }

  return psz;
}

void Sys_FindClose() {
  if (FILESYSTEM_INVALID_FIND_HANDLE != g_hfind) {
    g_pFileSystem->FindClose(g_hfind);
    g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
  }
}

// Purpose: OS Specific initializations.
void Sys_Init() {}

void Sys_Shutdown() {}

// Purpose: Print to system console.
void Sys_Printf(const ch *format, ...) {
  va_list arg_ptr;
  ch message[1024];

  va_start(arg_ptr, format);
  vsprintf_s(message, format, arg_ptr);
  va_end(arg_ptr);

  if (developer.GetInt()) {
#ifdef OS_WIN
    wch unicode_message[2048];
    MultiByteToWideChar(CP_UTF8, 0, message, -1, unicode_message,
                        std::size(unicode_message));
    unicode_message[std::size(unicode_message) - 1] = L'\0';

    OutputDebugStringW(unicode_message);
    Sleep(0);
#endif
  }

  if (s_bIsDedicated) {
    fprintf_s(stdout, "%s", message);
  }
}

bool Sys_MessageBox(const ch *title, const ch *info,
                    bool should_show_ok_cancel) {
#ifdef OS_WIN
  return IDOK == MessageBox(nullptr, title, info,
                            MB_ICONEXCLAMATION |
                                (should_show_ok_cancel ? MB_OKCANCEL : MB_OK));
#elif OS_POSIX
  Warning("%s\n", info);
  return true;
#else
#error Please, implement Sys_MessageBox at engine/sys_dll.cpp
#endif
}

// Purpose: Exit engine with error.
void Sys_Error(const ch *format, ...) {
  va_list arg_ptr;
  ch error_message[1024];
  static bool bReentry = false;  // Don't meltdown

  va_start(arg_ptr, format);
  vsprintf_s(error_message, format, arg_ptr);
  va_end(arg_ptr);

  if (bReentry) {
    fprintf(stderr, "%s\n", error_message);
    return;
  }

  bReentry = true;

  if (s_bIsDedicated) {
    fprintf(stderr, "%s\n", error_message);
  } else {
    Sys_Printf("%s\n", error_message);
  }

  g_bInErrorExit = true;

#ifndef SWDS
  if (videomode) videomode->Shutdown();
#endif

#ifdef OS_WIN
  if (!CommandLine()->FindParm("-makereslists") &&
      !CommandLine()->FindParm("-nomessagebox")) {
    MessageBox(nullptr, error_message, "Awesome Engine - Error",
               MB_OK | MB_ICONERROR | MB_TOPMOST);
  }

  DebuggerBreakIfDebugging();

  if (!Plat_IsInDebugSession() && !CommandLine()->FindParm("-nominidumps")) {
    WriteMiniDump();
  }
#endif

  host_initialized = false;

#ifdef OS_WIN
  // We don't want global destructors in our process OR in any DLL to get
  // executed. _exit() avoids calling global destructors in our module, but
  // not in other DLLs.
  TerminateProcess(GetCurrentProcess(), 100);
#else
  _exit(100);
#endif
}

bool IsInErrorExit() { return g_bInErrorExit; }

void Sys_Sleep(int msec) {
#ifdef OS_WIN
  Sleep(msec);
#elif OS_POSIX
  usleep(msec);
#endif
}

// Allocate memory for engine hunk.
void Sys_InitMemory() {
#ifdef OS_WIN
  // Allow overrides.
  const size_t heap_size = CommandLine()->ParmValue("-heapsize", 0);
  if (heap_size) {
    host_parms.memsize = heap_size * 1024;
    return;
  }

  host_parms.memsize = MinHeapMemoryBytes();

  if (CommandLine()->FindParm("-minmemory")) return;

  MEMORYSTATUSEX memory_status = {sizeof(MEMORYSTATUSEX)};
  if (GlobalMemoryStatusEx(&memory_status)) {
    host_parms.memsize = memory_status.ullTotalPhys > 0xFFFFFFFFUL
                             ? 0xFFFFFFFFUL
                             : memory_status.ullTotalPhys;
  }

  constexpr usize Mb_128{128 * 1024 * 1024};

  if (host_parms.memsize < Mb_128) {
    Sys_Error("Available memory (%zu) less than 128 MB.\n", host_parms.memsize);
  }

  // take one quarter the physical memory
  if (host_parms.memsize <= 512 * 1024 * 1024) {
    host_parms.memsize >>= 2;
    // Apply cap of 64MB for 512MB systems
    // this keeps the code the same as HL2 gold
    // but allows us to use more memory on 1GB+ systems
    if (host_parms.memsize > MaxDedicatedHeapBytes()) {
      host_parms.memsize = MaxDedicatedHeapBytes();
    }
  } else {
    // just take one quarter, no cap
    host_parms.memsize >>= 2;
  }

  host_parms.memsize = std::clamp(host_parms.memsize, MinHeapMemoryBytes(),
                                  MaxHeapMemoryBytes());
#else
  // FILE *meminfo=fopen("/proc/meminfo","r"); // read in meminfo file?
  // sysinfo() system call??

  // hard code 128 mb for dedicated servers
  host_parms.memsize = MAXIMUM_DEDICATED_MEMORY();
#endif
}

void Sys_ShutdownMemory() { host_parms.memsize = 0; }

// Debug library spew output
thread_local bool g_bInSpew;

SpewRetval_t Sys_SpewFunc(SpewType_t spewType, const ch *pMsg) {
  bool suppress = g_bInSpew;

  g_bInSpew = true;

  ch temp[8192];
  ch *pFrom = (ch *)pMsg;
  ch *pTo = temp;
  // always space for 2 chars plus 0 (ie %%)
  ch *pLimit = &temp[sizeof(temp) - 2];

  while (*pFrom && pTo < pLimit) {
    *pTo = *pFrom++;
    if (*pTo++ == '%') *pTo++ = '%';
  }
  *pTo = 0;

  if (!suppress) {
    // If this is a dedicated server, then we have taken over its spew
    // function, but we still want its vgui console to show the spew, so pass
    // it into the dedicated server.
    if (dedicated) dedicated->Sys_Printf((ch *)pMsg);

    if (g_bTextMode) fprintf_s(stdout, "%s", pMsg);

    if ((spewType != SPEW_LOG) || (sv.GetMaxClients() == 1)) {
      Color color;
      switch (spewType) {
#ifndef SWDS
        case SPEW_WARNING: {
          color.SetColor(255, 90, 90, 255);
        } break;
        case SPEW_ASSERT: {
          color.SetColor(255, 20, 20, 255);
        } break;
        case SPEW_ERROR: {
          color.SetColor(20, 70, 255, 255);
        } break;
#endif
        default: { color = GetSpewOutputColor(); } break;
      }

      Con_ColorPrintf(color, temp);
    } else {
      g_Log.Printf(temp);
    }
  }

  g_bInSpew = false;

  if (spewType == SPEW_ERROR) {
    Sys_Error(temp);
    return SPEW_ABORT;
  }

  if (spewType == SPEW_ASSERT) {
    return CommandLine()->FindParm("-noassert") == 0 ? SPEW_DEBUGGER
                                                     : SPEW_CONTINUE;
  }

  return SPEW_CONTINUE;
}

void OnDeveloperConvarChange(IConVar *con_var, const ch *old_value_string,
                             float old_value_float) {
  // Set the "developer" spew group to the value...
  ConVarRef var{con_var};
  const int val{var.GetInt()};

  SpewActivate("developer", val);

  // Activate console spew (spew value 2 == developer console spew)
  SpewActivate("console", val ? 2 : 1);
}

// Factory conglomeration, gets the client, server, and gameui dlls
// together.
void *GameFactory(const ch *interface_name, int *return_code) {
  // first ask the app factory
  void *an_interface = g_AppSystemFactory(interface_name, return_code);
  if (an_interface) return an_interface;

#ifndef SWDS
    // now ask the client dll
#ifdef OS_WIN
  if (ClientDLL_GetFactory()) {
    an_interface = ClientDLL_GetFactory()(interface_name, return_code);
    if (an_interface) return an_interface;
  }

  // gameui.dll
  if (EngineVGui()->GetGameUIFactory()) {
    an_interface =
        EngineVGui()->GetGameUIFactory()(interface_name, return_code);
    if (an_interface) return an_interface;
  }
#endif
#endif

  // server dll factory access would go here when needed
  return nullptr;
}

// factory instance
CreateInterfaceFn g_GameSystemFactory = GameFactory;

bool Sys_InitGame(CreateInterfaceFn create_interface_fn, const ch *base_dir,
                  HWND *window, bool is_dedicated) {
  InitMathlib();

  FileSystem_SetWhitelistSpewFlags();

  // Activate console spew. Must happen before developer.InstallChangeCallback
  // because that callback may reset it
  SpewActivate("console", 1);

  // Install debug spew output....
  developer.InstallChangeCallback(OnDeveloperConvarChange);

  SpewOutputFunc(Sys_SpewFunc);

  // Assume failure
  host_initialized = false;

#ifdef OS_WIN
  // Grab main window pointer
  pmainwindow = window;
#endif

  // Remember that this is a dedicated server
  s_bIsDedicated = is_dedicated;

  memset(&gmodinfo, 0, sizeof(gmodinfo));

  static ch base_directory[SOURCE_MAX_PATH];
  strcpy_s(base_directory, base_dir);
  Q_strlower(base_directory);
  Q_FixSlashes(base_directory);
  host_parms.basedir = base_directory;

#ifdef OS_POSIX
  if (CommandLine()->FindParm("-pidfile")) {
    FILE *pidFile = g_pFileSystem->Open(
        CommandLine()->ParmValue("-pidfile", "srcds.pid"), "w+");
    if (pidFile) {
      ch dir[SOURCE_MAX_PATH];
      getcwd(dir, sizeof(dir));
      g_pFileSystem->FPrintf(pidFile, "%i\n", getpid());
      g_pFileSystem->Close(pidFile);
    } else {
      Warning("Unable to open pidfile (%s)\n",
              CommandLine()->CheckParm("-pidfile"));
    }
  }
#endif

  // Initialize clock
  TRACEINIT(Sys_Init(), Sys_Shutdown());

#ifndef NDEBUG
  if (!CommandLine()->FindParm("-nodttest") &&
      !CommandLine()->FindParm("-dti")) {
    RunDataTableTest();
  }
#endif

  // NOTE: Can't use COM_CheckParm here because it hasn't been set up yet.
  SeedRandomNumberGenerator(CommandLine()->FindParm("-random_invariant") != 0);

  TRACEINIT(Sys_InitMemory(), Sys_ShutdownMemory());
  TRACEINIT(Host_Init(s_bIsDedicated), Host_Shutdown());

  if (!host_initialized) return false;

  MapReslistGenerator_BuildMapList();

  return true;
}

void Sys_ShutdownGame() {
  TRACESHUTDOWN(Host_Shutdown());
  TRACESHUTDOWN(Sys_ShutdownMemory());
  TRACESHUTDOWN(Sys_Shutdown());

  // Remove debug spew output....
  developer.InstallChangeCallback(nullptr);
  SpewOutputFunc(nullptr);
}

// Backward compatibility
class CServerGameDLLV3 : public IServerGameDLL {
 public:
  CServerGameDLLV3(ServerGameDLLV3::IServerGameDLL *pServerGameDLL)
      : m_pServerGameDLL(pServerGameDLL) {
    m_bInittedSendProxies = false;
  }

  virtual bool DLLInit(CreateInterfaceFn engineFactory,
                       CreateInterfaceFn physicsFactory,
                       CreateInterfaceFn fileSystemFactory,
                       CGlobalVars *pGlobals) {
    return m_pServerGameDLL->DLLInit(engineFactory, physicsFactory,
                                     fileSystemFactory, pGlobals);
  }

  virtual bool GameInit() { return m_pServerGameDLL->GameInit(); }

  virtual bool LevelInit(ch const *pMapName, ch const *pMapEntities,
                         ch const *pOldLevel, ch const *pLandmarkName,
                         bool loadGame, bool background) {
    return m_pServerGameDLL->LevelInit(pMapName, pMapEntities, pOldLevel,
                                       pLandmarkName, loadGame, background);
  }

  virtual void ServerActivate(edict_t *pEdictList, int edictCount,
                              int clientMax) {
    m_pServerGameDLL->ServerActivate(pEdictList, edictCount, clientMax);
  }

  virtual void GameFrame(bool simulating) {
    m_pServerGameDLL->GameFrame(simulating);
  }

  virtual void PreClientUpdate(bool simulating) {
    m_pServerGameDLL->PreClientUpdate(simulating);
  }

  virtual void LevelShutdown() { m_pServerGameDLL->LevelShutdown(); }

  virtual void GameShutdown() { m_pServerGameDLL->GameShutdown(); }

  virtual void DLLShutdown() { m_pServerGameDLL->DLLShutdown(); }

  virtual float GetTickInterval() const {
    return m_pServerGameDLL->GetTickInterval();
  }

  virtual ServerClass *GetAllServerClasses() {
    return m_pServerGameDLL->GetAllServerClasses();
  }

  virtual const ch *GetGameDescription() {
    return m_pServerGameDLL->GetGameDescription();
  }

  virtual void CreateNetworkStringTables() {
    return m_pServerGameDLL->CreateNetworkStringTables();
  }

  virtual CSaveRestoreData *SaveInit(int size) {
    return m_pServerGameDLL->SaveInit(size);
  }

  virtual void SaveWriteFields(CSaveRestoreData *s, const ch *c, void *v,
                               datamap_t *d, typedescription_t *t, int i) {
    return m_pServerGameDLL->SaveWriteFields(s, c, v, d, t, i);
  }

  virtual void SaveReadFields(CSaveRestoreData *s, const ch *c, void *v,
                              datamap_t *d, typedescription_t *t, int i) {
    return m_pServerGameDLL->SaveReadFields(s, c, v, d, t, i);
  }

  virtual void SaveGlobalState(CSaveRestoreData *s) {
    m_pServerGameDLL->SaveGlobalState(s);
  }

  virtual void RestoreGlobalState(CSaveRestoreData *s) {
    m_pServerGameDLL->RestoreGlobalState(s);
  }

  virtual void PreSave(CSaveRestoreData *s) { m_pServerGameDLL->PreSave(s); }

  virtual void Save(CSaveRestoreData *s) { m_pServerGameDLL->Save(s); }

  virtual void GetSaveComment(ch *comment, int maxlength, float flMinutes,
                              float flSeconds, bool bNoTime = false) {
    m_pServerGameDLL->GetSaveComment(comment, maxlength);
  }

  virtual void PreSaveGameLoaded(ch const *pSaveName, bool bCurrentlyInGame) {}

  virtual bool ShouldHideServer() { return false; }

  virtual void InvalidateMdlCache() {}

  virtual void GetSaveCommentEx(ch *comment, int maxlength, float flMinutes,
                                float flSeconds) {
    m_pServerGameDLL->GetSaveComment(comment, maxlength);
  }

  virtual void WriteSaveHeaders(CSaveRestoreData *s) {
    m_pServerGameDLL->WriteSaveHeaders(s);
  }

  virtual void ReadRestoreHeaders(CSaveRestoreData *s) {
    m_pServerGameDLL->ReadRestoreHeaders(s);
  }

  virtual void Restore(CSaveRestoreData *s, bool b) {
    m_pServerGameDLL->Restore(s, b);
  }

  virtual bool IsRestoring() { return m_pServerGameDLL->IsRestoring(); }

  virtual int CreateEntityTransitionList(CSaveRestoreData *s, int i) {
    return m_pServerGameDLL->CreateEntityTransitionList(s, i);
  }

  virtual void BuildAdjacentMapList() {
    m_pServerGameDLL->BuildAdjacentMapList();
  }

  virtual bool GetUserMessageInfo(int msg_type, ch *name, int maxnamelength,
                                  int &size) {
    return m_pServerGameDLL->GetUserMessageInfo(msg_type, name, maxnamelength,
                                                size);
  }

  virtual CStandardSendProxies *GetStandardSendProxies() {
    if (!m_bInittedSendProxies) {
      memset(&m_SendProxies, 0, sizeof(m_SendProxies));

      // Copy the version 1 info into the structure we export from here.
      CStandardSendProxiesV1 &out = m_SendProxies;
      const CStandardSendProxiesV1 &in =
          *m_pServerGameDLL->GetStandardSendProxies();
      out = in;

      m_bInittedSendProxies = true;
    }
    return &m_SendProxies;
  }

  virtual void PostInit() {}

  virtual void Think(bool finalTick) {}

  virtual void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie,
                                        edict_t *pPlayerEntity,
                                        EQueryCvarValueStatus eStatus,
                                        const ch *pCvarName,
                                        const ch *pCvarValue) {}

 private:
  ServerGameDLLV3::IServerGameDLL *m_pServerGameDLL;
  bool m_bInittedSendProxies;
  CStandardSendProxies m_SendProxies;
};

// Try to load a single DLL.  If it conforms to spec, keep it loaded, and add
// relevant info to the DLL directory.  If not, ignore it entirely.
CreateInterfaceFn g_ServerFactory;

static bool LoadThisDll(const ch *dll_name) {
  CSysModule *module = g_pFileSystem->LoadModule(dll_name, "GAMEBIN", false);

  // Load DLL, ignore if cannot
  // ensures that the game.dll is running under Steam
  // this will have to be undone when we want mods to be able to run
  if (module == nullptr) {
    ConMsg("Failed to load %s\n", dll_name);
    goto IgnoreThisDLL;
  }

  // Load interface factory and any interfaces exported by the game .dll
  g_ServerFactory = Sys_GetFactory(module);
  if (g_ServerFactory) {
    g_bServerGameDLLGreaterThanV5 = true;
    g_bServerGameDLLGreaterThanV4 = true;

    serverGameDLL = static_cast<IServerGameDLL *>(
        g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL, nullptr));
    if (!serverGameDLL) {
#ifdef REL_TO_STAGING_MERGE_TODO
      // Need to merge eiface for this.
      g_bServerGameDLLGreaterThanV5 = false;
      serverGameDLL = static_cast<IServerGameDLL *>(
          g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL_VERSION_5, nullptr));
      if (!serverGameDLL) {
#endif
        g_bServerGameDLLGreaterThanV4 = false;
        serverGameDLL = static_cast<IServerGameDLL *>(
            g_ServerFactory(INTERFACEVERSION_SERVERGAMEDLL_VERSION_4, nullptr));

        if (!serverGameDLL) {
          auto *pServerGameDLLV3 =
              static_cast<ServerGameDLLV3::IServerGameDLL *>(
                  g_ServerFactory(SERVERGAMEDLL_INTERFACEVERSION_3, nullptr));
          if (!pServerGameDLLV3) {
            ConMsg("Could not get IServerGameDLL interface from library %s",
                   dll_name);
            goto IgnoreThisDLL;
          }
          serverGameDLL = new CServerGameDLLV3(pServerGameDLLV3);
        }
#ifdef REL_TO_STAGING_MERGE_TODO
      }
#endif
    }

    serverGameEnts = static_cast<IServerGameEnts *>(
        g_ServerFactory(INTERFACEVERSION_SERVERGAMEENTS, nullptr));
    if (!serverGameEnts) {
      ConMsg("Could not get IServerGameEnts interface from library %s",
             dll_name);
      goto IgnoreThisDLL;
    }

    serverGameClients = (IServerGameClients *)g_ServerFactory(
        INTERFACEVERSION_SERVERGAMECLIENTS, nullptr);
    if (serverGameClients) {
      g_iServerGameClientsVersion = 4;
    } else {
      // Try the previous version.
      const ch *pINTERFACEVERSION_SERVERGAMECLIENTS_V3 = "ServerGameClients003";
      serverGameClients = (IServerGameClients *)g_ServerFactory(
          pINTERFACEVERSION_SERVERGAMECLIENTS_V3, nullptr);
      if (serverGameClients) {
        g_iServerGameClientsVersion = 3;
      } else {
        ConMsg("Could not get IServerGameClients interface from library %s",
               dll_name);
        goto IgnoreThisDLL;
      }
    }
    serverGameDirector = (IHLTVDirector *)g_ServerFactory(
        INTERFACEVERSION_HLTVDIRECTOR, nullptr);
    if (!serverGameDirector) {
      ConMsg("Could not get IHLTVDirector interface from library %s", dll_name);
      // this is not a critical
    }
  } else {
    ConMsg("Could not find factory interface in library %s", dll_name);
    goto IgnoreThisDLL;
  }

  g_GameDLL = module;
  return true;

IgnoreThisDLL:
  if (module != nullptr) {
    g_pFileSystem->UnloadModule(module);
    serverGameDLL = nullptr;
    serverGameEnts = nullptr;
    serverGameClients = nullptr;
  }
  return false;
}

// Scan DLL directory, load all DLLs that conform to spec.
void LoadEntityDLLs(const ch *the_base_dir) {
  memset(&gmodinfo, 0, sizeof(gmodinfo));

  gmodinfo.version = 1;
  gmodinfo.svonly = true;

  // Run through all DLLs found in the extension DLL directory
  g_GameDLL = nullptr;
  sv_noclipduringpause = nullptr;

  // Listing file for this game.
  KeyValues *modinfo = new KeyValues("modinfo");
  if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt")) {
    strcpy_s(gmodinfo.szInfo, modinfo->GetString("url_info"));
    strcpy_s(gmodinfo.szDL, modinfo->GetString("url_dl"));

    gmodinfo.version = modinfo->GetInt("version");
    gmodinfo.size = modinfo->GetInt("size");
    gmodinfo.svonly = modinfo->GetInt("svonly") ? true : false;
    gmodinfo.cldll = modinfo->GetInt("cldll") ? true : false;

    strcpy_s(gmodinfo.szHLVersion, modinfo->GetString("hlversion"));
  }
  modinfo->deleteThis();

  const ch *server_dll_name{
#ifdef OS_WIN
      "server.dll"
#elif OS_POSIX
      "server.so.1"
#else
#error Please, define server dll name in engine/sys_dll.cpp
#endif
  };

  LoadThisDll(server_dll_name);

  if (serverGameDLL) {
    Msg("Server library loaded for \"%s\".\n",
        serverGameDLL->GetGameDescription());
  }
}  //-V773

// Retrieves a string value from the registry
#ifdef OS_WIN
ch string_type_name[] = {'S', 't', 'r', 'i', 'n', 'g', '\0'};

LONG Sys_GetRegKeyValueUnderRoot(HKEY root_key, const ch *sub_key,
                                 const ch *element, ch *return_value,
                                 int return_size, const ch *default_value) {
  HKEY key;
  ULONG disposition;

  // Assume the worst.
  strcpy_s(return_value, return_size, default_value);

  // Create it if it doesn't exist.  (Create opens the key otherwise)
  LONG result_code = VCRHook_RegCreateKeyEx(
      root_key,                 // handle of open key
      sub_key,                  // address of name of subkey to open
      0ul,                      // DWORD ulOptions,	  // reserved
      string_type_name,         // Type of value
      REG_OPTION_NON_VOLATILE,  // Store permanently in reg.
      KEY_ALL_ACCESS,           // REGSAM samDesired, // security access mask
      nullptr, &key, &disposition);

  if (result_code == ERROR_SUCCESS) {
    if (disposition == REG_CREATED_NEW_KEY) {
      DWORD key_type, key_size = return_size;
      ch key_buffer[128];

      // We opened the existing key. Now go ahead and find out how big the key
      // is.
      result_code = VCRHook_RegQueryValueEx(key, element, 0, &key_type,
                                            (u8 *)key_buffer, &key_size);

      if (result_code == ERROR_SUCCESS) {
        // Only copy strings, and only copy as much data as requested.
        if (key_type == REG_SZ) strcpy_s(return_value, return_size, key_buffer);
      } else {
        // Didn't find it, so write out new value.  Just set the values
        // according to the defaults
        result_code = VCRHook_RegSetValueEx(key, element, 0, REG_SZ,
                                            (const u8 *)default_value,
                                            strlen(default_value) + 1);
      }
    } else {
      // First time, just set to Valve default.
      result_code = VCRHook_RegSetValueEx(key, element, 0, REG_SZ,
                                          (const u8 *)default_value,
                                          strlen(default_value) + 1);
    }

    // Always close this key before exiting.
    VCRHook_RegCloseKey(key);
  }

  return result_code;
}

// Purpose: Retrieves a DWORD value from the registry
LONG Sys_GetRegKeyValueUnderRootInt(HKEY root_key, const ch *sub_key,
                                    const ch *element, long *return_Value,
                                    const long default_value) {
  HKEY key;
  ULONG disposition;

  // Assume the worst
  // Set the return value to the default
  *return_Value = default_value;

  // Create it if it doesn't exist.  (Create opens the key otherwise)
  LONG result_code = VCRHook_RegCreateKeyEx(
      root_key,                 // handle of open key
      sub_key,                  // address of name of subkey to open
      0ul,                      // DWORD ulOptions,	  // reserved
      string_type_name,         // Type of value
      REG_OPTION_NON_VOLATILE,  // Store permanently in reg.
      KEY_ALL_ACCESS,           // REGSAM samDesired, // security access mask
      nullptr, &key, &disposition);

  if (result_code == ERROR_SUCCESS) {
    if (disposition == REG_CREATED_NEW_KEY) {
      DWORD type, size = sizeof(DWORD);

      // We opened the existing key.  Now go ahead and find out how big the
      // key is.
      result_code = VCRHook_RegQueryValueEx(key, element, 0, &type,
                                            (u8 *)return_Value, &size);

      if (result_code != ERROR_SUCCESS) {
        // Didn't find it, so write out new value.  Just set the values
        // according to the defaults
        result_code = VCRHook_RegSetValueEx(
            key, element, 0, REG_DWORD, (u8 *)&default_value, sizeof(DWORD));
      }
    } else {
      // First time, just set to Valve default.
      result_code =
          VCRHook_RegSetValueEx(key, element, 0, REG_DWORD,
                                (const u8 *)&default_value, sizeof(DWORD));
    }

    // Always close this key before exiting.
    VCRHook_RegCloseKey(key);
  }

  return result_code;
}

LONG Sys_SetRegKeyValueUnderRoot(HKEY root_key, const ch *sub_key,
                                 const ch *element, const ch *value) {
  HKEY key;
  ULONG disposition;

  // Create it if it doesn't exist.  (Create opens the key otherwise)
  LONG result_code = VCRHook_RegCreateKeyEx(
      root_key,                 // handle of open key
      sub_key,                  // address of name of subkey to open
      0ul,                      // DWORD ulOptions,	  // reserved
      string_type_name,         // Type of value
      REG_OPTION_NON_VOLATILE,  // Store permanently in reg.
      KEY_ALL_ACCESS,           // REGSAM samDesired, // security access mask
      nullptr, &key, &disposition);

  if (result_code == ERROR_SUCCESS) {
    // Just Set the Values according to the defaults
    result_code = VCRHook_RegSetValueEx(key, element, 0, REG_SZ,
                                        (const u8 *)value, strlen(value) + 1);

    // Always close this key before exiting.
    VCRHook_RegCloseKey(key);
  }

  return result_code;
}
#endif

bool Sys_GetRegKeyValue(const ch *sub_key, const ch *element, ch *return_value,
                        int return_size, const ch *default_value) {
#ifdef OS_WIN
  return Sys_GetRegKeyValueUnderRoot(HKEY_CURRENT_USER, sub_key, element,
                                     return_value, return_size,
                                     default_value) == ERROR_SUCCESS;
#else
  return true;
#endif
}

bool Sys_GetRegKeyValueInt(ch *sub_key, const ch *element, long *return_value,
                           long default_value) {
#ifdef OS_WIN
  return Sys_GetRegKeyValueUnderRootInt(HKEY_CURRENT_USER, sub_key, element,
                                        return_value,
                                        default_value) == ERROR_SUCCESS;
#else
  return true;
#endif
}

bool Sys_SetRegKeyValue(const ch *sub_key, const ch *element, const ch *value) {
#ifdef OS_WIN
  return Sys_SetRegKeyValueUnderRoot(HKEY_CURRENT_USER, sub_key, element,
                                     value) == ERROR_SUCCESS;
#else
  return true;
#endif
}

bool Sys_CreateFileAssociations(usize count, FileAssociationInfo *list) {
#ifdef OS_WIN
  const ch *kSourceEngineAppClass{"Valve.Source"};
  ch app_name[SOURCE_MAX_PATH];

  GetModuleFileName(0, app_name, sizeof(app_name));
  Q_FixSlashes(app_name);
  Q_strlower(app_name);

  ch quoted_appname_with_arg[512];
  sprintf_s(quoted_appname_with_arg, "\"%s\" \"%%1\"", app_name);

  // HKEY_CLASSES_ROOT/Valve.Source/shell/open/command == "u:\tf2\hl2.exe"
  // "%1" quoted
  LONG result_code = Sys_SetRegKeyValueUnderRoot(
      HKEY_CLASSES_ROOT, va("%s\\shell\\open\\command", kSourceEngineAppClass),
      "", quoted_appname_with_arg);

  if (result_code == ERROR_SUCCESS) {
    ch base_exe_name[256];
    Q_FileBase(app_name, base_exe_name, sizeof(base_exe_name));
    Q_DefaultExtension(base_exe_name, ".exe", sizeof(base_exe_name));

    // HKEY_CLASSES_ROOT/Applications/hl2.exe/shell/open/command ==
    // "u:\tf2\hl2.exe" "%1" quoted
    result_code = Sys_SetRegKeyValueUnderRoot(
        HKEY_CLASSES_ROOT,
        va("Applications\\%s\\shell\\open\\command", base_exe_name), "",
        quoted_appname_with_arg);
  }

  for (usize i = 0; i < count; ++i) {
    // Assume always success.
    if (result_code == ERROR_SUCCESS) {
      FileAssociationInfo *fa = &list[i];
      ch binding[32];
      binding[0] = '\0';

      // Create file association for our .exe
      // HKEY_CLASSES_ROOT/.dem == "Valve.Source"
      result_code = Sys_GetRegKeyValueUnderRoot(
          HKEY_CLASSES_ROOT, fa->extension, "", binding, sizeof(binding), "");

      if (result_code == ERROR_SUCCESS && strlen(binding) == 0) {
        result_code = Sys_SetRegKeyValueUnderRoot(
            HKEY_CLASSES_ROOT, fa->extension, "", kSourceEngineAppClass);
      }
    }
  }

  return result_code == ERROR_SUCCESS;
#else
  return true;
#endif
}

void Sys_TestSendKey(const ch *key) {
#ifdef OS_WIN
  int the_key = key[0];
  if (key[0] == '\\' && key[1] == 'r') {
    the_key = VK_RETURN;
  }

  HWND hWnd = (HWND)game->GetMainWindow();

  PostMessageA(hWnd, WM_KEYDOWN, the_key, 0);
  PostMessageA(hWnd, WM_KEYUP, the_key, 0);
#endif
}

void UnloadEntityDLLs() {
  if (!g_GameDLL) return;

  // Unlink the cvars associated with game DLL
  FileSystem_UnloadModule(g_GameDLL);
  g_GameDLL = nullptr;
  serverGameDLL = nullptr;
  serverGameEnts = nullptr;
  serverGameClients = nullptr;
  sv_noclipduringpause = nullptr;
}

CON_COMMAND(star_memory, "Dump memory stats") {
// get a current stat of available memory
// 32 MB is reserved and fixed by OS, so not reporting to allow memory loggers
// sync
#ifdef OS_POSIX
  struct mallinfo memstats = mallinfo();
  Msg("sbrk size: %.2f MB, Used: %.2f MB, #mallocs = %d\n",
      memstats.arena / (1024.0 * 1024.0), memstats.uordblks / (1024.0 * 1024.0),
      memstats.hblks);
#else
  MEMORYSTATUSEX stat{sizeof(MEMORYSTATUSEX)};
  if (GlobalMemoryStatusEx(&stat)) {
    Msg("Available: %.2f MB, Used: %.2f MB, Free: %.2f MB.\n",
        stat.ullTotalPhys / (1024.0f * 1024.0f) - 32.0f,
        (stat.ullTotalPhys - stat.ullAvailPhys) / (1024.0f * 1024.0f) - 32.0f,
        stat.ullAvailPhys / (1024.0f * 1024.0f));
  } else {
    Warning("Dump memory stats failed: %s.\n",
            source::windows::windows_errno_info_last_error().description);
  }
#endif
}
