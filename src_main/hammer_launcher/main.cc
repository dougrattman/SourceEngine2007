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
#include "vphysics/include/vphysics_interface.h"
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

namespace {
DbgReturn HammerOnDebugEvent(DbgLevel type, char const *message) {
  if (type == kDbgLevelAssert) return kDbgBreak;

  if (type == kDbgLevelError || type == kDbgLevelWarning) {
    MessageBox(nullptr, message, "Awesome Hammer - Error", MB_OK | MB_ICONSTOP);
    return kDbgAbort;
  }

  return kDbgContinue;
}
}  // namespace

// Hammer app.
class HammerApp : public CAppSystemGroup {
 public:
  // Create all singleton systems.
  bool Create() override {
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

    const AppModule_t filesystem_module = LoadModule(filesystem_dll_path);
    file_system_ =
        AddSystem<IFileSystem>(filesystem_module, FILESYSTEM_INTERFACE_VERSION);

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

  // Init, shutdown
  bool PreInit() override {
    SetDbgOutputCallback(HammerOnDebugEvent);

    if (!hammer_->InitSessionGameConfig(GetVProjectCmdLineValue())) {
      return false;
    }

    bool is_done = false;
    do {
      CFSSteamSetupInfo steam_info;
      steam_info.m_pDirectoryName = hammer_->GetDefaultModFullPath();
      steam_info.m_bOnlyUseDirectoryName = true;
      steam_info.m_bToolsMode = true;
      steam_info.m_bSetSteamDLLPath = true;
      steam_info.m_bSteam = file_system_->IsSteam();

      if (FileSystem_SetupSteamEnvironment(steam_info) != FS_OK) {
        MessageBoxW(nullptr, L"Failed to setup Steam environment.",
                    L"Awesome Hammer - Error", MB_OK | MB_ICONSTOP);
        return false;
      }

      CFSMountContentInfo fs_info;
      fs_info.m_bToolsMode = true;
      fs_info.m_pDirectoryName = steam_info.m_GameInfoPath;
      fs_info.m_pFileSystem = file_system_;

      if (!fs_info.m_pDirectoryName) {
        Error("Hammer PreInit: no %s or %s specified.",
              source::tier0::command_line_switches::kDefaultGamePath,
              source::tier0::command_line_switches::kGamePath);
        return false;
      }

      if (FileSystem_MountContent(fs_info) == FS_OK) {
        is_done = true;
      } else {
        MessageBox(nullptr, FileSystem_GetLastErrorString(),
                   "Awesome Hammer - Warning", MB_OK | MB_ICONEXCLAMATION);

        if (hammer_->RequestNewConfig() == REQUEST_QUIT) return false;
      }

      FileSystem_AddSearchPath_Platform(fs_info.m_pFileSystem,
                                        steam_info.m_GameInfoPath);
    } while (!is_done);

    // Required to run through the editor
    material_system_->EnableEditorMaterials();

    // needed for VGUI model rendering
    material_system_->SetAdapter(0, MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE);

    return true;
  }

  // main application
  int Main() override { return hammer_->MainLoop(); }

  void PostShutdown() override {}

  void Destroy() override {
    file_system_ = nullptr;
    material_system_ = nullptr;
    data_cache_ = nullptr;
    hammer_ = nullptr;
    input_system_ = nullptr;
  }

 private:
  IHammer *hammer_;
  IMaterialSystem *material_system_;
  IFileSystem *file_system_;
  IDataCache *data_cache_;
  IInputSystem *input_system_;
};

namespace {
// Define the application object
HammerApp hammer_app;
}  // namespace
DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(hammer_app);
