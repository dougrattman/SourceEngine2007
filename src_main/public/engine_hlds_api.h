// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINE_HLDS_API_H
#define ENGINE_HLDS_API_H

#include <tuple>
#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "tier1/interface.h"

struct ModInfo_t {
  void *m_pInstance;
  // Executable directory ("c:/program files/half-life 2", for example)
  const ch *m_pBaseDirectory;
  // Mod name ("cstrike", for example)
  const ch *m_pInitialMod;
  // Root game name ("hl2", for example, in the case of cstrike)
  const ch *m_pInitialGame;
  CAppSystemGroup *m_pParentAppSystemGroup;
  bool m_bTextMode;
};

// Purpose: This is the interface exported by the engine.dll to allow a
// dedicated server front end application to host it.
the_interface IDedicatedServerAPI : public IAppSystem {
 public:
  // Initialize the engine with the specified base directory and interface
  // factories
  virtual bool ModInit(ModInfo_t & info) = 0;
  // Shutdown the engine
  virtual void ModShutdown() = 0;
  // Run a frame
  virtual bool RunFrame() = 0;
  // Insert text into console
  virtual void AddConsoleText(const ch *console_text) = 0;
  // Get current status to display in the hlds UI (console window title bar,
  // etc.)
  // [fps, active players count, max players count].
  virtual std::tuple<f32, i32, i32> GetStatus(ch * map_name,
                                              usize map_name_size) const = 0;
  // Get current hostname to display in the hlds UI (console window title bar,
  // etc.)
  virtual void GetHostname(ch * host_name, usize host_name_size) const = 0;
};

#define VENGINE_HLDS_API_VERSION "VENGINE_HLDS_API_VERSION003"

#endif  // ENGINE_HLDS_API_H
