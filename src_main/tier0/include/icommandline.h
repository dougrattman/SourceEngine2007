// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_ICOMMANDLINE_H_
#define SOURCE_TIER0_INCLUDE_ICOMMANDLINE_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/command_line_switches.h"
#include "tier0/include/platform.h"

// Purpose: Interface to command line.
abstract_class ICommandLine {
 public:
  // Creates comand line from windows one.
  virtual void CreateCmdLine(const ch *command_line) = 0;
  // Creates command line from POSIX one.
  virtual void CreateCmdLine(i32 argc, ch * *argv) = 0;
  // Gets constructed full command line.
  virtual const ch *GetCmdLine() const = 0;

  // Check whether a particular parameter exists.
  virtual const ch *CheckParm(const ch *param, const ch **param_value = nullptr)
      const = 0;
  // Removes param from command line.
  virtual void RemoveParm(const ch *param) = 0;
  // Removes param with value to command line.
  virtual void AppendParm(const ch *param, const ch *value) = 0;

  // Returns the argument after the one specified, or the default if not found.
  virtual const ch *ParmValue(
      const ch *param, const ch *default_param_value = nullptr) const = 0;
  // Returns the argument after the one specified, or the default if not found.
  virtual i32 ParmValue(const ch *param, i32 default_param_value) const = 0;
  // Returns the argument after the one specified, or the default if not found.
  virtual f32 ParmValue(const ch *param, f32 default_param_value) const = 0;

  // Gets params count.
  virtual usize ParmCount() const = 0;
  // Find param, or 0 if not found.
  virtual usize FindParm(const ch *param) const = 0;
  // Gets parameter at exact index.
  virtual const ch *GetParm(usize index) const = 0;
};

// Gets a singleton to the commandline interface
PLATFORM_INTERFACE ICommandLine *CommandLine_Tier0();

#define CommandLine CommandLine_Tier0

#endif  // SOURCE_TIER0_INCLUDE_ICOMMANDLINE_H_
