// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "idedicated_os.h"

#include "datacache/idatacache.h"
#include "dedicated_common.h"
#include "dedicated_steam_app.h"
#include "engine_hlds_api.h"
#include "inputsystem/inputsystem.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/IVGui.h"
#include "vgui/vguihelpers.h"
#include "vphysics/include/vphysics_interface.h"

#include "console/textconsolewin32.h"
#include "console/conproc.h"

extern CTextConsoleWin32 console;
IDedicatedServerAPI *engine;

// Implements Windows specific layer (loosely).
class DedicatedOsWindows : public IDedicatedOs {
 public:
  virtual ~DedicatedOsWindows() {}

  // Loading modules used by the dedicated server.
  bool LoadModules(DedicatedSteamApp *steam_app) override {
    AppSystemInfo_t app_systems[] = {
        // NOTE: This one must be first!!
        {"engine.dll", CVAR_QUERY_INTERFACE_VERSION},
        {"inputsystem.dll", INPUTSYSTEM_INTERFACE_VERSION},
        {"materialsystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION},
        {"studiorender.dll", STUDIO_RENDER_INTERFACE_VERSION},
        {"vphysics.dll", VPHYSICS_INTERFACE_VERSION},
        {"datacache.dll", DATACACHE_INTERFACE_VERSION},
        {"datacache.dll", MDLCACHE_INTERFACE_VERSION},
        {"datacache.dll", STUDIO_DATA_CACHE_INTERFACE_VERSION},
        {"vgui2.dll", VGUI_IVGUI_INTERFACE_VERSION},
        {"engine.dll", VENGINE_HLDS_API_VERSION},
        // Required to terminate the list
        {"", ""}};

    if (!steam_app->AddSystems(app_systems)) return false;

    engine =
        steam_app->FindSystem<IDedicatedServerAPI>(VENGINE_HLDS_API_VERSION);
    if (!engine) {
      Error("No Dedicated server API interface %s found.\n",
            VENGINE_HLDS_API_VERSION);
      return false;
    }

    auto *material_system = steam_app->FindSystem<IMaterialSystem>(
        MATERIAL_SYSTEM_INTERFACE_VERSION);
    if (!material_system) {
      Error("No Material system interface %s found.\n",
            MATERIAL_SYSTEM_INTERFACE_VERSION);
      return false;
    }

    material_system->SetShaderAPI("shaderapiempty.dll");

    return true;
  }

  void Sleep(u32 milliseconds) const override { ::Sleep(milliseconds); }

  bool GetExecutableName(ch *exe_name, usize exe_name_size) override {
    return GetExeName(exe_name, exe_name_size);
  }

  void ErrorMessage(const ch *msg) override {
    MessageBoxA(nullptr, msg, "Dedicated Half-Life 2 Server - Error",
                MB_ICONERROR | MB_OK);
    PostQuitMessage(0);
  }

  bool WriteStatusText(ch *status_text) override {
    return !SetConsoleTitleA(status_text);
  }

  void UpdateStatus(bool force) override {
    if (!engine) return;

    static f64 last_time = 0.0;
    const f64 current_time = Plat_FloatTime();

    if (!force) {
      if (current_time - last_time < 0.5) return;
    }

    last_time = current_time;

    ch map_name[64], host_name[128];
    auto [fps, current_players_count, max_players_count] =
        engine->GetStatus(map_name, std::size(map_name));
    engine->GetHostname(host_name, std::size(host_name));

    console.SetTitle(host_name);

    ch status[256];
    sprintf_s(status, "%.1f fps %2i/%2i on map %16s", fps,
              current_players_count, max_players_count, map_name);

    console.SetStatusLine(status);
    console.UpdateStatus();
  }

  uintptr_t LoadSharedLibrary(ch *lib_path) override {
    return reinterpret_cast<uintptr_t>(LoadLibraryA(lib_path));
  }

  bool FreeSharedLibrary(uintptr_t library) override {
    return library ? FreeLibrary(reinterpret_cast<HMODULE>(library)) : false;
  }

  bool CreateConsoleWindow() override {
    if (!AllocConsole()) return false;

    InitConProc();
    return true;
  }

  bool DestroyConsoleWindow() override {
    bool is_free = FreeConsole();

    // shut down QHOST hooks if necessary
    DeinitConProc();
    return is_free;
  }

  void ConsoleOutput(ch *string) override {
    if (g_bVGui) {
      VGUIPrintf(string);
    } else {
      console.Print(string);
    }
  }

  ch *ConsoleInput() override { return console.GetLine(); }

  void Printf(const ch *format, ...) override {
    // Dump text to debugging console.
    va_list arg_list;
    ch text[1024];

    va_start(arg_list, format);
    vsprintf_s(text, format, arg_list);
    va_end(arg_list);

    // Get current text and append it.
    ConsoleOutput(text);
  }
};

IDedicatedOs *DedicatedOs() {
  static DedicatedOsWindows dedicated_os;
  return &dedicated_os;
}
