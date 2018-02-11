// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: An application framework

#ifdef _LINUX
#include "linuxapp.cpp"
#else
#if defined(_WIN32)
#include "winlite.h"
#endif
#include "appframework/appframework.h"
#include "appframework/iappsystemgroup.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier1/interface.h"
#include "vstdlib/cvar.h"
#include "xbox/xbox_console.h"

// memdbgon must be the last include file in a .cc file!!!
#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

//-----------------------------------------------------------------------------
// default spec function
//-----------------------------------------------------------------------------
SpewRetval_t WinAppDefaultSpewFunc(SpewType_t spewType, char const *pMsg) {
  Plat_DebugString(pMsg);
  switch (spewType) {
    case SPEW_MESSAGE:
    case SPEW_WARNING:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
    case SPEW_ERROR:
    default:
      return SPEW_DEBUGGER;
  }
}

SpewRetval_t ConsoleAppDefaultSpewFunc(SpewType_t spewType, char const *pMsg) {
  printf("%s", pMsg);
  Plat_DebugString(pMsg);

  switch (spewType) {
    case SPEW_MESSAGE:
    case SPEW_WARNING:
    case SPEW_LOG:
      return SPEW_CONTINUE;

    case SPEW_ASSERT:
    case SPEW_ERROR:
    default:
      return SPEW_DEBUGGER;
  }
}

SpewOutputFunc_t g_DefaultSpewFunc = WinAppDefaultSpewFunc;

//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance() { return s_HInstance; }

//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling
// AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance(void *hInstance) { s_HInstance = (HINSTANCE)hInstance; }

//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------
int AppMain(void *hInstance, void *, const char *, int,
            CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = WinAppDefaultSpewFunc;
  s_HInstance = (HINSTANCE)hInstance;
  CommandLine()->CreateCmdLine(::GetCommandLine());

  return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain(int argc, char **argv, CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = ConsoleAppDefaultSpewFunc;
  s_HInstance = NULL;

  CommandLine()->CreateCmdLine(argc, argv);

  return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup(void *hInstance, void *, const char *, int,
               CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = WinAppDefaultSpewFunc;
  s_HInstance = (HINSTANCE)hInstance;
  CommandLine()->CreateCmdLine(::GetCommandLine());

  return pAppSystemGroup->Startup();
}

int AppStartup(int argc, char **argv, CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = ConsoleAppDefaultSpewFunc;
  s_HInstance = NULL;
  CommandLine()->CreateCmdLine(argc, argv);

  return pAppSystemGroup->Startup();
}

void AppShutdown(CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);
  pAppSystemGroup->Shutdown();
}

//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using Steam
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamApplication::CSteamApplication(CSteamAppSystemGroup *pAppSystemGroup) {
  m_pChildAppSystemGroup = pAppSystemGroup;
  m_pFileSystem = NULL;
  m_bSteam = false;
}

//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create() {
  FileSystem_SetErrorMode(FS_ERRORMODE_AUTO);

  char pFileSystemDLL[MAX_PATH];
  if (FileSystem_GetFileSystemDLLName(pFileSystemDLL, MAX_PATH, m_bSteam) !=
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

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy() { m_pFileSystem = NULL; }

//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit() { return true; }

void CSteamApplication::PostShutdown() {}

//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main() {
  // Now that Steam is loaded, we can load up main libraries through steam
  if (FileSystem_SetBasePaths(m_pFileSystem) != FS_OK) return 0;

  m_pChildAppSystemGroup->Setup(m_pFileSystem, this);
  return m_pChildAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CSteamApplication::Startup() {
  int nRetVal = BaseClass::Startup();
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

#endif
