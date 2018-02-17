// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Material editor.

#ifndef SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_
#define SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_

#include "base/include/base_types.h"

#include "appframework/tier3app.h"

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
  virtual void *CreateAppWindow(ch const *title, bool is_windowed, i32 width,
                                i32 height);

  void *m_HWnd;
  i32 m_nWidth;
  i32 m_nHeight;
};

#endif  // SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_
