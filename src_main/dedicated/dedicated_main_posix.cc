// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "dedicated_os.h"

#include <dlfcn.h>
#include <malloc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "conproc.h"
#include "console/TextConsoleUnix.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "engine_hlds_api.h"
#include "icvar.h"
#include "idedicatedexports.h"
#include "istudiorender.h"
#include "isys.h"
#include "materialsystem/imaterialsystem.h"
#include "mathlib/mathlib.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "tier1/checksum_md5.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "vphysics_interface.h"

extern CTextConsoleUnix console;

ch g_szEXEName[256];

class CVCRHelpers : public IVCRHelpers {
 public:
  virtual void ErrorMessage(const ch *message) {
    fprintf(stderr, "ERROR: %s\n", message);
  }

  virtual void *GetMainWindow() { return nullptr; }
};

static CVCRHelpers g_VCRHelpers;

class DedicatedOsPosix : public IDedicatedOs {
 public:
  virtual ~DedicatedOsPosix() {}

  bool LoadModules(DedicatedSteamApp *steam_app) override {
    AppSystemInfo_t app_systems[] = {
        // loaded for backwards compatability, prevents crash on exit for old
        // game dlls
        {"bin/soundemittersystem.so.1", SOUNDEMITTERSYSTEM_INTERFACE_VERSION},
        {"bin/materialsystem.so.1", MATERIAL_SYSTEM_INTERFACE_VERSION},
        {"bin/studiorender.so.1", STUDIO_RENDER_INTERFACE_VERSION},
        {"bin/vphysics.so.1", VPHYSICS_INTERFACE_VERSION},
        {"bin/datacache.so.1", DATACACHE_INTERFACE_VERSION},
        {"bin/datacache.so.1", MDLCACHE_INTERFACE_VERSION},
        {"bin/datacache.so.1", STUDIO_DATA_CACHE_INTERFACE_VERSION},
        {"bin/engine.so.1", VENGINE_HLDS_API_VERSION},
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

    material_system->SetShaderAPI("bin/shaderapiempty.so.1");

    return true;
  }

  void Sleep(u32 msec) const override { usleep(msec * 1000); }

  bool GetExecutableName(ch *out, usize size) override {
    ch *name = strrchr(g_szEXEName, '/');
    if (name) {
      strcpy(out, name + 1);
      return true;
    }

    return false;
  }

  void ErrorMessage(const ch *msg) override {
    Error("%s.\n", msg);
    exit(-1);
  }

  bool WriteStatusText(ch *szText) override { return true; }
  void UpdateStatus(bool force) override {}

  uintptr_t LoadSharedLibrary(ch *so_name) override {
    ch cwd[SOURCE_MAX_PATH];
    if (!_getcwd(cwd, std::size(cwd))) {
      Error("getcwd failed (%d).\n", errno);
      return 0;
    }

    if (cwd[strlen(cwd) - 1] == '/') cwd[strlen(cwd) - 1] = '\0';

    ch so_path[SOURCE_MAX_PATH];
    Q_snprintf(so_path, std::size(so_path), "%s/%s", cwd, so_name);

    void *handle = dlopen(so_path, RTLD_NOW);
    if (handle) return reinterpret_cast<uintptr_t>(handle);

    Error("dlopen %s failed (%s).\n", so_path, dlerror());
    return 0;
  }

  bool FreeSharedLibrary(uintptr_t handle) override {
    return handle ? !dlclose(reinterpret_cast<void *>(handle)) : false;
  }

  bool CreateConsoleWindow(void) override { return true; }
  bool DestroyConsoleWindow(void) override { return true; }

  void ConsoleOutput(ch *string) override { console.Print(string); }
  ch *ConsoleInput() override { return console.GetLine(); }

  void Printf(ch *fmt, ...) override {
    // Dump text to debugging console.
    va_list arg_list;
    ch text[1024];

    va_start(arg_list, fmt);
    Q_vsnprintf(text, std::size(text), fmt, arg_list);
    va_end(arg_list);

    // Get Current text and append it.
    ConsoleOutput(text);
  }
};

IDedicatedOs *DedicatedOs() {
  static DedicatedOsPosix dedicated_os;
  return &dedicated_os;
}

extern int main(int argc, ch *argv[]);

SOURCE_API_EXPORT int DedicatedMain(int argc, ch *argv[]) {
  return main(argc, argv);
}
