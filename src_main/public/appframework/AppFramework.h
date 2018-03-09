// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: An application framework.

#ifndef SOURCE_APPFRAMEWORK_APPFRAMEWORK_H_
#define SOURCE_APPFRAMEWORK_APPFRAMEWORK_H_

#include "base/include/base_types.h"

#include "appframework/iappsystemgroup.h"

// Gets the application instance.
void *GetAppInstance();

// Sets the application instance, should only be used if you're not calling
// AppMain.
void SetAppInstance(void *instance);

// Main entry point for the application.
i32 AppMain(void *instance, void *prev_instance, const ch *cmd_line,
            i32 cmd_show, CAppSystemGroup *app_system_group);
i32 AppMain(i32 argc, ch **argv, CAppSystemGroup *app_system_group);

// Used to startup/shutdown the application.
i32 AppStartup(void *instance, void *prev_instance, const ch *cmd_line,
               i32 cmd_show, CAppSystemGroup *pAppSystemGroup);
i32 AppStartup(i32 argc, ch **argv, CAppSystemGroup *app_system_group);
void AppShutdown(CAppSystemGroup *app_system_group);

// Macros to create singleton application objects for windowed + console apps.
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(_globalVarName) \
  i32 SOURCE_STDCALL WinMain(HINSTANCE instance, HINSTANCE prev_instance, \
                        LPSTR cmd_line, i32 cmd_show) {              \
    return AppMain(instance, prev_instance, cmd_line, cmd_show,      \
                   &_globalVarName);                                 \
  }

#define DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR(_globalVarName) \
  i32 main(i32 argc, ch **argv) { return AppMain(argc, argv, &_globalVarName); }

#define DEFINE_WINDOWED_APPLICATION_OBJECT(_className) \
  static _className __s_ApplicationObject;             \
  DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(__s_ApplicationObject)

#define DEFINE_CONSOLE_APPLICATION_OBJECT(_className) \
  static _className __s_ApplicationObject;            \
  DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR(__s_ApplicationObject)

// This class is a helper class used for steam-based applications. It loads up
// the file system in preparation for using it to load other required modules
// from steam.
class CSteamApplication : public CAppSystemGroup {
  using BaseClass = CAppSystemGroup;

 public:
  CSteamApplication(CSteamAppSystemGroup *steam_app_system_group);

  // Implementation of IAppSystemGroup.
  bool Create() override;
  bool PreInit() override;
  i32 Main() override;
  void PostShutdown() override;
  void Destroy() override;

  // Use this version in cases where you can't control the main loop and
  // expect to be ticked.
  i32 Startup() override;
  void Shutdown() override;

 protected:
  IFileSystem *m_pFileSystem;
  CSteamAppSystemGroup *m_pChildAppSystemGroup;
  bool m_bSteam;
};

// Macros to help create singleton application objects for windowed + console
// steam apps.
#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR(_className, \
                                                           _varName)   \
  static CSteamApplication __s_SteamApplicationObject(&_varName);      \
  DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(__s_SteamApplicationObject)

#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT(_className)                   \
  static _className __s_ApplicationObject;                                     \
  static CSteamApplication __s_SteamApplicationObject(&__s_ApplicationObject); \
  DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(__s_SteamApplicationObject)

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT_GLOBALVAR(_className, \
                                                          _varName)   \
  static CSteamApplication __s_SteamApplicationObject(&_varName);     \
  DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR(__s_SteamApplicationObject)

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT(_className)                    \
  static _className __s_ApplicationObject;                                     \
  static CSteamApplication __s_SteamApplicationObject(&__s_ApplicationObject); \
  DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR(__s_SteamApplicationObject)

#endif  // SOURCE_APPFRAMEWORK_APPFRAMEWORK_H_
