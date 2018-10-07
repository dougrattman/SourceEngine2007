// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// The application objects for apps that use tier3.

#ifndef SOURCE_APPFRAMEWORK_INCLUDE_TIER3_APP_H_
#define SOURCE_APPFRAMEWORK_INCLUDE_TIER3_APP_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"

#include "include/tier2_app.h"
#include "tier3/tier3.h"
#include "vgui_controls/Controls.h"

// The application object for apps that use tier3.
class CTier3SteamApp : public CTier2SteamApp {
  using BaseClass = CTier2SteamApp;

 public:
  // Methods of IApplication.
  bool PreInit() override {
    if (BaseClass::PreInit()) {
      CreateInterfaceFn factory = GetFactory();
      ConnectTier3Libraries(&factory, 1);
      return true;
    }

    return false;
  }

  void PostShutdown() override {
    DisconnectTier3Libraries();
    BaseClass::PostShutdown();
  }
};

// The application object for apps that use tier3.
class CTier3DmSteamApp : public CTier2DmSteamApp {
  using BaseClass = CTier2DmSteamApp;

 public:
  // Methods of IApplication.
  bool PreInit() override {
    if (BaseClass::PreInit()) {
      CreateInterfaceFn factory = GetFactory();
      ConnectTier3Libraries(&factory, 1);
      return true;
    }

    return false;
  }

  void PostShutdown() override {
    DisconnectTier3Libraries();
    BaseClass::PostShutdown();
  }
};

// The application object for apps that use vgui.
class CVguiSteamApp : public CTier3SteamApp {
  using BaseClass = CTier3SteamApp;

 public:
  // Methods of IApplication.
  bool PreInit() override {
    if (BaseClass::PreInit()) {
      CreateInterfaceFn factory = GetFactory();
      return vgui::VGui_InitInterfacesList("CVguiSteamApp", &factory, 1);
    }

    return false;
  }
};

// The application object for apps that use vgui.
class CVguiDmSteamApp : public CTier3DmSteamApp {
  using BaseClass = CTier3DmSteamApp;

 public:
  // Methods of IApplication.
  bool PreInit() override {
    if (BaseClass::PreInit()) {
      CreateInterfaceFn factory = GetFactory();
      return vgui::VGui_InitInterfacesList("CVguiSteamApp", &factory, 1);
    }

    return false;
  }
};

#endif  // SOURCE_APPFRAMEWORK_INCLUDE_TIER3_APP_H_
