// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "isys.h"

#include "datacache/idatacache.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "inputsystem/inputsystem.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/IVGui.h"
#include "vgui/vguihelpers.h"
#include "vphysics_interface.h"

#include "console/TextConsoleWin32.h"
#include "console/conproc.h"

#undef LoadLibrary

// Purpose: Implements OS Specific layer (loosely)
class CSys : public ISys {
 public:
  virtual ~CSys() { sys = nullptr; }
  virtual bool LoadModules(CDedicatedAppSystemGroup *pAppSystemGroup);

  void Sleep(int msec) { ::Sleep(msec); }
  bool GetExecutableName(char *out);
  void ErrorMessage(int level, const char *msg);

  void WriteStatusText(char *szText);
  void UpdateStatus(int force);

  uintptr_t LoadLibrary(char *lib);
  void FreeLibrary(uintptr_t library);

  bool CreateConsoleWindow(void);
  void DestroyConsoleWindow(void);

  void ConsoleOutput(char *string);
  char *ConsoleInput();
  void Printf(const char *fmt, ...);
};

static CSys g_Sys;
ISys *sys = &g_Sys;

extern CTextConsoleWin32 console;
extern bool g_bVGui;

uintptr_t CSys::LoadLibrary(char *lib) {
  return reinterpret_cast<uintptr_t>(::LoadLibraryA(lib));
}

void CSys::FreeLibrary(uintptr_t library) {
  if (library) {
    ::FreeLibrary(reinterpret_cast<HMODULE>(library));
  }
}

bool CSys::GetExecutableName(char *out) {
  return ::GetModuleFileName(GetModuleHandle(nullptr), out, 256) != FALSE;
}

void CSys::ErrorMessage(int level, const char *msg) {
  MessageBox(nullptr, msg, "Half-Life", MB_ICONERROR | MB_OK);
  PostQuitMessage(0);
}

void CSys::UpdateStatus(int force) {
  static double last_time = 0.0;

  if (!engine) return;

  const double current_time = Sys_FloatTime();

  if (!force) {
    if ((current_time - last_time) < 0.5) return;
  }

  last_time = current_time;

  char szMap[64], szHostname[128];
  int n, nMax;
  float fps;

  engine->UpdateStatus(&fps, &n, &nMax, szMap, ARRAYSIZE(szMap));
  engine->UpdateHostname(szHostname, ARRAYSIZE(szHostname));

  console.SetTitle(szHostname);

  char szPrompt[256];
  Q_snprintf(szPrompt, ARRAYSIZE(szPrompt), "%.1f fps %2i/%2i on map %16s", fps,
             n, nMax, szMap);

  console.SetStatusLine(szPrompt);
  console.UpdateStatus();
}

void CSys::ConsoleOutput(char *string) {
  if (g_bVGui) {
    VGUIPrintf(string);
  } else {
    console.Print(string);
  }
}

void CSys::Printf(const char *fmt, ...) {
  // Dump text to debugging console.
  va_list argptr;
  char szText[1024];

  va_start(argptr, fmt);
  Q_vsnprintf(szText, ARRAYSIZE(szText), fmt, argptr);
  va_end(argptr);

  // Get Current text and append it.
  ConsoleOutput(szText);
}

char *CSys::ConsoleInput() { return console.GetLine(); }

void CSys::WriteStatusText(char *szText) { SetConsoleTitle(szText); }

bool CSys::CreateConsoleWindow() {
  if (!AllocConsole()) {
    return false;
  }

  InitConProc();
  return true;
}

void CSys::DestroyConsoleWindow() {
  FreeConsole();
  // shut down QHOST hooks if necessary
  DeinitConProc();
}

// Loading modules used by the dedicated server.
bool CSys::LoadModules(CDedicatedAppSystemGroup *pAppSystemGroup) {
  AppSystemInfo_t appSystems[] = {
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

  if (!pAppSystemGroup->AddSystems(appSystems)) return false;

  engine = (IDedicatedServerAPI *)pAppSystemGroup->FindSystem(
      VENGINE_HLDS_API_VERSION);
  if (!engine) {
    Error("No Dedicated Server API interface %s found.\n",
          VENGINE_HLDS_API_VERSION);
    return false;
  }

  auto *material_system = (IMaterialSystem *)pAppSystemGroup->FindSystem(
      MATERIAL_SYSTEM_INTERFACE_VERSION);
  material_system->SetShaderAPI("shaderapiempty.dll");
  if (!material_system) {
    Error("No Material System interface %s found.\n",
          MATERIAL_SYSTEM_INTERFACE_VERSION);
    return false;
  }

  return true;
}
