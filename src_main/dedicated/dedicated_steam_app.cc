// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_steam_app.h"

#include <direct.h>

#include "dedicated_common.h"
#include "engine_hlds_api.h"
#include "filesystem_init.h"
#include "idedicated_os.h"
#include "idedicatedexports.h"
#include "tier0/include/icommandline.h"
#include "tier1/KeyValues.h"
#include "vgui/vguihelpers.h"

#if defined(OS_WIN)
#include "console/textconsolewin32.h"
CTextConsoleWin32 console;
#else
#include "console/textconsoleunix.h"
CTextConsoleUnix console;
#endif

DbgReturn DedicatedSpewOutputFunc(DbgLevel spewType, char const *pMsg) {
  DedicatedOs()->Printf("%s", pMsg);

#ifdef _WIN32
  Plat_DebugString(pMsg);
#endif

  if (spewType == kDbgLevelError) {
  // In Windows vgui mode, make a message box or they won't ever see the
  // error.
#ifdef _WIN32
    if (g_bVGui) {
      MessageBox(nullptr, pMsg, "Awesome Dedicated Server - Error",
                 MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }
    TerminateProcess(GetCurrentProcess(), 1);
#elif OS_POSIX
    fflush(stdout);
    _exit(1);
#else
#error "Implement me"
#endif

    return kDbgAbort;
  }

  if (spewType == kDbgLevelAssert) {
    if (CommandLine()->FindParm("-noassert") == 0) return kDbgBreak;

    return kDbgContinue;
  }

  return kDbgContinue;
}

// Instantiate all main libraries.
bool DedicatedSteamApp::Create() {
  // Hook the debug output stuff (override the spew func in the appframework).
  SetDbgOutputCallback(DedicatedSpewOutputFunc);

  // Added the dedicated exports module for the engine to grab
  AppModule_t dedicatedModule = LoadModule(Sys_GetFactoryThis());
  IAppSystem *pSystem =
      AddSystem(dedicatedModule, VENGINE_DEDICATEDEXPORTS_API_VERSION);
  if (!pSystem) return false;

  return DedicatedOs()->LoadModules(this);
}

bool DedicatedSteamApp::PreInit() {
  // A little hack needed because dedicated links directly to filesystem .cpp
  // files
  g_pFullFileSystem = nullptr;

  if (!BaseClass::PreInit()) return false;

  CFSSteamSetupInfo steamInfo;
  steamInfo.m_pDirectoryName = nullptr;
  steamInfo.m_bOnlyUseDirectoryName = false;
  steamInfo.m_bToolsMode = false;
  steamInfo.m_bSetSteamDLLPath = false;
  steamInfo.m_bSteam = g_pFullFileSystem->IsSteam();
  steamInfo.m_bNoGameInfo = steamInfo.m_bSteam;
  if (FileSystem_SetupSteamEnvironment(steamInfo) != FS_OK) return false;

  CFSMountContentInfo fsInfo;
  fsInfo.m_pFileSystem = g_pFullFileSystem;
  fsInfo.m_bToolsMode = false;
  fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

  if (FileSystem_MountContent(fsInfo) != FS_OK) return false;

  const i32 error_code = scoped_winsock_initializer_.error_code();

  if (error_code != ERROR_SUCCESS) {
    DedicatedOs()->Printf("Winsock 2.2 unavailable (0x%.8x)...", error_code);
    return false;
  }

#ifdef OS_WIN
  if (CommandLine()->CheckParm("-console")) {
    g_bVGui = false;
  } else {
    g_bVGui = true;
  }
#else
  // no VGUI under linux
  g_bVGui = false;
#endif

  if (!g_bVGui && !DedicatedOs()->CreateConsoleWindow()) return false;

  return true;
}

// initialize the console or wait for vgui to start the server
bool ConsoleStartup(CreateInterfaceFn dedicatedFactory) {
#ifdef OS_WIN
  if (g_bVGui) {
    StartVGUI(dedicatedFactory);
    RunVGUIFrame();
    // Run the config screen
    while (VGUIIsInConfig() && VGUIIsRunning()) {
      RunVGUIFrame();
    }

    if (VGUIIsStopping()) {
      return false;
    }
  } else
#endif  // OS_WIN
  {
    if (!console.Init()) {
      return false;
    }
  }
  return true;
}

// filesystem_steam.cpp implements this useful function - mount all the caches
// for a given app ID.
extern void MountDependencies(int iAppId, CUtlVector<unsigned int> &depList);

int DedicatedSteamApp::Main() {
  if (!ConsoleStartup(GetFactory())) return -1;

#ifdef OS_WIN
  if (g_bVGui) {
    RunVGUIFrame();
  } else {
    // mount the caches
    if (CommandLine()->CheckParm("-steam")) {
      // Add a search path for the base dir
      char fullLocationPath[SOURCE_MAX_PATH];
      if (_getcwd(fullLocationPath, SOURCE_MAX_PATH)) {
        g_pFullFileSystem->AddSearchPath(fullLocationPath, "MAIN");
      }

      // Find the gameinfo.txt for our mod and mount it's caches
      char gameInfoFilename[SOURCE_MAX_PATH];
      sprintf_s(gameInfoFilename, "%s\\gameinfo.txt",
                CommandLine()->ParmValue(
                    source::tier0::command_line_switches::kGamePath, "hl2"));
      KeyValues *gameData = new KeyValues("GameInfo");
      if (gameData->LoadFromFile(g_pFullFileSystem, gameInfoFilename)) {
        KeyValues *pFileSystem = gameData->FindKey("FileSystem");
        int iAppId = pFileSystem->GetInt("SteamAppId");
        if (iAppId) {
          CUtlVector<unsigned int> depList;
          MountDependencies(iAppId, depList);
        }
      }
      gameData->deleteThis();

      // remove our base search path
      g_pFullFileSystem->RemoveSearchPaths("MAIN");
    }
  }
#endif

  // Set up mod information
  ModInfo_t info;
  info.m_pInstance = GetAppInstance();
  info.m_pBaseDirectory = GetBaseDirectory();
  info.m_pInitialMod = CommandLine()->ParmValue(
      source::tier0::command_line_switches::kGamePath, "hl2");
  info.m_pInitialGame = CommandLine()->ParmValue(
      source::tier0::command_line_switches::kDefaultGamePath, "hl2");
  info.m_pParentAppSystemGroup = this;
  info.m_bTextMode =
      CommandLine()->CheckParm(source::tier0::command_line_switches::textMode);

  if (engine->ModInit(info)) engine->ModShutdown();

  return 0;
}

void DedicatedSteamApp::PostShutdown() {
#ifdef OS_WIN
  if (g_bVGui) StopVGUI();
#endif

  DedicatedOs()->DestroyConsoleWindow();
  console.ShutDown();
  BaseClass::PostShutdown();
}

void DedicatedSteamApp::Destroy() {}
