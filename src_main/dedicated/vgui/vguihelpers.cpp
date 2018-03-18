// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include <direct.h>
#include "base/include/windows/windows_light.h"

#include "FileSystem.h"
#include "IAdminServer.h"
#include "IVGuiModule.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/MainPanel.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Panel.h"

#include "tier0/include/memdbgon.h"

static CMainPanel *g_pMainPanel = NULL;  // the main panel to show
static CSysModule *g_hAdminServerModule;
IAdminServer *g_pAdminServer = NULL;
static IVGuiModule *g_pAdminVGuiModule = NULL;

void *DedicatedFactory(const char *pName, int *pReturnCode);

//-----------------------------------------------------------------------------
// Purpose: Starts up the VGUI system and loads the base panel
//-----------------------------------------------------------------------------
int StartVGUI(CreateInterfaceFn dedicatedFactory) {
  // the "base dir" so we can scan mod name
  g_pFullFileSystem->AddSearchPath(".", "MAIN");
  // the main platform dir
  g_pFullFileSystem->AddSearchPath("platform", "PLATFORM", PATH_ADD_TO_HEAD);

  vgui::ivgui()->SetSleep(false);

  // find our configuration directory
  char config_dir[SOURCE_MAX_PATH], steam_path[SOURCE_MAX_PATH];
  usize steam_path_size;

  if (!getenv_s(&steam_path_size, steam_path, "SteamInstallPath") &&
      steam_path_size > 0) {
    // put the config dir directly under steam
    Q_snprintf(config_dir, SOURCE_ARRAYSIZE(config_dir), "%s/config",
               steam_path);
  } else {
    // we're not running steam, so just put the config dir under the platform
    Q_strncpy(config_dir, "platform/config", SOURCE_ARRAYSIZE(config_dir));
  }
  g_pFullFileSystem->CreateDirHierarchy("config", "PLATFORM");
  g_pFullFileSystem->AddSearchPath(config_dir, "CONFIG", PATH_ADD_TO_HEAD);

  // initialize the user configuration file
  vgui::system()->SetUserConfigFile("DedicatedServerDialogConfig.vdf",
                                    "CONFIG");

  // Init the surface
  g_pMainPanel = new CMainPanel();
  g_pMainPanel->SetVisible(true);

  vgui::surface()->SetEmbeddedPanel(g_pMainPanel->GetVPanel());

  // load the scheme
  vgui::scheme()->LoadSchemeFromFile("Resource/SourceScheme.res", NULL);

  // localization
  g_pVGuiLocalize->AddFile("Resource/platform_%language%.txt");
  g_pVGuiLocalize->AddFile("Resource/vgui_%language%.txt");
  g_pVGuiLocalize->AddFile("Admin/server_%language%.txt");

  // Start vgui
  vgui::ivgui()->Start();

  // load the module
  g_pFullFileSystem->GetLocalCopy("bin/adminserver.dll");
  g_hAdminServerModule = g_pFullFileSystem->LoadModule("adminserver");
  Assert(g_hAdminServerModule != NULL);
  CreateInterfaceFn adminFactory = NULL;

  if (!g_hAdminServerModule) {
    vgui::ivgui()->DPrintf2(
        "Admin Error: module version (bin/adminserver.dll, %s) invalid, not "
        "loading\n",
        IMANAGESERVER_INTERFACE_VERSION);
  } else {
    // make sure we get the right version
    adminFactory = Sys_GetFactory(g_hAdminServerModule);
    g_pAdminServer =
        (IAdminServer *)adminFactory(ADMINSERVER_INTERFACE_VERSION, NULL);
    g_pAdminVGuiModule =
        (IVGuiModule *)adminFactory("VGuiModuleAdminServer001", NULL);
    Assert(g_pAdminServer != NULL);
    Assert(g_pAdminVGuiModule != NULL);
    if (!g_pAdminServer || !g_pAdminVGuiModule) {
      vgui::ivgui()->DPrintf2(
          "Admin Error: module version (bin/adminserver.dll, %s) invalid, "
          "not loading\n",
          IMANAGESERVER_INTERFACE_VERSION);
    }
  }

  // finish initializing admin module
  g_pAdminVGuiModule->Initialize(&dedicatedFactory, 1);
  g_pAdminVGuiModule->PostInitialize(&adminFactory, 1);
  g_pAdminVGuiModule->SetParent(g_pMainPanel->GetVPanel());

  // finish setting up main panel
  g_pMainPanel->Initialize();
  g_pMainPanel->Open();

  return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the VGUI system
//-----------------------------------------------------------------------------
void StopVGUI() {
  SetEvent(g_pMainPanel->GetShutdownHandle());

  delete g_pMainPanel;
  g_pMainPanel = NULL;

  if (g_hAdminServerModule) {
    g_pAdminVGuiModule->Shutdown();
    Sys_UnloadModule(g_hAdminServerModule);
  }
}

//-----------------------------------------------------------------------------
// Purpose: Run a single VGUI frame
//-----------------------------------------------------------------------------
void RunVGUIFrame() { vgui::ivgui()->RunFrame(); }

bool VGUIIsStopping() { return g_pMainPanel->Stopping(); }

bool VGUIIsRunning() { return vgui::ivgui()->IsRunning(); }

bool VGUIIsInConfig() { return g_pMainPanel->IsInConfig(); }

void VGUIFinishedConfig() {
  Assert(g_pMainPanel);
  if (g_pMainPanel)  // engine is loaded, pass the message on
  {
    SetEvent(g_pMainPanel->GetShutdownHandle());
  }
}

void VGUIPrintf(const char *msg) {
  if (!g_pMainPanel || VGUIIsInConfig() || VGUIIsStopping()) {
    ::MessageBox(NULL, msg, "Dedicated Server Error", MB_OK | MB_TOPMOST);
  } else if (g_pMainPanel) {
    g_pMainPanel->AddConsoleText(msg);
  }
}

#endif  // _WIN32
