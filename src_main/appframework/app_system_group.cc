// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order

#include "appframework/iappsystemgroup.h"

#include "appframework/iappsystem.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier1/interface.h"

#include "tier0/include/memdbgon.h"

extern SpewOutputFunc_t g_DefaultSpewFunc;

CAppSystemGroup::CAppSystemGroup(CAppSystemGroup *parent_app_system_group)
    : m_SystemDict{false, 0, 16},
      m_pParentAppSystem{parent_app_system_group},
      m_nErrorStage{CREATION} {}

CSysModule *CAppSystemGroup::LoadModuleDLL(const ch *module_name) {
  return Sys_LoadModule(module_name);
}

AppModule_t CAppSystemGroup::LoadModule(const ch *module_name) {
  // Remove the extension when creating the name.
  const usize module_name_length = strlen(module_name) + 1;
  ch *pModuleName = stack_alloc<ch>(module_name_length);
  Q_StripExtension(module_name, pModuleName, module_name_length);

  // See if we already loaded it...
  for (i32 i = m_Modules.Count(); --i >= 0;) {
    if (m_Modules[i].m_pModuleName) {
      if (!_stricmp(pModuleName, m_Modules[i].m_pModuleName)) {
        stack_free(pModuleName);
        return i;
      }
    }
  }

  CSysModule *pSysModule = LoadModuleDLL(module_name);
  if (!pSysModule) {
    Warning("AppFramework: Unable to load module %s!\n", module_name);
    return APP_MODULE_INVALID;
  }

  i32 nIndex = m_Modules.AddToTail();
  m_Modules[nIndex].m_pModule = pSysModule;
  m_Modules[nIndex].m_Factory = nullptr;
  m_Modules[nIndex].m_pModuleName = (ch *)malloc(module_name_length);
  strcpy_s(m_Modules[nIndex].m_pModuleName, module_name_length, pModuleName);

  stack_free(pModuleName);

  return nIndex;
}

AppModule_t CAppSystemGroup::LoadModule(CreateInterfaceFn factory) {
  if (!factory) {
    Warning("AppFramework: Unable to load module %s!\n", factory);
    return APP_MODULE_INVALID;
  }

  // See if we already loaded it...
  for (i32 i = m_Modules.Count(); --i >= 0;) {
    if (m_Modules[i].m_Factory) {
      if (m_Modules[i].m_Factory == factory) return i;
    }
  }

  i32 nIndex = m_Modules.AddToTail();
  m_Modules[nIndex].m_pModule = nullptr;
  m_Modules[nIndex].m_Factory = factory;
  m_Modules[nIndex].m_pModuleName = nullptr;

  return nIndex;
}

void CAppSystemGroup::UnloadAllModules() {
  // NOTE: Iterate in reverse order so they are unloaded in opposite order from
  // loading
  for (i32 i = m_Modules.Count(); --i >= 0;) {
    if (m_Modules[i].m_pModule) {
      Sys_UnloadModule(m_Modules[i].m_pModule);
    }
    if (m_Modules[i].m_pModuleName) {
      heap_free(m_Modules[i].m_pModuleName);
    }
  }
  m_Modules.RemoveAll();
}

// Methods to add/remove various global singleton systems
IAppSystem *CAppSystemGroup::AddSystem(AppModule_t module,
                                       const ch *pInterfaceName) {
  if (module == APP_MODULE_INVALID) return nullptr;

  Assert((module >= 0) && (module < m_Modules.Count()));
  CreateInterfaceFn pFactory = m_Modules[module].m_pModule
                                   ? Sys_GetFactory(m_Modules[module].m_pModule)
                                   : m_Modules[module].m_Factory;

  if (!pFactory) {
    Warning("AppFramework: No factory in module %s. Need system %s!\n",
            m_Modules[module].m_pModuleName, pInterfaceName);
    return nullptr;
  }

  i32 retval;
  void *pSystem = pFactory(pInterfaceName, &retval);
  if ((retval != IFACE_OK) || (!pSystem)) {
    Warning("AppFramework: Unable to create system %s!\n", pInterfaceName);
    return nullptr;
  }

  IAppSystem *pAppSystem = static_cast<IAppSystem *>(pSystem);
  i32 sysIndex = m_Systems.AddToTail(pAppSystem);

  // Inserting into the dict will help us do named lookup later
  MEM_ALLOC_CREDIT();
  m_SystemDict.Insert(pInterfaceName, sysIndex);
  return pAppSystem;
}

static ch const *g_StageLookup[] = {
    "CREATION",       "CONNECTION",  "PREINITIALIZATION",
    "INITIALIZATION", "SHUTDOWN",    "POSTSHUTDOWN",
    "DISCONNECTION",  "DESTRUCTION", "NONE",
};

void CAppSystemGroup::ReportStartupFailure(i32 nErrorStage, i32 nSysIndex) {
  ch const *pszStageDesc = "Unknown";
  if (nErrorStage >= 0 && (size_t)nErrorStage < std::size(g_StageLookup)) {
    pszStageDesc = g_StageLookup[nErrorStage];
  }

  ch const *pszSystemName = "(Unknown)";
  for (i32 i = m_SystemDict.First(); i != m_SystemDict.InvalidIndex();
       i = m_SystemDict.Next(i)) {
    if (m_SystemDict[i] != nSysIndex) continue;

    pszSystemName = m_SystemDict.GetElementName(i);
    break;
  }

  // Walk the dictionary
  Warning("AppFramework: System (%s) failed during stage %s\n", pszSystemName,
          pszStageDesc);
}

void CAppSystemGroup::AddSystem(IAppSystem *pAppSystem,
                                const ch *pInterfaceName) {
  if (!pAppSystem) return;

  i32 sysIndex = m_Systems.AddToTail(pAppSystem);

  // Inserting into the dict will help us do named lookup later
  MEM_ALLOC_CREDIT();
  m_SystemDict.Insert(pInterfaceName, sysIndex);
}

void CAppSystemGroup::RemoveAllSystems() {
  // NOTE: There's no deallcation here since we don't really know
  // how the allocation has happened. We could add a deallocation method
  // to the code in interface.h; although when the modules are unloaded
  // the deallocation will happen anyways
  m_Systems.RemoveAll();
  m_SystemDict.RemoveAll();
}

// Simpler method of doing the LoadModule/AddSystem thing.
bool CAppSystemGroup::AddSystems(AppSystemInfo_t *systems) {
  while (systems->m_pModuleName[0]) {
    AppModule_t module = LoadModule(systems->m_pModuleName);
    if (module == APP_MODULE_INVALID) {
      Warning("AppFramework: Unable to load %s\n", systems->m_pModuleName);
      return false;
    }

    IAppSystem *system = AddSystem(module, systems->m_pInterfaceName);

    if (!system) {
      Warning("AppFramework: Unable to load interface %s from %s\n",
              systems->m_pInterfaceName, systems->m_pModuleName);
      return false;
    }
    ++systems;
  }

  return true;
}

// Methods to find various global singleton systems
void *CAppSystemGroup::FindSystem(const ch *pSystemName) {
  u16 i = m_SystemDict.Find(pSystemName);
  if (i != m_SystemDict.InvalidIndex()) return m_Systems[m_SystemDict[i]];

  // If it's not an interface we know about, it could be an older
  // version of an interface, or maybe something implemented by
  // one of the instantiated interfaces...

  // QUESTION: What order should we iterate this in?
  // It controls who wins if multiple ones implement the same interface
  for (i = 0; i < m_Systems.Count(); ++i) {
    void *pInterface = m_Systems[i]->QueryInterface(pSystemName);
    if (pInterface) return pInterface;
  }

  if (m_pParentAppSystem) {
    void *pInterface = m_pParentAppSystem->FindSystem(pSystemName);
    if (pInterface) return pInterface;
  }

  // No dice..
  return nullptr;
}

// Gets at the parent appsystem group
CAppSystemGroup *CAppSystemGroup::GetParent() { return m_pParentAppSystem; }

// Method to connect/disconnect all systems
bool CAppSystemGroup::ConnectSystems() {
  for (i32 i = 0; i < m_Systems.Count(); ++i) {
    IAppSystem *sys = m_Systems[i];

    if (!sys->Connect(GetFactory())) {
      ReportStartupFailure(CONNECTION, i);
      return false;
    }
  }
  return true;
}

void CAppSystemGroup::DisconnectSystems() {
  // Disconnect in reverse order of connection
  for (i32 i = m_Systems.Count(); --i >= 0;) {
    m_Systems[i]->Disconnect();
  }
}

// Method to initialize/shutdown all systems
InitReturnVal_t CAppSystemGroup::InitSystems() {
  for (i32 i = 0; i < m_Systems.Count(); ++i) {
    InitReturnVal_t nRetVal = m_Systems[i]->Init();
    if (nRetVal != INIT_OK) {
      ReportStartupFailure(INITIALIZATION, i);
      return nRetVal;
    }
  }
  return INIT_OK;
}

void CAppSystemGroup::ShutdownSystems() {
  // Shutdown in reverse order of initialization
  for (i32 i = m_Systems.Count(); --i >= 0;) {
    m_Systems[i]->Shutdown();
  }
}

// Returns the stage at which the app system group ran into an error
CAppSystemGroup::AppSystemGroupStage_t CAppSystemGroup::GetErrorStage() const {
  return m_nErrorStage;
}

// Gets at a factory that works just like FindSystem
// This function is used to make this system appear to the outside world to
// function exactly like the currently existing factory system
CAppSystemGroup *s_pCurrentAppSystem;
void *AppSystemCreateInterfaceFn(const ch *pName, i32 *pReturnCode) {
  void *pInterface = s_pCurrentAppSystem->FindSystem(pName);
  if (pReturnCode) {
    *pReturnCode = pInterface ? IFACE_OK : IFACE_FAILED;
  }
  return pInterface;
}

// Gets at a class factory for the topmost appsystem group in an appsystem stack
CreateInterfaceFn CAppSystemGroup::GetFactory() {
  return AppSystemCreateInterfaceFn;
}

// Main application loop
i32 CAppSystemGroup::Run() {
  // The factory now uses this app system group
  s_pCurrentAppSystem = this;

  // Load, connect, init
  i32 nRetVal = OnStartup();
  if (m_nErrorStage != NONE) return nRetVal;

  // Main loop implemented by the application
  // TODO(d.rattman): HACK workaround to avoid vgui porting
  nRetVal = Main();

  // Shutdown, disconnect, unload
  OnShutdown();

  // The factory now uses the parent's app system group
  s_pCurrentAppSystem = GetParent();

  return nRetVal;
}

// Virtual methods for override
i32 CAppSystemGroup::Startup() { return OnStartup(); }

void CAppSystemGroup::Shutdown() { return OnShutdown(); }

// Use this version in cases where you can't control the main loop and
// expect to be ticked
i32 CAppSystemGroup::OnStartup() {
  // The factory now uses this app system group
  s_pCurrentAppSystem = this;

  m_nErrorStage = NONE;

  // Call an installed application creation function
  if (!Create()) {
    m_nErrorStage = CREATION;
    return -1;
  }

  // Let all systems know about each other
  if (!ConnectSystems()) {
    m_nErrorStage = CONNECTION;
    return -1;
  }

  // Allow the application to do some work before init
  if (!PreInit()) {
    m_nErrorStage = PREINITIALIZATION;
    return -1;
  }

  // Call Init on all App Systems
  i32 nRetVal = InitSystems();
  if (nRetVal != INIT_OK) {
    m_nErrorStage = INITIALIZATION;
    return -1;
  }

  return nRetVal;
}

void CAppSystemGroup::OnShutdown() {
  // The factory now uses this app system group
  s_pCurrentAppSystem = this;

  switch (m_nErrorStage) {
    case SHUTDOWN:
    case POSTSHUTDOWN:
    case DISCONNECTION:
    case DESTRUCTION:
    case NONE:
      break;

    case PREINITIALIZATION:
    case INITIALIZATION:
      goto disconnect;

    case CREATION:
    case CONNECTION:
      goto destroy;
  }

  // Cal Shutdown on all App Systems
  ShutdownSystems();

  // Allow the application to do some work after shutdown
  PostShutdown();

disconnect:
  // Systems should disconnect from each other
  DisconnectSystems();

destroy:
  // Unload all DLLs loaded in the AppCreate block
  RemoveAllSystems();

  // By default, direct dbg reporting...
  // Have to do this because the spew func may point to a module which is being
  // unloaded
  SpewOutputFunc(g_DefaultSpewFunc);

  UnloadAllModules();

  // Call an installed application destroy function
  Destroy();
}

// This class represents a group of app systems that are loaded through steam
CSteamAppSystemGroup::CSteamAppSystemGroup(IFileSystem *pFileSystem,
                                           CAppSystemGroup *pAppSystemParent) {
  m_pFileSystem = pFileSystem;
  m_pGameInfoPath[0] = 0;
}

// Used by CSteamApplication to set up necessary pointers if we can't do it in
// the constructor
void CSteamAppSystemGroup::Setup(IFileSystem *pFileSystem,
                                 CAppSystemGroup *pParentAppSystem) {
  m_pFileSystem = pFileSystem;
  m_pParentAppSystem = pParentAppSystem;
}

// Loads the module from Steam
CSysModule *CSteamAppSystemGroup::LoadModuleDLL(const ch *pDLLName) {
  return m_pFileSystem->LoadModule(pDLLName);
}

// Returns the game info path
const ch *CSteamAppSystemGroup::GetGameInfoPath() const {
  return m_pGameInfoPath;
}

// Sets up the search paths
bool CSteamAppSystemGroup::SetupSearchPaths(const ch *pStartingDir,
                                            bool bOnlyUseStartingDir,
                                            bool bIsTool) {
  CFSSteamSetupInfo steamInfo;
  steamInfo.m_pDirectoryName = pStartingDir;
  steamInfo.m_bOnlyUseDirectoryName = bOnlyUseStartingDir;
  steamInfo.m_bToolsMode = bIsTool;
  steamInfo.m_bSetSteamDLLPath = true;
  steamInfo.m_bSteam = m_pFileSystem->IsSteam();
  if (FileSystem_SetupSteamEnvironment(steamInfo) != FS_OK) return false;

  CFSMountContentInfo fsInfo;
  fsInfo.m_pFileSystem = m_pFileSystem;
  fsInfo.m_bToolsMode = bIsTool;
  fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

  if (FileSystem_MountContent(fsInfo) != FS_OK) return false;

  // Finally, load the search paths for the "GAME" path.
  CFSSearchPathsInit searchPathsInit;
  searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
  searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
  if (FileSystem_LoadSearchPaths(searchPathsInit) != FS_OK) return false;

  FileSystem_AddSearchPath_Platform(fsInfo.m_pFileSystem,
                                    steamInfo.m_GameInfoPath);
  strcpy_s(m_pGameInfoPath, steamInfo.m_GameInfoPath);
  return true;
}
