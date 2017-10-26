// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ENGINE_HLDS_API_H
#define ENGINE_HLDS_API_H

#include "appframework/IAppSystem.h"
#include "tier1/interface.h"

#define VENGINE_HLDS_API_VERSION "VENGINE_HLDS_API_VERSION002"

struct ModInfo_t {
  void *m_pInstance;
  // Executable directory ("c:/program files/half-life 2", for example)
  const char *m_pBaseDirectory;
  // Mod name ("cstrike", for example)
  const char *m_pInitialMod;
  // Root game name ("hl2", for example, in the case of cstrike)
  const char *m_pInitialGame;
  CAppSystemGroup *m_pParentAppSystemGroup;
  bool m_bTextMode;
};

// Purpose: This is the interface exported by the engine.dll to allow a
// dedicated server front end application to host it.
abstract_class IDedicatedServerAPI : public IAppSystem {
 public:
  // Initialize the engine with the specified base directory and interface
  // factories
  virtual bool ModInit(ModInfo_t & info) = 0;
  // Shutdown the engine
  virtual void ModShutdown() = 0;
  // Run a frame
  virtual bool RunFrame() = 0;
  // Insert text into console
  virtual void AddConsoleText(char *console_text) = 0;
  // Get current status to display in the hlds UI (console window title bar,
  // etc.)
  virtual void UpdateStatus(float *fps, int *nActive, int *nMaxPlayers,
                            char *pszMap, int maxlen) = 0;
  // Get current hostname to display in the hlds UI (console window title bar,
  // etc.)
  virtual void UpdateHostname(char *pszHostname, int maxlen) = 0;
};

#endif  // ENGINE_HLDS_API_H
