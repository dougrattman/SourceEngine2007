// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "source_app_system_group.h"

#include "base/include/windows/windows_light.h"

#include <Objbase.h>

#include "IHammer.h"
#include "appframework/AppFramework.h"
#include "appframework/IAppSystemGroup.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "base/include/chrono.h"
#include "base/include/windows/scoped_com_initializer.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "engine_launcher_api.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "icvar.h"
#include "inputsystem/iinputsystem.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "p4lib/ip4.h"
#include "tier0/include/icommandline.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vphysics_interface.h"
#include "vstdlib/iprocessutils.h"

#include "iresource_listing_writer.h"

// Instantiate all main libraries
bool SourceAppSystemGroup::Create() {
  auto *file_system = FindSystem<IFileSystem>(FILESYSTEM_INTERFACE_VERSION);
  if (!file_system) {
    Error("No File System interface %s found.", FILESYSTEM_INTERFACE_VERSION);
    return false;
  }

  file_system->InstallDirtyDiskReportFunc([]() {});

  if (source::windows::failed(scoped_com_initializer_.errno_code())) {
    Error("COM initialization failed: %s.",
          source::windows::make_windows_errno_info(
              scoped_com_initializer_.errno_code())
              .description);
    return false;
  }

  // Are we running in edit mode?
  is_edit_mode_ = command_line_->CheckParm("-edit");

  f64 app_system_load_time;
  const bool is_ok =
      source::chrono::HpetTimer::TimeIt(app_system_load_time, [&]() {
        AppSystemInfo_t app_system_infos[]{
            // NOTE: This one must be first!!
            {"engine.dll", CVAR_QUERY_INTERFACE_VERSION},
            {"inputsystem.dll", INPUTSYSTEM_INTERFACE_VERSION},
            {"materialsystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION},
            {"datacache.dll", DATACACHE_INTERFACE_VERSION},
            {"datacache.dll", MDLCACHE_INTERFACE_VERSION},
            {"datacache.dll", STUDIO_DATA_CACHE_INTERFACE_VERSION},
            {"studiorender.dll", STUDIO_RENDER_INTERFACE_VERSION},
            {"vphysics.dll", VPHYSICS_INTERFACE_VERSION},
            {"valve_avi.dll", AVI_INTERFACE_VERSION},
            {"valve_avi.dll", BIK_INTERFACE_VERSION},
            // NOTE: This has to occur before vgui2.dll so it replaces vgui2's
            // surface
            // implementation
            {"vguimatsurface.dll", VGUI_SURFACE_INTERFACE_VERSION},
            {"vgui2.dll", VGUI_IVGUI_INTERFACE_VERSION},
            {"engine.dll", VENGINE_LAUNCHER_API_VERSION},
            // Required to terminate the list
            {"", ""}};

        if (!AddSystems(app_system_infos)) return false;

        // Hook in datamodel and p4 control if we're running with -tools.
        if (command_line_->FindParm("-tools") ||
            command_line_->FindParm("-p4")) {
          AppModule_t p5lib_module{LoadModule("p4lib.dll")};
          auto *ip4 = AddSystem<IP4>(p5lib_module, P4_INTERFACE_VERSION);
          if (!ip4) return false;

          AppModule_t vstdlib_module{LoadModule("vstdlib.dll")};
          auto *process_utils = AddSystem<IProcessUtils>(
              vstdlib_module, PROCESS_UTILS_INTERFACE_VERSION);
          if (!process_utils) return false;
        }

        engine_api_ = FindSystem<IEngineAPI>(VENGINE_LAUNCHER_API_VERSION);
        if (!engine_api_) {
          Error("No Engine API interface %s found.",
                VENGINE_LAUNCHER_API_VERSION);
          return false;
        }

        // Load the hammer DLL if we're in editor mode.
        if (is_edit_mode_) {
          AppModule_t hammer_module{LoadModule("hammer_dll.dll")};

          hammer_ = AddSystem<IHammer>(hammer_module, INTERFACEVERSION_HAMMER);
          if (!hammer_) return false;
        }

        // Load up the appropriate shader DLL. This has to be done before
        // connection.
        const char *shader_api_dll_name{!command_line_->FindParm("-noshaderapi")
                                            ? "shaderapidx9.dll"
                                            : "shaderapiempty.dll"};

        auto *material_system =
            FindSystem<IMaterialSystem>(MATERIAL_SYSTEM_INTERFACE_VERSION);
        if (!material_system) {
          Error("No Material System interface %s found.",
                MATERIAL_SYSTEM_INTERFACE_VERSION);
          return false;
        }

        material_system->SetShaderAPI(shader_api_dll_name);

        return true;
      });

  Plat_TimestampedLog(
      "SourceAppSystemGroup::Create: %.4f seconds to load modules and get "
      "factories.",
      app_system_load_time);

  return is_ok;
}

constexpr char kDefaultHl2GameDir[]{"hl2"};

bool SourceAppSystemGroup::PreInit() {
  CreateInterfaceFn factory = GetFactory();
  ConnectTier1Libraries(&factory, 1);
  ConVar_Register();
  ConnectTier2Libraries(&factory, 1);
  ConnectTier3Libraries(&factory, 1);

  if (!g_pFullFileSystem || !g_pMaterialSystem) return false;

  CFSSteamSetupInfo steam_info;
  steam_info.m_bOnlyUseDirectoryName = true;
  steam_info.m_bSteam = g_pFullFileSystem->IsSteam();
  steam_info.m_bToolsMode = false;
  steam_info.m_bSetSteamDLLPath = false;
  steam_info.m_pDirectoryName = DetermineDefaultMod();
  if (!steam_info.m_pDirectoryName) {
    steam_info.m_pDirectoryName = DetermineDefaultGame();
    if (!steam_info.m_pDirectoryName) {
      Error(
          "FileSystem_LoadFileSystemModule: no %s or %s "
          "specified.",
          source::tier0::command_line_switches::kDefaultGamePath,
          source::tier0::command_line_switches::kGamePath);
    }
  }

  if (FileSystem_SetupSteamEnvironment(steam_info) != FS_OK) return false;

  CFSMountContentInfo fsInfo;
  fsInfo.m_bToolsMode = is_edit_mode_;
  fsInfo.m_pDirectoryName = steam_info.m_GameInfoPath;
  fsInfo.m_pFileSystem = g_pFullFileSystem;
  if (FileSystem_MountContent(fsInfo) != FS_OK) return false;

  fsInfo.m_pFileSystem->AddSearchPath("platform", "PLATFORM");

  // This will get called multiple times due to being here, but only the first
  // one will do anything
  ResourceListing()->Init(
      base_directory_,
      command_line_->ParmValue(source::tier0::command_line_switches::kGamePath,
                               kDefaultHl2GameDir));

  // This will also get called each time, but will actually fix up the command
  // line as needed
  ResourceListing()->SetupCommandLine();

  // TODO(d.rattman): Logfiles is mod-specific, needs to move into the engine.
  file_system_access_logger_.Init();

  // Required to run through the editor
  if (is_edit_mode_) g_pMaterialSystem->EnableEditorMaterials();

  StartupInfo_t info;
  info.m_pInstance = GetAppInstance();
  info.m_pBaseDirectory = base_directory_;
  info.m_pInitialMod = DetermineDefaultMod();
  info.m_pInitialGame = DetermineDefaultGame();
  info.m_pParentAppSystemGroup = this;
  info.m_bTextMode = is_text_mode_;

  engine_api_->SetStartupInfo(info);

  return true;
}

int SourceAppSystemGroup::Main() { return engine_api_->Run(); }

void SourceAppSystemGroup::PostShutdown() {
  // TODO(d.rattman): Logfiles is mod-specific, needs to move into the engine.
  file_system_access_logger_.Shutdown();

  ResourceListing()->Shutdown();

  DisconnectTier3Libraries();
  DisconnectTier2Libraries();
  ConVar_Unregister();
  DisconnectTier1Libraries();
}

void SourceAppSystemGroup::Destroy() {
  engine_api_ = nullptr;
  g_pMaterialSystem = nullptr;
  hammer_ = nullptr;
}

// Determines the initial mod to use at load time. We eventually (hopefully)
// will be able to switch mods at runtime because the engine/hammer integration
// really wants this feature.
const char *SourceAppSystemGroup::DetermineDefaultMod() {
  return !is_edit_mode_ ? command_line_->ParmValue(
                              source::tier0::command_line_switches::kGamePath,
                              kDefaultHl2GameDir)
                        : hammer_->GetDefaultMod();
}

const char *SourceAppSystemGroup::DetermineDefaultGame() {
  return !is_edit_mode_
             ? command_line_->ParmValue(
                   source::tier0::command_line_switches::kDefaultGamePath,
                   kDefaultHl2GameDir)
             : hammer_->GetDefaultGame();
}
