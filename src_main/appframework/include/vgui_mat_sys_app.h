// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Vgui material system app.

#ifndef SOURCE_APPFRAMEWORK_INCLUDE_VGUI_MAT_SYS_APP_H_
#define SOURCE_APPFRAMEWORK_INCLUDE_VGUI_MAT_SYS_APP_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "base/include/macros.h"

#include "include/tier3_app.h"

SOURCE_FORWARD_DECLARE_HANDLE(HWND);

// The application object.
class CVguiMatSysApp : public CVguiSteamApp {
  using BaseClass = CVguiSteamApp;

 public:
  CVguiMatSysApp();

  // Methods of IApplication.
  bool Create() override;
  bool PreInit() override;
  void PostShutdown() override;
  void Destroy() override;

 protected:
  void AppPumpMessages();

  // Sets the video mode.
  bool SetVideoMode();

  // Returns the window.
  void *GetAppWindow();

  // Gets the window size.
  i32 GetWindowWidth() const;
  i32 GetWindowHeight() const;

  // Sets up the game path.
  bool SetupSearchPaths(const ch *start_dir, bool use_only_start_dir,
                        bool is_tool);

 private:
  // Returns the app name.
  virtual const ch *GetAppName() = 0;
  virtual bool AppUsesReadPixels() { return false; }

  // Creates the app window.
  virtual HWND CreateAppWindow(ch const *title, bool is_windowed, i32 width,
                               i32 height);

  HWND m_HWnd;
  i32 m_nWidth;
  i32 m_nHeight;
};

#endif  // SOURCE_APPFRAMEWORK_INCLUDE_VGUI_MAT_SYS_APP_H_
