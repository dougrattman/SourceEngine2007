// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Material editor.

#ifndef SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_
#define SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_

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
  int GetWindowWidth() const;
  int GetWindowHeight() const;

  // Sets up the game path.
  bool SetupSearchPaths(const char *start_dir, bool use_only_start_dir,
                        bool is_tool);

 private:
  // Returns the app name.
  virtual const char *GetAppName() = 0;
  virtual bool AppUsesReadPixels() { return false; }

  // Creates the app window.
  virtual void *CreateAppWindow(char const *title, bool is_windowed, int width,
                                int height);

  void *m_HWnd;
  int m_nWidth;
  int m_nHeight;
};

#endif  // SOURCE_APPFRAMEWORK_VGUIMATSYSAPP_H_
