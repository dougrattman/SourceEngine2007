// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines a group of app systems that all have the same lifetime that
// need to be connected/initialized, etc. in a well-defined order.

#ifndef SOURCE_APPFRAMEWORK_IAPPSYSTEMGROUP_H_
#define SOURCE_APPFRAMEWORK_IAPPSYSTEMGROUP_H_

#ifdef _WIN32
#pragma once
#endif

#include <type_traits>
#include "appframework/iappsystem.h"
#include "base/include/base_types.h"
#include "tier0/include/basetypes.h"
#include "tier1/interface.h"
#include "tier1/utldict.h"
#include "tier1/utlvector.h"

class IAppSystem;
class CSysModule;
class IBaseInterface;
class IFileSystem;

// Handle to a module.
using AppModule_t = i32;

enum { APP_MODULE_INVALID = static_cast<AppModule_t>(~0) };

// NOTE: The following methods must be implemented in your application although
// they can be empty implementations if you like...
the_interface IAppSystemGroup {
 public:
  // An installed application creation function, you should tell the group
  // the DLLs and the singleton interfaces you want to instantiate. Return false
  // if there's any problems and the app will abort.
  virtual bool Create() = 0;

  // Allow the application to do some work after AppSystems are connected but
  // they are all Initialized. Return false if there's any problems and the app
  // will abort.
  virtual bool PreInit() = 0;

  // Main loop implemented by the application.
  virtual i32 Main() = 0;

  // Allow the application to do some work after all AppSystems are shut down.
  virtual void PostShutdown() = 0;

  // Call an installed application destroy function, occurring after all modules
  // are unloaded.
  virtual void Destroy() = 0;
};

// Specifies a module + interface name for initialization.
struct AppSystemInfo_t {
  const ch *m_pModuleName;
  const ch *m_pInterfaceName;
};

// This class represents a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order.
class CAppSystemGroup : public IAppSystemGroup {
  friend void *AppSystemCreateInterfaceFn(const ch *interface_name,
                                          i32 *return_code);
  friend class CSteamAppSystemGroup;

 public:
  // Used to determine where we exited out from the system.
  enum AppSystemGroupStage_t {
    CREATION = 0,
    CONNECTION,
    PREINITIALIZATION,
    INITIALIZATION,
    SHUTDOWN,
    POSTSHUTDOWN,
    DISCONNECTION,
    DESTRUCTION,

    NONE,  // This means no error.
  };

 public:
  CAppSystemGroup(CAppSystemGroup *parent_app_system_group = nullptr);

  // Runs the app system group. First, modules are loaded, next they are
  // connected, followed by initialization Then Main() is run Then modules are
  // shut down, disconnected, and unloaded.
  i32 Run();

  // Use this version in cases where you can't control the main loop and
  // expect to be ticked.
  virtual i32 Startup();
  virtual void Shutdown();

  // Returns the stage at which the app system group ran into an error.
  AppSystemGroupStage_t GetErrorStage() const;

 protected:
  // These methods are meant to be called by derived classes of CAppSystemGroup.

  // Methods to load + unload modules.
  AppModule_t LoadModule(const ch *module_name);
  AppModule_t LoadModule(CreateInterfaceFn factory);

  // Method to add various global singleton systems.
  IAppSystem *AddSystem(AppModule_t module, const ch *interface_name);

  // Method to add various global singleton typed systems.
  template <typename T>
  T *AddSystem(AppModule_t module, const ch *interface_name) {
    static_assert(std::is_base_of<IAppSystem, T>::value,
                  "T should be IAppSystem");
    return static_cast<T *>(AddSystem(module, interface_name));
  }

  void AddSystem(IAppSystem *app_system, const ch *interface_name);

  // Simpler method of doing the LoadModule/AddSystem thing.
  // Make sure the last AppSystemInfo has a nullptr module name.
  bool AddSystems(AppSystemInfo_t *systems);

  // Method to look up a particular named system.
  void *FindSystem(const ch *interface_name);

  // Method to look up a particular named typed system.
  template <typename T>
  T *FindSystem(const ch *interface_name) {
    static_assert(std::is_base_of<IAppSystem, T>::value,
                  "T should be IAppSystem");
    return static_cast<T *>(FindSystem(interface_name));
  }

  // Gets at a class factory for the topmost appsystem group in an appsystem
  // stack.
  static CreateInterfaceFn GetFactory();

 private:
  i32 OnStartup();
  void OnShutdown();

  void UnloadAllModules();
  void RemoveAllSystems();

  // Method to connect/disconnect all systems.
  bool ConnectSystems();
  void DisconnectSystems();

  // Method to initialize/shutdown all systems.
  InitReturnVal_t InitSystems();
  void ShutdownSystems();

  // Gets at the parent appsystem group.
  CAppSystemGroup *GetParent();

  // Loads a module the standard way.
  virtual CSysModule *LoadModuleDLL(const ch *module_dll_name);

  void ReportStartupFailure(i32 error_stage, i32 system_index);

  struct Module_t {
    CSysModule *m_pModule;
    CreateInterfaceFn m_Factory;
    ch *m_pModuleName;
  };

  CUtlVector<Module_t> m_Modules;
  CUtlVector<IAppSystem *> m_Systems;
  CUtlDict<i32, u16> m_SystemDict;
  CAppSystemGroup *m_pParentAppSystem;
  AppSystemGroupStage_t m_nErrorStage;
};

// This class represents a group of app systems that are loaded through steam.
class CSteamAppSystemGroup : public CAppSystemGroup {
 public:
  CSteamAppSystemGroup(IFileSystem *file_system = nullptr,
                       CAppSystemGroup *parent_app_system = nullptr);

  // Used by CSteamApplication to set up necessary pointers if we can't do it in
  // the constructor.
  void Setup(IFileSystem *file_system, CAppSystemGroup *parent_app_system);

 protected:
  // Sets up the search paths.
  bool SetupSearchPaths(const ch *start_dir, bool use_only_start_dir,
                        bool is_tool);

  // Returns the game info path. Only works if you've called SetupSearchPaths
  // first.
  const ch *GetGameInfoPath() const;

 private:
  virtual CSysModule *LoadModuleDLL(const ch *module_dll_name);

  IFileSystem *m_pFileSystem;
  ch m_pGameInfoPath[SOURCE_MAX_PATH];
};

// Helper empty decorator implementation of an IAppSystemGroup.
template <class CBaseClass>
class CDefaultAppSystemGroup : public CBaseClass {
 public:
  virtual bool Create() { return true; }
  virtual bool PreInit() { return true; }
  virtual void PostShutdown() {}
  virtual void Destroy() {}
};

struct CFSSteamSetupInfo;

// Special helper for game info directory suggestion.
// SuggestGameInfoDirFn_t.
//   Game info suggestion function.
//   Provided by the application to possibly detect the suggested game info
//   directory and initialize all the game-info-related systems appropriately.
// Parameters:
//   fs_steam_setup_info - steam file system setup information if available.
//   path_buffer - buffer to hold game info directory path on return.
//   buffer_length - length of the provided buffer to hold game info directory
//                   path.
//   should_bubble_dirs - should contain "true" on return to bubble the
// directories up searching for game info file.
// Return values:
//   Returns "true" if the game info directory path suggestion is available and
//   was successfully copied into the provided buffer.
//   Returns "false" otherwise, interpreted that no suggestion will be used.
using SuggestGameInfoDirFn_t =
    bool (*)(CFSSteamSetupInfo const *fs_steam_setup_info, ch *path_buffer,
             i32 buffer_length, bool *should_bubble_dirs);

// SetSuggestGameInfoDirFn.
// Installs the supplied game info directory suggestion function.
// Parameters:
//   suggest_game_info_dir_func - the new game info directory suggestion
//   function.
// Returns:
//   The previously installed suggestion function or nullptr if none was
//   installed before. This function never fails.
SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn(
    SuggestGameInfoDirFn_t suggest_game_info_dir_func);

#endif  // SOURCE_APPFRAMEWORK_IAPPSYSTEMGROUP_H_
