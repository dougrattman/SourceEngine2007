// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order.

#ifndef SOURCE_DEDICATED_DEDICATED_STEAM_APP_H_
#define SOURCE_DEDICATED_DEDICATED_STEAM_APP_H_

#include "appframework/tier3app.h"
#include "base/include/windows/scoped_winsock_initializer.h"

// Inner loop: initialize, shutdown main systems, load steam to
#ifdef OS_POSIX
#define DEDICATED_BASECLASS CTier2SteamApp
#else
#define DEDICATED_BASECLASS CVguiSteamApp
#endif

class DedicatedSteamApp : public DEDICATED_BASECLASS {
  using BaseClass = DEDICATED_BASECLASS;

 public:
  DedicatedSteamApp()
      : scoped_winsock_initializer_{
            source::windows::WinsockVersion::Version2_2} {}

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
  template <typename T>
  T *FindSystem(const ch *interface_name) {
    return CSteamAppSystemGroup::FindSystem<T>(interface_name);
  }

 private:
  source::windows::ScopedWinsockInitializer scoped_winsock_initializer_;
};

#endif  // SOURCE_DEDICATED_DEDICATED_STEAM_APP_H_
