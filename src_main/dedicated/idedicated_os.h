// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_IDEDICATED_OS_H_
#define SOURCE_IDEDICATED_OS_H_

#include "base/include/base_types.h"
#include "tier1/interface.h"

class DedicatedSteamApp;
class IDedicatedServerAPI;

extern IDedicatedServerAPI *engine;

// Operating system abstraction.
the_interface IDedicatedOs {
 public:
  virtual ~IDedicatedOs() {}

  virtual bool LoadModules(DedicatedSteamApp * steam_app) = 0;

  virtual void Sleep(u32 milliseconds) const = 0;
  virtual bool GetExecutableName(ch * exe_name, usize exe_name_size) = 0;
  virtual void ErrorMessage(const ch *message) = 0;

  virtual bool WriteStatusText(ch * status_text) = 0;
  virtual void UpdateStatus(bool force) = 0;

  virtual uintptr_t LoadSharedLibrary(ch * library_path) = 0;
  virtual bool FreeSharedLibrary(uintptr_t library_handle) = 0;

  virtual bool CreateConsoleWindow() = 0;
  virtual bool DestroyConsoleWindow() = 0;

  virtual void ConsoleOutput(ch * message) = 0;
  virtual ch *ConsoleInput() = 0;
  virtual void Printf(const ch *format, ...) = 0;
};

IDedicatedOs *DedicatedOs();

#endif  // SOURCE_IDEDICATED_OS_H_
