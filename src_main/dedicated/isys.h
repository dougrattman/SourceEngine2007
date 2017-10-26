// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_DEDICTED_ISYS_H_
#define SOURCE_DEDICTED_ISYS_H_

#include <cstddef>
#include "tier1/interface.h"

class CDedicatedAppSystemGroup;

abstract_class ISys {
 public:
  virtual ~ISys() {}

  virtual bool LoadModules(CDedicatedAppSystemGroup *
                           dedicated_app_system_group) = 0;

  virtual void Sleep(int milliseconds) = 0;
  virtual bool GetExecutableName(char *exe_name) = 0;
  virtual void ErrorMessage(int level, const char *message) = 0;

  virtual void WriteStatusText(char *status_text) = 0;
  virtual void UpdateStatus(int force) = 0;

  virtual uintptr_t LoadLibrary(char *library_path) = 0;
  virtual void FreeLibrary(uintptr_t library_handle) = 0;

  virtual bool CreateConsoleWindow() = 0;
  virtual void DestroyConsoleWindow() = 0;

  virtual void ConsoleOutput(char *message) = 0;
  virtual char *ConsoleInput() = 0;
  virtual void Printf(const char *format, ...) = 0;
};

extern ISys *sys;

#endif  // SOURCE_DEDICTED_ISYS_H_
