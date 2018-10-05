// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: An application framework

#include "build/include/build_config.h"

#include "appframework/appframework.h"
#include "appframework/iappsystemgroup.h"
#include "base/include/windows/windows_light.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier1/interface.h"
#include "vstdlib/cvar.h"

#include "tier0/include/memdbgon.h"

HINSTANCE s_HInstance;

DbgReturn WinAppDefaultSpewFunc(DbgLevel spewType, ch const *pMsg) {
  Plat_DebugString(pMsg);
  switch (spewType) {
    case kDbgLevelMessage:
    case kDbgLevelWarning:
    case kDbgLevelLog:
      return kDbgContinue;

    case kDbgLevelAssert:
    case kDbgLevelError:
    default:
      return kDbgBreak;
  }
}

DbgReturn ConsoleAppDefaultSpewFunc(DbgLevel spewType, ch const *pMsg) {
  printf("%s", pMsg);
  Plat_DebugString(pMsg);

  switch (spewType) {
    case kDbgLevelMessage:
    case kDbgLevelWarning:
    case kDbgLevelLog:
      return kDbgContinue;

    case kDbgLevelAssert:
    case kDbgLevelError:
    default:
      return kDbgBreak;
  }
}

DbgOutputFn g_DefaultSpewFunc = WinAppDefaultSpewFunc;

// HACK: Since I don't want to refit vgui yet...
HINSTANCE GetAppInstance() { return s_HInstance; }

// Sets the application instance, should only be used if you're not calling
// AppMain.
void SetAppInstance(HINSTANCE hInstance) { s_HInstance = hInstance; }

// Version of AppMain used by windows applications
i32 AppMain(void *hInstance, void *, const ch *, i32,
            CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = WinAppDefaultSpewFunc;
  s_HInstance = (HINSTANCE)hInstance;
  CommandLine()->CreateCmdLine(::GetCommandLine());

  return pAppSystemGroup->Run();
}

// Version of AppMain used by console applications
i32 AppMain(i32 argc, ch **argv, CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = ConsoleAppDefaultSpewFunc;
  s_HInstance = nullptr;

  CommandLine()->CreateCmdLine(argc, argv);

  return pAppSystemGroup->Run();
}

// Used to startup/shutdown the application
i32 AppStartup(void *hInstance, void *, const ch *, i32,
               CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = WinAppDefaultSpewFunc;
  s_HInstance = (HINSTANCE)hInstance;
  CommandLine()->CreateCmdLine(::GetCommandLine());

  return pAppSystemGroup->Startup();
}

i32 AppStartup(i32 argc, ch **argv, CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = ConsoleAppDefaultSpewFunc;
  s_HInstance = nullptr;
  CommandLine()->CreateCmdLine(argc, argv);

  return pAppSystemGroup->Startup();
}

void AppShutdown(CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);
  pAppSystemGroup->Shutdown();
}

// Default implementation of an application meant to be run using Steam

CSteamApplication::CSteamApplication(CSteamAppSystemGroup *pAppSystemGroup) {
  m_pChildAppSystemGroup = pAppSystemGroup;
  m_pFileSystem = nullptr;
  m_bSteam = false;
}

// Create necessary interfaces
bool CSteamApplication::Create() {
  FileSystem_SetErrorMode(FS_ERRORMODE_AUTO);

  ch pFileSystemDLL[SOURCE_MAX_PATH];
  if (FileSystem_GetFileSystemDLLName(pFileSystemDLL, std::size(pFileSystemDLL), m_bSteam) !=
      FS_OK)
    return false;

  FileSystem_SetupSteamInstallPath();

  // Add in the cvar factory
  AppModule_t cvarModule = LoadModule(VStdLib_GetICVarFactory());
  AddSystem(cvarModule, CVAR_INTERFACE_VERSION);

  AppModule_t fileSystemModule = LoadModule(pFileSystemDLL);
  m_pFileSystem =
      (IFileSystem *)AddSystem(fileSystemModule, FILESYSTEM_INTERFACE_VERSION);
  if (!m_pFileSystem) {
    Error("Unable to load %s", pFileSystemDLL);
    return false;
  }

  return true;
}

// The file system pointer is invalid at this point
void CSteamApplication::Destroy() { m_pFileSystem = nullptr; }

// Pre-init, shutdown
bool CSteamApplication::PreInit() { return true; }

void CSteamApplication::PostShutdown() {}

// Run steam main loop
i32 CSteamApplication::Main() {
  // Now that Steam is loaded, we can load up main libraries through steam
  if (FileSystem_SetBasePaths(m_pFileSystem) != FS_OK) return 0;

  m_pChildAppSystemGroup->Setup(m_pFileSystem, this);
  return m_pChildAppSystemGroup->Run();
}

// Use this version in cases where you can't control the main loop and
// expect to be ticked
i32 CSteamApplication::Startup() {
  i32 nRetVal = BaseClass::Startup();

  if (GetErrorStage() != NONE) return nRetVal;
  if (FileSystem_SetBasePaths(m_pFileSystem) != FS_OK) return 0;

  // Now that Steam is loaded, we can load up main libraries through steam
  m_pChildAppSystemGroup->Setup(m_pFileSystem, this);
  return m_pChildAppSystemGroup->Startup();
}

void CSteamApplication::Shutdown() {
  m_pChildAppSystemGroup->Shutdown();
  BaseClass::Shutdown();
}
