// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: An application framework.

#include "appframework/AppFramework.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier1/interface.h"

HINSTANCE s_HInstance;

// Default spec function.
SpewRetval_t LinuxAppDefaultSpewFunc(SpewType_t spew_type,
                                     char const *message) {
  fprintf(stderr, "%s", message);
  switch (spew_type) {
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

SpewRetval_t ConsoleAppDefaultSpewFunc(SpewType_t spew_type,
                                       char const *message) {
  printf("%s", message);
  switch (spew_type) {
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

SpewOutputFunc_t g_DefaultSpewFunc = LinuxAppDefaultSpewFunc;

// HACK: Since I don't want to refit vgui yet...
void *GetAppInstance() { return s_HInstance; }

// Sets the application instance, should only be used if you're not calling
// AppMain.
void SetAppInstance(void *hInstance) { s_HInstance = (HINSTANCE)hInstance; }

// Version of AppMain used by windows applications.
int AppMain(void *hInstance, void *hPrevInstance, const char *lpCmdLine,
            int nCmdShow, CAppSystemGroup *pAppSystemGroup) {
  Assert(0);
  return -1;
}

// Version of AppMain used by console applications.
int AppMain(int argc, char **argv, CAppSystemGroup *pAppSystemGroup) {
  Assert(pAppSystemGroup);

  g_DefaultSpewFunc = ConsoleAppDefaultSpewFunc;
  s_HInstance = nullptr;

  CommandLine()->CreateCmdLine(argc, argv);

  return pAppSystemGroup->Run();
}

// Default implementation of an application meant to be run using Steam.
CSteamApplication::CSteamApplication(CSteamAppSystemGroup *pAppSystemGroup) {
  m_pChildAppSystemGroup = pAppSystemGroup;
  m_pFileSystem = NULL;
}

// Create necessary interfaces.
bool CSteamApplication::Create() {
  FileSystem_SetErrorMode(FS_ERRORMODE_NONE);

  char pFileSystemDLL[SOURCE_MAX_PATH];
  if (FileSystem_GetFileSystemDLLName(pFileSystemDLL, std::size(pFileSystemDLL),
                                      m_bSteam) != FS_OK)
    return false;

  AppModule_t fileSystemModule = LoadModule(pFileSystemDLL);
  m_pFileSystem =
      (IFileSystem *)AddSystem(fileSystemModule, FILESYSTEM_INTERFACE_VERSION);
  if (!m_pFileSystem) {
    Error("Unable to load %s", pFileSystemDLL);
    return false;
  }

  return true;
}

// The file system pointer is invalid at this point.
void CSteamApplication::Destroy() { m_pFileSystem = NULL; }

// Pre-init, shutdown.
bool CSteamApplication::PreInit() { return true; }

void CSteamApplication::PostShutdown() {}

// Run steam main loop.
int CSteamApplication::Main() {
  // Now that Steam is loaded, we can load up main libraries through steam.
  m_pChildAppSystemGroup->Setup(m_pFileSystem, this);
  return m_pChildAppSystemGroup->Run();
}

int CSteamApplication::Startup() {
  int nRetVal = BaseClass::Startup();
  if (GetErrorStage() != NONE) return nRetVal;

  if (FileSystem_SetBasePaths(m_pFileSystem) != FS_OK) return 0;

  // Now that Steam is loaded, we can load up main libraries through steam.
  m_pChildAppSystemGroup->Setup(m_pFileSystem, this);
  return m_pChildAppSystemGroup->Startup();
}

void CSteamApplication::Shutdown() {
  m_pChildAppSystemGroup->Shutdown();
  BaseClass::Shutdown();
}
