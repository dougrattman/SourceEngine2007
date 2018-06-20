// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"
#endif

#include <tuple>
#include "DevShotGenerator.h"
#include "IHammer.h"
#include "MapReslistGenerator.h"
#include "appframework/IAppSystemGroup.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "base/include/windows/scoped_se_translator.h"
#include "cdll_engine_int.h"
#include "cl_main.h"
#include "client.h"
#include "common.h"
#include "datacache/idatacache.h"
#include "engine_hlds_api.h"
#include "engine_launcher_api.h"
#include "filesystem_engine.h"
#include "gl_shader.h"
#include "idedicatedexports.h"
#include "iengine.h"
#include "igame.h"
#include "inputsystem/iinputsystem.h"
#include "iregistry.h"
#include "ivideomode.h"
#include "keys.h"
#include "l_studio.h"
#include "materialsystem/materialsystem_config.h"
#include "quakedef.h"
#include "server.h"
#include "sys_dll.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/minidump.h"
#include "tier0/include/vcrmode.h"
#include "tier3/tier3.h"
#include "toolframework/itoolframework.h"
#include "traceinit.h"
#include "vphysics_interface.h"

#ifdef OS_WIN
#include "vguimatsurface/imatsystemsurface.h"
#endif

// This is here just for legacy support of older .dlls!!!
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "eiface.h"

#ifndef SWDS
#include "cl_steamauth.h"
#include "igameuifuncs.h"
#include "sys_mainwind.h"
#include "vgui/isystem.h"
#include "vgui_controls/controls.h"
#endif  // SWDS

#ifdef OS_WIN
#include <eh.h>
#endif

#include "xbox/xboxstubs.h"

#include "tier0/include/memdbgon.h"

// Globals
IDedicatedExports *dedicated = nullptr;
extern CreateInterfaceFn g_AppSystemFactory;
IHammer *g_pHammer = nullptr;
IPhysics *g_pPhysics = nullptr;
IAvi *avi = nullptr;
IBik *bik = nullptr;

#ifndef SWDS
extern CreateInterfaceFn g_ClientFactory;
#endif

std::tuple<f32, i32, i32> Host_GetHostInfo(ch *map_name, usize map_name_size);
const char *Key_BindingForKey(int keynum);
void COM_ShutdownFileSystem(void);
void COM_InitFilesystem(const char *pFullModPath);
void Host_ReadPreStartupConfiguration();
void EditorToggle_f();

#ifdef OS_WIN
HWND *pmainwindow = nullptr;
#endif

// ConVars and console commands

#if !defined(SWDS)
static ConCommand editor_toggle(
    "editor_toggle", EditorToggle_f,
    "Disables the simulation and returns focus to the editor", FCVAR_CHEAT);
#endif

#ifndef SWDS
// Purpose: exports an interface that can be used by the launcher to run the
// engine	this is the exported function when compiled as a blob
void EXPORT F(IEngineAPI **api) {
  *api = reinterpret_cast<IEngineAPI *>(
      Sys_GetFactoryThis()(VENGINE_LAUNCHER_API_VERSION, nullptr));
}
#endif  // SWDS

extern bool cs_initialized;
extern int lowshift;
static const char *empty_string = "";

extern void SCR_UpdateScreen();
extern bool g_bMajorMapChange;
extern bool g_bPrintingKeepAliveDots;

void Sys_ShowProgressTicks(char *specialProgressMsg) {
#ifdef LATER
#define MAX_NUM_TICS 40

  static long numTics = 0;

  // Nothing to do if not using Steam
  if (!g_pFileSystem->IsSteam()) return;

  // Update number of tics to show...
  numTics++;
  if (isDedicated) {
    if (g_bMajorMapChange) {
      g_bPrintingKeepAliveDots = TRUE;
      Msg(".");
    }
  } else {
    int i;
    int numTicsToPrint = numTics % (MAX_NUM_TICS - 1);
    char msg[MAX_NUM_TICS + 1];

    Q_strncpy(msg, ".", sizeof(msg));

    // Now add in the growing number of tics...
    for (i = 1; i < numTicsToPrint; i++) {
      Q_strncat(msg, ".", sizeof(msg), COPY_ALL_CHARACTERS);
    }

    SCR_UpdateScreen();
  }
#endif
}

void ClearIOStates() {
#ifndef SWDS
  if (g_ClientDLL) {
    g_ClientDLL->IN_ClearStates();
  }
#endif
}

void MoveConsoleWindowToFront() {
#ifdef OS_WIN
  // TODO: remove me!!!!!

  // Move the window to the front.
  HWND hwnd = GetConsoleWindow();
  if (hwnd) {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  }
#endif
}

#ifdef OS_WIN
#include <conio.h>
#endif

CUtlVector<char> g_TextModeLine;

char NextGetch() { return -1; }

void EatTextModeKeyPresses() {
  if (!g_bTextMode) return;

  static bool bFirstRun = true;
  if (bFirstRun) {
    bFirstRun = false;
    MoveConsoleWindowToFront();
  }

  char ch;
  while ((ch = NextGetch()) != -1) {
    if (ch == 8) {
      // Backspace..
      if (g_TextModeLine.Count()) {
        g_TextModeLine.Remove(g_TextModeLine.Count() - 1);
      }
    } else if (ch == '\r') {
      // Finish the line.
      if (g_TextModeLine.Count()) {
        g_TextModeLine.AddMultipleToTail(2, "\n");
        Cbuf_AddText(g_TextModeLine.Base());
        g_TextModeLine.Purge();
      }
      printf("\n");
    } else {
      g_TextModeLine.AddToTail(ch);
    }

    printf("%c", ch);
  }
}

// The SDK launches the game with the full path to gameinfo.txt, so we need
// to strip off the path.
const char *GetModDirFromPath(const char *pszPath) {
  char *pszSlash = Q_strrchr(pszPath, '\\');
  if (pszSlash) {
    return pszSlash + 1;
  } else if ((pszSlash = Q_strrchr(pszPath, '/')) != nullptr) {
    return pszSlash + 1;
  }

  // Must just be a mod directory already.
  return pszPath;
}

// Purpose: Main entry
#ifndef SWDS
#include "gl_matsysiface.h"
#endif

void SV_ShutdownGameDLL();

bool g_bUsingLegacyAppSystems = false;

// Inner loop: initialize, shutdown main systems, load steam to
class CModAppSystemGroup : public CAppSystemGroup {
  using BaseClass = CAppSystemGroup;

 public:
  CModAppSystemGroup(bool bServerOnly,
                     CAppSystemGroup *pParentAppSystem = nullptr)
      : BaseClass(pParentAppSystem), is_server_only_(bServerOnly) {}

  CreateInterfaceFn GetFactory() { return CAppSystemGroup::GetFactory(); }

  // Methods of IApplication
  bool Create() override {
#ifndef SWDS
    if (!IsServerOnly() && !ClientDLL_Load()) return false;
#endif

    if (!ServerDLL_Load()) return false;

    IClientDLLSharedAppSystems *client_shared = nullptr;

#ifndef SWDS
    if (!IsServerOnly()) {
      client_shared = static_cast<IClientDLLSharedAppSystems *>(
          g_ClientFactory(CLIENT_DLL_SHARED_APPSYSTEMS, nullptr));
      if (!client_shared) return AddLegacySystems();
    }
#endif

    auto *server_shared = static_cast<IServerDLLSharedAppSystems *>(
        g_ServerFactory(SERVER_DLL_SHARED_APPSYSTEMS, nullptr));
    if (!server_shared) {
      Assert(!"Expected both game and client .dlls to have or not have shared app systems interfaces!!!");
      return AddLegacySystems();
    }

    // Load game and client .dlls and build list then
    CUtlVector<AppSystemInfo_t> systems;

    int serverCount = server_shared->Count();
    for (int i = 0; i < serverCount; ++i) {
      const char *dllName = server_shared->GetDllName(i);
      const char *interfaceName = server_shared->GetInterfaceName(i);

      systems.AddToTail({dllName, interfaceName});
    }

    if (!IsServerOnly()) {
      int clientCount = client_shared->Count();
      for (int i = 0; i < clientCount; ++i) {
        const char *dllName = client_shared->GetDllName(i);
        const char *interfaceName = client_shared->GetInterfaceName(i);

        if (ModuleAlreadyInList(systems, dllName, interfaceName)) continue;

        systems.AddToTail({dllName, interfaceName});
      }
    }

    systems.AddToTail({"", ""});

    if (!AddSystems(systems.Base())) return false;

#ifndef OS_POSIX
    AppModule_t toolFrameworkModule = LoadModule("engine.dll");
    if (!AddSystem(toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION))
      return false;
#endif

    return true;
  }

  bool PreInit() override { return true; }

  int Main() override;

  void PostShutdown() override {}

  void Destroy() override {
    // unload game and client .dlls
    ServerDLL_Unload();
#ifndef SWDS
    if (!IsServerOnly()) {
      ClientDLL_Unload();
    }
#endif
  }

 private:
  bool IsServerOnly() const { return is_server_only_; }

  bool ModuleAlreadyInList(CUtlVector<AppSystemInfo_t> &list,
                           const char *moduleName, const char *interfaceName) {
    for (int i = 0; i < list.Count(); ++i) {
      if (!_stricmp(list[i].m_pModuleName, moduleName)) {
        if (_stricmp(list[i].m_pInterfaceName, interfaceName)) {
          Error(
              "Game and client .dlls requesting different versions '%s' vs. "
              "'%s' "
              "from '%s'\n",
              list[i].m_pInterfaceName, interfaceName, moduleName);
        }
        return true;
      }
    }

    return false;
  }

  bool AddLegacySystems() {
    g_bUsingLegacyAppSystems = true;

    AppSystemInfo_t appSystems[] = {
        {"soundemittersystem", SOUNDEMITTERSYSTEM_INTERFACE_VERSION},
        {"", ""}  // Required to terminate the list
    };

    if (!AddSystems(appSystems)) return false;

#ifndef OS_POSIX
    AppModule_t toolFrameworkModule = LoadModule("engine.dll");
    if (!AddSystem(toolFrameworkModule, VTOOLFRAMEWORK_INTERFACE_VERSION))
      return false;
#endif

    return true;
  }

  const bool is_server_only_;
};

#ifndef SWDS
extern void S_ClearBuffer();

void __cdecl WriteMiniDump(unsigned int uStructuredExceptionCode,
                           EXCEPTION_POINTERS *pExceptionInfo) {
  WriteMiniDumpUsingExceptionInfo(uStructuredExceptionCode, pExceptionInfo,
                                  MINIDUMP_TYPE::MiniDumpNormal);

  // Clear DSound Buffers so the sound doesn't loop while the game shuts
  // down
  S_ClearBuffer();
}

static bool IsValveMod(const char *pModName) {
  const ch *current_mod{GetCurrentMod()};
  // Figure out if we're running a Valve mod or not.
  return (_stricmp(current_mod, "cstrike") == 0 ||
          _stricmp(current_mod, "dod") == 0 ||
          _stricmp(current_mod, "hl1mp") == 0 ||
          _stricmp(current_mod, "tf") == 0 ||
          _stricmp(current_mod, "hl2mp") == 0);
}

// Main engine interface exposed to launcher
class CEngineAPI : public CTier3AppSystem<IEngineAPI> {
  using BaseClass = CTier3AppSystem<IEngineAPI>;

 public:
  // Connect, disconnect
  bool Connect(CreateInterfaceFn factory) override {
    // Store off the app system factory...
    g_AppSystemFactory = factory;

    if (!BaseClass::Connect(factory)) return false;

    g_pFileSystem = g_pFullFileSystem;
    if (!g_pFileSystem) return false;

    g_pFileSystem->SetWarningFunc(Warning);

    if (!Shader_Connect(true)) return false;

    g_pPhysics = (IPhysics *)factory(VPHYSICS_INTERFACE_VERSION, nullptr);

    avi = (IAvi *)factory(AVI_INTERFACE_VERSION, nullptr);
    if (!avi) return false;

    bik = (IBik *)factory(BIK_INTERFACE_VERSION, nullptr);
    if (!bik) return false;

    if (!g_pStudioRender || !g_pDataCache || !g_pPhysics || !g_pMDLCache ||
        !g_pMatSystemSurface || !g_pInputSystem) {
      Warning("Engine wasn't able to acquire required interfaces!\n");
      return false;
    }

    g_pHammer = (IHammer *)factory(INTERFACEVERSION_HAMMER, nullptr);

    ConnectMDLCacheNotify();

    return true;
  }

  void Disconnect() override {
    DisconnectMDLCacheNotify();

    g_pHammer = nullptr;
    bik = nullptr;
    avi = nullptr;
    g_pPhysics = nullptr;

    Shader_Disconnect();

    g_pFileSystem = nullptr;

    BaseClass::Disconnect();

    g_AppSystemFactory = nullptr;
  }

  // Query interface
  void *QueryInterface(const char *pInterfaceName) override {
    // Loading the engine DLL mounts *all* engine interfaces
    return Sys_GetFactoryThis()(pInterfaceName, nullptr);
  }

  // Init, shutdown
  InitReturnVal_t Init() override {
    InitReturnVal_t nRetVal = BaseClass::Init();
    if (nRetVal != INIT_OK) return nRetVal;

    is_running_simulation_ = false;

    // This creates the videomode singleton object, it doesn't depend on the
    // registry
    VideoMode_Create();

    // Initialize the editor hwnd to render into
    editor_hwnd_ = nullptr;

    // One-time setup
    // TODO(d.rattman): OnStartup + OnShutdown should be removed + moved into
    // the launcher or the launcher code should be merged into the engine into
    // the code in OnStartup/OnShutdown
    if (!OnStartup(startup_info_.m_pInstance, startup_info_.m_pInitialMod)) {
      return HandleSetModeError();
    }

    return INIT_OK;
  }

  void Shutdown() override {
    VideoMode_Destroy();
    BaseClass::Shutdown();
  }

  // This function must be called before init
  void SetStartupInfo(StartupInfo_t &info) override {
    g_bTextMode = info.m_bTextMode;

    // Set up the engineparms_t which contains global information about the mod
    host_parms.basedir = const_cast<char *>(info.m_pBaseDirectory);

    // Copy off all the startup info
    startup_info_ = info;

    // Needs to be done prior to init material system config
    TRACEINIT(COM_InitFilesystem(startup_info_.m_pInitialMod),
              COM_ShutdownFileSystem());
  }

  int Run() override {
#ifdef OS_WIN
    if (!Plat_IsInDebugSession() && !CommandLine()->FindParm("-nominidumps")) {
      const source::windows::ScopedSeTranslator scoped_se_translator{
          WriteMiniDump};

      // This try block allows the SE translator to work.
      try {
        return RunListenServer();
      } catch (...) {
        return RUN_OK;
      }
    }

    return RunListenServer();
#else
    return RUN_OK;  // linux doesn't do this
#endif
  }

  // Sets the engine to run in a particular editor window
  void SetEngineWindow(HWND hWnd) override {
    if (!InEditMode()) return;

    // Detach input from the previous editor window
    game->InputDetachFromGameWindow();

    editor_hwnd_ = hWnd;
    videomode->SetGameWindow(editor_hwnd_);
  }

  // Posts a console command
  void PostConsoleCommand(const char *pCommand) override {
    Cbuf_AddText(pCommand);
  }

  // Are we running the simulation?
  bool IsRunningSimulation() const override {
    return eng->GetState() == IEngine::DLL_ACTIVE;
  }

  // Start/stop running the simulation
  void ActivateSimulation(bool bActive) override {
    // TODO(d.rattman): Not sure what will happen in this case
    if (eng->GetState() != IEngine::DLL_ACTIVE &&
        eng->GetState() != IEngine::DLL_PAUSED) {
      return;
    }

    bool bCurrentlyActive = eng->GetState() != IEngine::DLL_PAUSED;
    if (bActive == bCurrentlyActive) return;

    // TODO(d.rattman): Should attachment/detachment be part of the state
    // machine in IEngine?
    if (!bActive) {
      eng->SetNextState(IEngine::DLL_PAUSED);

      // Detach input from the previous editor window
      game->InputDetachFromGameWindow();
    } else {
      eng->SetNextState(IEngine::DLL_ACTIVE);

      // Start accepting input from the new window
      // TODO(d.rattman): What if the attachment fails?
      game->InputAttachToGameWindow();
    }
  }

  // Reset the map we're on
  void SetMap(const char *pMapName) override {
    char map_buffer[SOURCE_MAX_PATH];
    sprintf_s(map_buffer, "map %s", pMapName);
    Cbuf_AddText(map_buffer);
  }

  // Purpose: Message pump
  bool MainLoop() {
    bool bIdle = true;
    long lIdleCount = 0;

    // Main message pump
    while (true) {
      // Pump messages unless someone wants to quit
      if (eng->GetQuitting() != IEngine::QUIT_NOTQUITTING) {
        if (eng->GetQuitting() != IEngine::QUIT_TODESKTOP) return true;
        return false;
      }

      // Pump the message loop
      if (!InEditMode()) {
        PumpMessages();
      } else {
        PumpMessagesEditMode(bIdle, lIdleCount);
      }

      // Run engine frame + hammer frame
      if (!InEditMode() || editor_hwnd_) {
        VCRSyncToken("Frame");

        // Deactivate edit mode shaders
        ActivateEditModeShaders(false);

        eng->Frame();

        // Reactivate edit mode shaders (in Edit mode only...)
        ActivateEditModeShaders(true);
      }

      if (InEditMode()) {
        g_pHammer->RunFrame();
      }
    }

    return false;
  }

 private:
  // Purpose: Main loop for non-dedicated servers
  int RunListenServer() {
    // NOTE: Systems set up here should depend on the mod
    // Systems which are mod-independent should be set up in the launcher or
    // Init()

    // Innocent until proven guilty
    int nRunResult = RUN_OK;

    // Happens every time we start up and shut down a mod
    if (ModInit(startup_info_.m_pInitialMod, startup_info_.m_pInitialGame)) {
      CModAppSystemGroup modAppSystemGroup(
          false, startup_info_.m_pParentAppSystemGroup);

      // Store off the app system factory...
      g_AppSystemFactory = modAppSystemGroup.GetFactory();

      nRunResult = modAppSystemGroup.Run();

      g_AppSystemFactory = nullptr;

      // Shuts down the mod
      ModShutdown();

      // Disconnects from the editor window
      videomode->SetGameWindow(nullptr);
    }

    // Closes down things that were set up in OnStartup
    // TODO(d.rattman): OnStartup + OnShutdown should be removed + moved into
    // the launcher or the launcher code should be merged into the engine into
    // the code in OnStartup/OnShutdown
    OnShutdown();

    return nRunResult;
  }

  // Hooks a particular mod up to the registry
  void SetRegistryMod(const char *pModName);

  // One-time setup, based on the initially selected mod
  // TODO(d.rattman): This should move into the launcher!
  bool OnStartup(void *pInstance, const char *pStartupModName) {
    // This fixes a bug on certain machines where the input will
    // stop coming in for about 1 second when someone hits a key.
    // (true means to disable priority boost)
    SetThreadPriorityBoost(GetCurrentThread(), TRUE);

    // TODO(d.rattman): Turn videomode + game into IAppSystems?

    // Try to create the window
    Plat_TimestampedLog("Engine::CEngineAPI::OnStartup: game->Init");

    // This has to happen before CreateGameWindow to set up the instance
    // for use by the code that creates the window
    if (!game->Init(pInstance)) goto onStartupError;

    // Try to create the window
    Plat_TimestampedLog("Engine::CEngineAPI::OnStartup: videomode->Init");

    // This needs to be after Shader_Init and registry->Init
    // This way mods can have different default video settings
    if (!videomode->Init()) goto onStartupShutdownGame;

    // We need to access the registry to get various settings (specifically,
    // InitMaterialSystemConfig requires it).
    if (!InitRegistry(pStartupModName)) goto onStartupShutdownVideoMode;

    materials->ModInit();

    // Setup the material system config record, CreateGameWindow depends on it
    // (when we're running stand-alone)
    InitMaterialSystemConfig(InEditMode());

    ShutdownRegistry();
    return true;

    // Various error conditions
  onStartupShutdownVideoMode:
    videomode->Shutdown();

  onStartupShutdownGame:
    game->Shutdown();

  onStartupError:
    return false;
  }

  // One-time shutdown (shuts down stuff set up in OnStartup)
  // TODO(d.rattman): This should move into the launcher!
  void OnShutdown() {
    if (videomode) videomode->Shutdown();

    // Shut down the game
    game->Shutdown();

    materials->ModShutdown();
    TRACESHUTDOWN(COM_ShutdownFileSystem());
  }

  // Initialization, shutdown of a mod.
  bool ModInit(const char *pModName, const char *pGameDir) {
    // Set up the engineparms_t which contains global information about the mod
    host_parms.mod = COM_StringCopy(GetModDirFromPath(pModName));
    host_parms.game = COM_StringCopy(pGameDir);

    // By default, restrict server commands in Valve games and don't restrict
    // them in mods.
    cl.m_bRestrictServerCommands = IsValveMod(host_parms.mod);
    cl.m_bRestrictClientCommands = cl.m_bRestrictServerCommands;

    // build the registry path we're going to use for this mod
    InitRegistry(pModName);

    // This sets up the game search path, depends on host_parms
    TRACEINIT(MapReslistGenerator_Init(), MapReslistGenerator_Shutdown());
    TRACEINIT(DevShotGenerator_Init(), DevShotGenerator_Shutdown());

    // Slam cvars based on mod/config.cfg
    Host_ReadPreStartupConfiguration();

    // Create the game window now that we have a search path
    // TODO(d.rattman): Deal with initial window width + height better
    return (videomode && videomode->CreateGameWindow(
                             g_pMaterialSystemConfig->m_VideoMode.m_Width,
                             g_pMaterialSystemConfig->m_VideoMode.m_Height,
                             g_pMaterialSystemConfig->Windowed()));
  }

  void ModShutdown() {
    COM_StringFree(host_parms.mod);
    COM_StringFree(host_parms.game);

    // Stop accepting input from the window
    game->InputDetachFromGameWindow();

    TRACESHUTDOWN(DevShotGenerator_Shutdown());
    TRACESHUTDOWN(MapReslistGenerator_Shutdown());

    ShutdownRegistry();
  }

  // Initializes, shuts down the registry
  bool InitRegistry(const char *pModName) {
    char szRegSubPath[SOURCE_MAX_PATH];
    sprintf_s(szRegSubPath, "%s\\%s", "Source", pModName);
    return registry->Init(szRegSubPath);
  }

  void ShutdownRegistry() { registry->Shutdown(); }

  // Handles there being an error setting up the video mode
  // Purpose: Handles there being an error setting up the video mode
  // Output : Returns true on if the engine should restart, false if it should
  // quit
  InitReturnVal_t HandleSetModeError() {
    // show an error, see if the user wants to restart
    if (CommandLine()->FindParm("-safe")) {
      Sys_MessageBox(
          "Failed to set video mode.\n\nThis game has a minimum requirement of "
          "DirectX 9.0 compatible hardware.\n",
          "Video mode error", false);
      return INIT_FAILED;
    }

    if (CommandLine()->FindParm("-autoconfig")) {
      if (Sys_MessageBox(
              "Failed to set video mode - falling back to safe mode "
              "settings.\n\nGame will now restart with the new video "
              "settings.",
              "Video - safe mode fallback", true)) {
        CommandLine()->AppendParm("-safe", nullptr);
        return (InitReturnVal_t)INIT_RESTART;
      }
      return INIT_FAILED;
    }

    if (Sys_MessageBox("Failed to set video mode - resetting to "
                       "defaults.\n\nGame will now restart with the new video "
                       "settings.",
                       "Video mode warning", true)) {
      CommandLine()->AppendParm("-autoconfig", nullptr);
      return (InitReturnVal_t)INIT_RESTART;
    }

    return INIT_FAILED;
  }

  // Purpose: Message pump when running stand-alone
  void PumpMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Get input from attached devices
    g_pInputSystem->PollInputState();

    game->DispatchAllStoredGameMessages();

    EatTextModeKeyPresses();
  }

  // Purpose: Message pump when running with the editor
  void PumpMessagesEditMode(bool &bIdle, long &lIdleCount) {
    if (bIdle && !g_pHammer->HammerOnIdle(lIdleCount++)) {
      bIdle = false;
    }

    // Get input from attached devices
    g_pInputSystem->PollInputState();

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        eng->SetQuitting(IEngine::QUIT_TODESKTOP);
        break;
      }

      if (!g_pHammer->HammerPreTranslateMessage(&msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // Reset idle state after pumping idle message.
      if (g_pHammer->HammerIsIdleMessage(&msg)) {
        bIdle = true;
        lIdleCount = 0;
      }
    }

    game->DispatchAllStoredGameMessages();
  }

  // Activate/deactivates edit mode shaders
  void ActivateEditModeShaders(bool bActive) {
    if (InEditMode() && (g_pMaterialSystemConfig->bEditMode != bActive)) {
      MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
      config.bEditMode = bActive;
      OverrideMaterialSystemConfig(config);
    }
  }

 private:
  HWND editor_hwnd_;
  bool is_running_simulation_;
  StartupInfo_t startup_info_;
};

static CEngineAPI s_EngineAPI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineAPI, IEngineAPI,
                                  VENGINE_LAUNCHER_API_VERSION, s_EngineAPI);

int CModAppSystemGroup::Main() {
  int nRunResult = RUN_OK;

  if (IsServerOnly()) {
    // Start up the game engine
    if (eng->Load(true, host_parms.basedir)) {
      // Dedicated server drives frame loop manually
      dedicated->RunServer();

      SV_ShutdownGameDLL();
    }
  } else {
    eng->SetQuitting(IEngine::QUIT_NOTQUITTING);

    Plat_TimestampedLog("Engine::CEngineAPI::Main: eng->Load");

    // Start up the game engine
    if (eng->Load(false, host_parms.basedir)) {
#ifndef SWDS
      toolframework->ServerInit(g_ServerFactory);

      if (s_EngineAPI.MainLoop()) {
        nRunResult = RUN_RESTART;
      }

      // unload systems
      eng->Unload();

      toolframework->ServerShutdown();
#endif
      SV_ShutdownGameDLL();
    }
  }

  return nRunResult;
}

#endif  // SWDS

// Console command to toggle back and forth between the engine running or not
#ifndef SWDS
void EditorToggle_f() {
  // Will switch back to the editor
  bool bActive = (eng->GetState() != IEngine::DLL_PAUSED);
  s_EngineAPI.ActivateSimulation(!bActive);
}
#endif  // SWDS

// Purpose: Expose engine interface to launcher	for dedicated servers
class CDedicatedServerAPI : public CTier3AppSystem<IDedicatedServerAPI> {
  using BaseClass = CTier3AppSystem<IDedicatedServerAPI>;

 public:
  CDedicatedServerAPI() : dedicated_server_{nullptr} {}

  bool Connect(CreateInterfaceFn factory) override {
    // Store off the app system factory...
    g_AppSystemFactory = factory;

    if (!BaseClass::Connect(factory)) return false;

    dedicated = reinterpret_cast<IDedicatedExports *>(
        factory(VENGINE_DEDICATEDEXPORTS_API_VERSION, nullptr));
    if (!dedicated) return false;

    g_pFileSystem = g_pFullFileSystem;
    g_pFileSystem->SetWarningFunc(Warning);

    if (!Shader_Connect(false)) return false;

    if (!g_pStudioRender) {
      Sys_Error("Unable to init studio render system version %s\n",
                STUDIO_RENDER_INTERFACE_VERSION);
      return false;
    }

    g_pPhysics = reinterpret_cast<IPhysics *>(
        factory(VPHYSICS_INTERFACE_VERSION, nullptr));

    if (!g_pDataCache || !g_pPhysics || !g_pMDLCache) {
      Warning("Engine wasn't able to acquire required interfaces!\n");
      return false;
    }

    ConnectMDLCacheNotify();
    return true;
  }

  void Disconnect() override {
    DisconnectMDLCacheNotify();

    g_pPhysics = nullptr;

    Shader_Disconnect();

    g_pFileSystem = nullptr;

    ConVar_Unregister();

    dedicated = nullptr;

    BaseClass::Disconnect();

    g_AppSystemFactory = nullptr;
  }

  void *QueryInterface(const char *pInterfaceName) override {
    // Loading the engine DLL mounts *all* engine interfaces
    return Sys_GetFactoryThis()(pInterfaceName, nullptr);
  }

  bool ModInit(ModInfo_t &info) override {
    eng->SetQuitting(IEngine::QUIT_NOTQUITTING);

    // Set up the engineparms_t which contains global information about the mod
    host_parms.basedir = info.m_pBaseDirectory;
    host_parms.mod = GetModDirFromPath(info.m_pInitialMod);
    host_parms.game = info.m_pInitialGame;

    g_bTextMode = info.m_bTextMode;

    TRACEINIT(COM_InitFilesystem(info.m_pInitialMod), COM_ShutdownFileSystem());
    materials->ModInit();

    // Setup the material system config record, CreateGameWindow depends on it
    // (when we're running stand-alone)
#ifndef SWDS
    // !!should this be called standalone or not?
    InitMaterialSystemConfig(true);
#endif

    // Initialize general game stuff and create the main window
    if (game->Init(nullptr)) {
      dedicated_server_ =
          new CModAppSystemGroup(true, info.m_pParentAppSystemGroup);

      // Store off the app system factory...
      g_AppSystemFactory = dedicated_server_->GetFactory();

      dedicated_server_->Run();
      return true;
    }

    return false;
  }

  void ModShutdown() override {
    if (dedicated_server_) {
      delete dedicated_server_;
      dedicated_server_ = nullptr;
    }

    g_AppSystemFactory = nullptr;

    // Unload GL, Sound, etc.
    eng->Unload();

    // Shut down memory, etc.
    game->Shutdown();

    materials->ModShutdown();
    TRACESHUTDOWN(COM_ShutdownFileSystem());
  }

  bool RunFrame() override {
    // Bail if someone wants to quit.
    if (eng->GetQuitting() != IEngine::QUIT_NOTQUITTING) {
      return false;
    }

    // Run engine frame
    eng->Frame();
    return true;
  }

  void AddConsoleText(const ch *text) override { Cbuf_AddText(text); }

  std::tuple<f32, i32, i32> GetStatus(ch *map_name,
                                      usize map_name_size) const override {
    return Host_GetHostInfo(map_name, map_name_size);
  }

  void GetHostname(ch *host_name, usize host_name_size) const override {
    if (host_name && host_name_size > 0) {
      strcpy_s(host_name, host_name_size, sv.GetName());
    }
  }

 private:
  CModAppSystemGroup *dedicated_server_;
};

EXPOSE_SINGLE_INTERFACE(CDedicatedServerAPI, IDedicatedServerAPI,
                        VENGINE_HLDS_API_VERSION);

#ifndef SWDS
class CGameUIFuncs : public IGameUIFuncs {
 public:
  bool IsKeyDown(const ch *key_name, bool &is_down) const override {
    is_down = false;
    if (!g_ClientDLL) return false;

    return g_ClientDLL->IN_IsKeyDown(key_name, is_down);
  }

  const ch *GetBindingForButtonCode(ButtonCode_t code) const override {
    return ::Key_BindingForKey(code);
  }

  ButtonCode_t GetButtonCodeForBind(const ch *bind) const override {
    const ch *key_name = Key_NameForBinding(bind);
    if (!key_name) return KEY_NONE;

    return g_pInputSystem->StringToButtonCode(key_name);
  }

  std::tuple<vmode_t *, usize> GetVideoModes() const override {
    if (videomode) {
      return {videomode->GetMode(0), videomode->GetModeCount()};
    }

    return {nullptr, static_cast<usize>(0)};
  }

  void SetFriendsID(u32 friendsID, const ch *friendsName) override {
    cl.SetFriendsID(friendsID, friendsName);
  }

  std::tuple<i32, i32> GetDesktopResolution() const override {
    i32 width, height;
    std::tie(width, height, std::ignore) = game->GetDesktopInfo();

    return {width, height};
  }

  bool IsConnectedToVACSecureServer() const override {
    if (cl.IsConnected()) return Steam3Client().BGSSecure();
    return false;
  }
};

EXPOSE_SINGLE_INTERFACE(CGameUIFuncs, IGameUIFuncs,
                        VENGINE_GAMEUIFUNCS_VERSION);
#endif
