// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include <cstdio>
#include <cstdlib>
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "filesystem.h"
#include "idedicatedexports.h"
#include "isys.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "tier1/strtools.h"
#include "vgui/vguihelpers.h"

CSysModule *s_hMatSystemModule = nullptr;
CSysModule *s_hEngineModule = nullptr;
CSysModule *s_hSoundEmitterModule = nullptr;

CreateInterfaceFn s_MaterialSystemFactory;
CreateInterfaceFn s_EngineFactory;
CreateInterfaceFn s_SoundEmitterFactory;

// Load support for third party .dlls ( gamehost )
void Load3rdParty() {
  if (CommandLine()->CheckParm("-usegh")) {
    Warning("No ghost injection allowed.");
  }
}

// Dummy funcion called by engine.
int EF_VID_ForceUnlockedAndReturnState() { return 0; }

// Dummy funcion called by engine.
void EF_VID_ForceLockState(int) {}

bool InitInstance() {
  Load3rdParty();
  return true;
}

void ProcessConsoleInput() {
  if (!engine) return;

  char *s;

  do {
    s = sys->ConsoleInput();
    if (s) {
      char szBuf[256];
      Q_snprintf(szBuf, SOURCE_ARRAYSIZE(szBuf), "%s\n", s);
      engine->AddConsoleText(szBuf);
    }
  } while (s);
}

void RunServer();

class CDedicatedExports : public CBaseAppSystem<IDedicatedExports> {
 public:
  void Sys_Printf(char *text) override {
    if (sys) {
      sys->Printf("%s", text);
    }
  }

  void RunServer() override {
    void RunServer();
    ::RunServer();
  }
};

EXPOSE_SINGLE_INTERFACE(CDedicatedExports, IDedicatedExports,
                        VENGINE_DEDICATEDEXPORTS_API_VERSION);

SpewRetval_t DedicatedSpewOutputFunc(SpewType_t spewType, char const *pMsg) {
  if (sys) {
    sys->Printf("%s", pMsg);
  }
#ifdef _WIN32
  Plat_DebugString(pMsg);
#endif

  if (spewType == SPEW_ERROR) {
    // In Windows vgui mode, make a message box or they won't ever see the
    // error.
#ifdef _WIN32
    extern bool g_bVGui;
    if (g_bVGui) {
      MessageBox(nullptr, pMsg, "Awesome Dedicated Server - Error",
                 MB_OK | MB_ICONERROR | MB_TASKMODAL);
    }
    TerminateProcess(GetCurrentProcess(), 1);
#elif OS_POSIX
    fflush(stdout);
    _exit(1);
#else
#error "Implement me"
#endif

    return SPEW_ABORT;
  }

  if (spewType == SPEW_ASSERT) {
    if (CommandLine()->FindParm("-noassert") == 0)
      return SPEW_DEBUGGER;
    else
      return SPEW_CONTINUE;
  }

  return SPEW_CONTINUE;
}

int Sys_GetExecutableName(char *out) {
#ifdef _WIN32
  if (!::GetModuleFileName(GetModuleHandle(nullptr), out, 256)) {
    return 0;
  }
#else
  extern char g_szEXEName[256];
  strcpy(out, g_szEXEName);
#endif
  return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetExecutableDir() {
  static char exedir[SOURCE_MAX_PATH];

  exedir[0] = 0;
  if (!Sys_GetExecutableName(exedir)) return NULL;

  char *pSlash;
  char *pSlash2;
  pSlash = strrchr(exedir, '\\');
  pSlash2 = strrchr(exedir, '/');
  if (pSlash2 > pSlash) {
    pSlash = pSlash2;
  }
  if (pSlash) {
    *pSlash = 0;
  }

  // Return the bin directory as the executable dir if it's not in there
  // because that's really where we're running from...
  int exeLen = strlen(exedir);
  if (exedir[exeLen - 4] != CORRECT_PATH_SEPARATOR ||
      exedir[exeLen - 3] != 'b' || exedir[exeLen - 2] != 'i' ||
      exedir[exeLen - 1] != 'n') {
    Q_strncat(exedir, "\\bin", sizeof(exedir), COPY_ALL_CHARACTERS);
    Q_FixSlashes(exedir);
  }

  return exedir;
}

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------
const char *UTIL_GetBaseDir(void) {
  static char basedir[SOURCE_MAX_PATH];

  char const *pOverrideDir = CommandLine()->CheckParm("-basedir");
  if (pOverrideDir) return pOverrideDir;

  basedir[0] = 0;
  const char *pExeDir = UTIL_GetExecutableDir();
  if (pExeDir) {
    strcpy(basedir, pExeDir);
    int dirlen = strlen(basedir);
    if (basedir[dirlen - 3] == 'b' && basedir[dirlen - 2] == 'i' &&
        basedir[dirlen - 1] == 'n') {
      basedir[dirlen - 4] = 0;
    }
  }

  return basedir;
}
