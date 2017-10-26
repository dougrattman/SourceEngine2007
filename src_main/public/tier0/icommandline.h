// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_ICOMMANDLINE_H_
#define SOURCE_TIER0_ICOMMANDLINE_H_

#include "tier0/command_line_switches.h"
#include "tier0/platform.h"

// Purpose: Interface to command line.
abstract_class ICommandLine {
 public:
  // Creates comand line from windows one.
  virtual void CreateCmdLine(const char *command_line) = 0;
  // Creates command line from POSIX one.
  virtual void CreateCmdLine(int argc, char **argv) = 0;
  // Gets constructed full command line.
  virtual const char *GetCmdLine() const = 0;

  // Check whether a particular parameter exists.
  virtual const char *CheckParm(const char *param,
                                const char **param_value = nullptr) const = 0;
  // Removes param from command line.
  virtual void RemoveParm(const char *param) = 0;
  // Removes param with value to command line.
  virtual void AppendParm(const char *param, const char *value) = 0;

  // Returns the argument after the one specified, or the default if not found.
  virtual const char *ParmValue(
      const char *param, const char *default_param_value = nullptr) const = 0;
  // Returns the argument after the one specified, or the default if not found.
  virtual int ParmValue(const char *param, int default_param_value) const = 0;
  // Returns the argument after the one specified, or the default if not found.
  virtual float ParmValue(const char *param, float default_param_value)
      const = 0;

  // Gets params count.
  virtual size_t ParmCount() const = 0;
  // Find param, or 0 if not found.
  virtual size_t FindParm(const char *param) const = 0;
  // Gets parameter at exact index.
  virtual const char *GetParm(size_t index) const = 0;
};

// Gets a singleton to the commandline interface
// NOTE: The #define trickery here is necessary for backwards compatibility
// since this interface used to lie in the vstdlib library.
PLATFORM_INTERFACE ICommandLine *CommandLine_Tier0();

#if !defined(VSTDLIB_BACKWARD_COMPAT)
#define CommandLine CommandLine_Tier0
#endif

#endif  // SOURCE_TIER0_ICOMMANDLINE_H_