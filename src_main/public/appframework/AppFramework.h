// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: An application framework.

#ifndef SOURCE_APPFRAMEWORK_APPFRAMEWORK_H_
#define SOURCE_APPFRAMEWORK_APPFRAMEWORK_H_

#include "appframework/iappsystemgroup.h"

// Gets the application instance.
void *GetAppInstance();

// Sets the application instance, should only be used if you're not calling
// AppMain.
void SetAppInstance(void *instance);

// Main entry point for the application.
int AppMain(void *instance, void *prev_instance, const char *cmd_line,
            int cmd_show, CAppSystemGroup *app_system_group);
int AppMain(int argc, char **argv, CAppSystemGroup *app_system_group);

// Used to startup/shutdown the application.
int AppStartup(void *instance, void *prev_instance, const char *cmd_line,
               int cmd_show, CAppSystemGroup *pAppSystemGroup);
int AppStartup(int argc, char **argv, CAppSystemGroup *app_system_group);
void AppShutdown(CAppSystemGroup *app_system_group);

// Macros to create singleton application objects for windowed + console apps.
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(_globalVarName) \
  int __stdcall WinMain(HINSTANCE instance, HINSTANCE prev_instance, \
                        LPSTR cmd_line, int cmd_show) {              \
    return AppMain(instance, prev_instance, cmd_line, cmd_show,      \
                   &_globalVarName);                                 \
  }

#define DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR(_globalVarName) \
  int main(int argc, char **argv) {                                 \
    return AppMain(argc, argv, &_globalVarName);                    \
  }

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
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

  // Use this version in cases where you can't control the main loop and
  // expect to be ticked.
  int Startup() override;
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
