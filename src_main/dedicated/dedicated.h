// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order.

#ifndef SOURCE_DEDICATED_DEDICATED_H_
#define SOURCE_DEDICATED_DEDICATED_H_

#include "appframework/tier3app.h"

class IDedicatedServerAPI;

extern IDedicatedServerAPI *engine;

// Inner loop: initialize, shutdown main systems, load steam to
#ifdef _LINUX
#define DEDICATED_BASECLASS CTier2SteamApp
#else
#define DEDICATED_BASECLASS CVguiSteamApp
#endif

class CDedicatedAppSystemGroup : public DEDICATED_BASECLASS {
  using BaseClass = DEDICATED_BASECLASS;

 public:
  // Methods of IApplication.
  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

  // Used to chain to base class.
  AppModule_t LoadModule(CreateInterfaceFn factory) {
    return CSteamAppSystemGroup::LoadModule(factory);
  }

  // Method to add various global singleton systems.
  bool AddSystems(AppSystemInfo_t *systems) {
    return CSteamAppSystemGroup::AddSystems(systems);
  }

  // Find system by interface name.
  void *FindSystem(const char *interface_name) {
    return CSteamAppSystemGroup::FindSystem(interface_name);
  }
};

#endif  // SOURCE_DEDICATED_DEDICATED_H_
