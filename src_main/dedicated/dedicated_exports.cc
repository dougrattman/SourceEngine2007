// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "idedicatedexports.h"

#include "dedicated_common.h"
#include "dedicated_steam_app.h"
#include "engine_hlds_api.h"
#include "idedicated_os.h"
#include "vgui/vguihelpers.h"

void ProcessConsoleInput() {
  if (!engine) return;

  char *input, console_text[512];

  do {
    input = DedicatedOs()->ConsoleInput();

    if (input) {
      Q_snprintf(console_text, SOURCE_ARRAYSIZE(console_text), "%s\n", input);
      engine->AddConsoleText(console_text);
    }
  } while (input);
}

class DedicatedExports : public CBaseAppSystem<IDedicatedExports> {
 public:
  void Sys_Printf(char *text) override { DedicatedOs()->Printf("%s", text); }

  void RunServer() override {
#ifdef OS_WIN
    extern char *gpszCvars;

    if (gpszCvars) engine->AddConsoleText(gpszCvars);
#endif

    int bDone = 0;

    // run 2 engine frames first to get the engine to load its resources
    if (g_bVGui) {
#ifdef OS_WIN
      RunVGUIFrame();
#endif
    }

    if (!engine->RunFrame()) return;

    if (g_bVGui) {
#ifdef OS_WIN
      RunVGUIFrame();
#endif
    }

    if (!engine->RunFrame()) return;

    if (g_bVGui) {
#ifdef OS_WIN
      VGUIFinishedConfig();
      RunVGUIFrame();
#endif
    }

    while (true) {
      if (bDone) break;

      // Running really fast, yield some time to other apps
      if (VCRGetMode() != VCR_Playback) {
        DedicatedOs()->Sleep(1);
      }

#ifdef OS_WIN
      MSG msg;

      while (VCRHook_PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          bDone = true;
          break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // NOTE: Under some implementations of Win9x,
      // dispatching messages can cause the FPU control word to change
      SetupFPUControlWord();

      if (bDone /*|| gbAppHasBeenTerminated*/) break;
#endif  // OS_WIN

      if (g_bVGui) {
#ifdef OS_WIN
        RunVGUIFrame();
#endif
      } else {
        ProcessConsoleInput();
      }

      if (!engine->RunFrame()) bDone = true;

      DedicatedOs()->UpdateStatus(0 /* don't force */);
    }
  }
};

EXPOSE_SINGLE_INTERFACE(DedicatedExports, IDedicatedExports,
                        VENGINE_DEDICATEDEXPORTS_API_VERSION);
