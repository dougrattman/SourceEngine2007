// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Launcher for hammer, which is sitting in its own DLL

#include "base/include/windows/windows_light.h"

#include "SteamWriteMinidump.h"
#include "appframework/AppFramework.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "ihammer.h"
#include "inputsystem/iinputsystem.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "vgui/isurface.h"
#include "vgui/ivgui.h"
#include "vphysics_interface.h"
#include "vstdlib/cvar.h"

void __cdecl WriteMiniDumpUsingExceptionInfo(
    unsigned int uStructuredExceptionCode,
    EXCEPTION_POINTERS *pExceptionInfo) {
#ifndef NO_STEAM
  // TODO: dynamically set the minidump comment from contextual info about the
  // crash (i.e current VPROF node)?
  SteamWriteMiniDumpUsingExceptionInfoWithBuildId(uStructuredExceptionCode,
                                                  pExceptionInfo, 0);
#endif
}

// The application object
class CHammerApp : public CAppSystemGroup {
 public:
  // Methods of IApplication
  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

 private:
  int MainLoop();

  IHammer *hammer_;
  IMaterialSystem *material_system_;
  IFileSystem *file_system_;
  IDataCache *data_cache_;
  IInputSystem *input_system_;
};

// Define the application object
CHammerApp g_HammerApp;
DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(g_HammerApp);

// Create all singleton systems
bool CHammerApp::Create() {
  // Save some memory so engine/hammer isn't so painful
  CommandLine()->AppendParm("-disallowhwmorph", nullptr);

  // Add in the cvar factory
  const AppModule_t cvar_module = LoadModule(VStdLib_GetICVarFactory());
  ICvar *cvar_system = AddSystem<ICvar>(cvar_module, CVAR_INTERFACE_VERSION);
  if (!cvar_system) return false;

  bool is_steam;
  char filesystem_dll_path[SOURCE_MAX_PATH];
  if (FileSystem_GetFileSystemDLLName(filesystem_dll_path,
                                      SOURCE_ARRAYSIZE(filesystem_dll_path),
                                      is_steam) != FS_OK)
    return false;

  FileSystem_SetupSteamInstallPath();

  const AppModule_t filessytem_module = LoadModule(filesystem_dll_path);
  file_system_ =
      AddSystem<IFileSystem>(filessytem_module, FILESYSTEM_INTERFACE_VERSION);

  AppSystemInfo_t app_systems[] = {
      {"materialsystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION},
      {"inputsystem.dll", INPUTSYSTEM_INTERFACE_VERSION},
      {"studiorender.dll", STUDIO_RENDER_INTERFACE_VERSION},
      {"vphysics.dll", VPHYSICS_INTERFACE_VERSION},
      {"datacache.dll", DATACACHE_INTERFACE_VERSION},
      {"datacache.dll", MDLCACHE_INTERFACE_VERSION},
      {"datacache.dll", STUDIO_DATA_CACHE_INTERFACE_VERSION},
      {"vguimatsurface.dll", VGUI_SURFACE_INTERFACE_VERSION},
      {"vgui2.dll", VGUI_IVGUI_INTERFACE_VERSION},
      {"hammer_dll.dll", INTERFACEVERSION_HAMMER},
      {"", ""}  // Required to terminate the list
  };

  if (!AddSystems(app_systems)) return false;

  // Connect to interfaces loaded in AddSystems that we need locally
  material_system_ =
      FindSystem<IMaterialSystem>(MATERIAL_SYSTEM_INTERFACE_VERSION);
  hammer_ = FindSystem<IHammer>(INTERFACEVERSION_HAMMER);
  data_cache_ = FindSystem<IDataCache>(DATACACHE_INTERFACE_VERSION);
  input_system_ = FindSystem<IInputSystem>(INPUTSYSTEM_INTERFACE_VERSION);

  // This has to be done before connection.
  material_system_->SetShaderAPI("shaderapidx9.dll");

  return true;
}

void CHammerApp::Destroy() {
  file_system_ = nullptr;
  material_system_ = nullptr;
  data_cache_ = nullptr;
  hammer_ = nullptr;
  input_system_ = nullptr;
}

SpewRetval_t HammerSpewFunc(SpewType_t type, char const *pMsg) {
  if (type == SPEW_ASSERT) {
    return SPEW_DEBUGGER;
  }

  if (type == SPEW_ERROR || type == SPEW_WARNING) {
    MessageBox(nullptr, pMsg, "Awesome Hammer - Error", MB_OK | MB_ICONSTOP);
    return SPEW_ABORT;
  }

  return SPEW_CONTINUE;
}

// Init, shutdown
bool CHammerApp::PreInit() {
  SpewOutputFunc(HammerSpewFunc);
  if (!hammer_->InitSessionGameConfig(GetVProjectCmdLineValue())) return false;

  bool bDone = false;
  do {
    CFSSteamSetupInfo steamInfo;
    steamInfo.m_pDirectoryName = hammer_->GetDefaultModFullPath();
    steamInfo.m_bOnlyUseDirectoryName = true;
    steamInfo.m_bToolsMode = true;
    steamInfo.m_bSetSteamDLLPath = true;
    steamInfo.m_bSteam = file_system_->IsSteam();
    if (FileSystem_SetupSteamEnvironment(steamInfo) != FS_OK) {
      MessageBox(nullptr, "Failed to setup steam environment.",
                 "Awesome Hammer - Error", MB_OK | MB_ICONSTOP);
      return false;
    }

    CFSMountContentInfo fsInfo;
    fsInfo.m_pFileSystem = file_system_;
    fsInfo.m_bToolsMode = true;
    fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;
    if (!fsInfo.m_pDirectoryName) {
      Error(
          "FileSystem_LoadFileSystemModule: no -defaultgamedir or -game "
          "specified.");
    }

    if (FileSystem_MountContent(fsInfo) == FS_OK) {
      bDone = true;
    } else {
      char error[512];
      Q_snprintf(error, SOURCE_ARRAYSIZE(error), "%s",
                 FileSystem_GetLastErrorString());
      MessageBox(nullptr, error, "Awesome Hammer - Warning",
                 MB_OK | MB_ICONEXCLAMATION);

      if (hammer_->RequestNewConfig() == REQUEST_QUIT) return false;
    }

    FileSystem_AddSearchPath_Platform(fsInfo.m_pFileSystem,
                                      steamInfo.m_GameInfoPath);

  } while (!bDone);

  // Required to run through the editor
  material_system_->EnableEditorMaterials();

  // needed for VGUI model rendering
  material_system_->SetAdapter(0, MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE);

  return true;
}

void CHammerApp::PostShutdown() {}

// main application
int CHammerApp::Main() { return hammer_->MainLoop(); }
