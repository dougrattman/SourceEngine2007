// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINE_LAUNCHER_API_H_
#define ENGINE_LAUNCHER_API_H_

#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "base/include/macros.h"

class CAppSystemGroup;

struct StartupInfo_t {
  void *m_pInstance;
  const ch *m_pBaseDirectory;  // Executable directory ("c:/program
                               // files/half-life 2", for example)
  const ch *m_pInitialMod;     // Mod name ("cstrike", for example)
  const ch *m_pInitialGame;    // Root game name ("hl2", for example, in the
                               // case of cstrike)
  CAppSystemGroup *m_pParentAppSystemGroup;
  bool m_bTextMode;
};

// Return values from the initialization stage of the application framework
enum {
  INIT_RESTART = INIT_LAST_VAL,
  RUN_FIRST_VAL,
};

// Return values from IEngineAPI::Run.
enum {
  RUN_OK = RUN_FIRST_VAL,
  RUN_RESTART,
};

SOURCE_FORWARD_DECLARE_HANDLE(HWND);

// Main engine interface to launcher + tools
the_interface IEngineAPI : public IAppSystem {
 public:
  // This function must be called before init
  virtual void SetStartupInfo(StartupInfo_t & info) = 0;

  // Run the engine
  virtual i32 Run() = 0;

  // Sets the engine to run in a particular editor window
  virtual void SetEngineWindow(HWND hWnd) = 0;

  // Sets the engine to run in a particular editor window
  virtual void PostConsoleCommand(const ch *command) = 0;

  // Are we running the simulation?
  virtual bool IsRunningSimulation() const = 0;

  // Start/stop running the simulation
  virtual void ActivateSimulation(bool is_active) = 0;

  // Reset the map we're on
  virtual void SetMap(const ch *map_name) = 0;
};

#define VENGINE_LAUNCHER_API_VERSION "VENGINE_LAUNCHER_API_VERSION005"

#endif  // ENGINE_LAUNCHER_APIH
