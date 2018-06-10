// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Launcher for hammer, which is sitting in its own DLL

#include "base/include/windows/windows_light.h"

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

// Indicates to hybrid graphics systems to prefer the discrete part by default.
extern "C" {
// Starting with the Release 302 drivers, application developers can direct the
// Optimus driver at runtime to use the High Performance Graphics to render any
// application —- even those applications for which there is no existing
// application profile.  See
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// This will select the high performance GPU as long as no profile exists that
// assigns the application to another GPU.  Please make sure to use a 13.35 or
// newer driver.  Older drivers do not support this.  See
// https://community.amd.com/thread/169965
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
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
                                      std::size(filesystem_dll_path),
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
      MessageBox(nullptr, FileSystem_GetLastErrorString(),
                 "Awesome Hammer - Warning", MB_OK | MB_ICONEXCLAMATION);

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
