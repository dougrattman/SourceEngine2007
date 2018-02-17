// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: The application object for apps that use tier2.

#ifndef SOURCE_APPFRAMEWORK_TIER2APP_H_
#define SOURCE_APPFRAMEWORK_TIER2APP_H_

#include "base/include/base_types.h"

#include "appframework/AppFramework.h"
#include "tier1/convar.h"
#include "tier2/tier2dm.h"

// The application object for apps that use tier2
class CTier2SteamApp : public CSteamAppSystemGroup {
  using BaseClass = CSteamAppSystemGroup;

 public:
  // Methods of IApplication
  bool PreInit() override {
    CreateInterfaceFn factory = GetFactory();
    ConnectTier1Libraries(&factory, 1);
    ConVar_Register(0);
    ConnectTier2Libraries(&factory, 1);
    return true;
  }

  void PostShutdown() override {
    DisconnectTier2Libraries();
    ConVar_Unregister();
    DisconnectTier1Libraries();
  }
};

// The application object for apps that use tier2 and datamodel.
class CTier2DmSteamApp : public CTier2SteamApp {
  using BaseClass = CTier2SteamApp;

 public:
  // Methods of IApplication
  bool PreInit() override {
    if (!BaseClass::PreInit()) return false;

    const CreateInterfaceFn factory = GetFactory();
    if (!ConnectDataModel(factory)) return false;

    const InitReturnVal_t return_Valkue = InitDataModel();
    return return_Valkue == INIT_OK;
  }

  void PostShutdown() override {
    ShutdownDataModel();
    DisconnectDataModel();
    BaseClass::PostShutdown();
  }
};

#endif  // SOURCE_APPFRAMEWORK_TIER2APP_H_
