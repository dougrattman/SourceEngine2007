// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_VSTDLIB_IPROCESSUTILS_H_
#define SOURCE_VSTDLIB_IPROCESSUTILS_H_

#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "tier1/interface.h"

// Handle to a process
using ProcessHandle_t = i32;
enum {
  PROCESS_HANDLE_INVALID = 0,
};

#define PROCESS_UTILS_INTERFACE_VERSION "VProcessUtils001"

// Interface for makefiles to build differently depending on where they are run
// from.
abstract_class IProcessUtils : public IAppSystem {
 public:
  // Starts, stops a process
  virtual ProcessHandle_t StartProcess(const ch *pCommandLine,
                                       bool bConnectStdPipes) = 0;
  virtual ProcessHandle_t StartProcess(i32 argc, const ch **argv,
                                       bool bConnectStdPipes) = 0;
  virtual void CloseProcess(ProcessHandle_t hProcess) = 0;
  virtual void AbortProcess(ProcessHandle_t hProcess) = 0;

  // Returns true if a process is complete
  virtual bool IsProcessComplete(ProcessHandle_t hProcess) = 0;

  // Waits until a process is complete
  virtual void WaitUntilProcessCompletes(ProcessHandle_t hProcess) = 0;

  // Methods used to write input into a process
  virtual i32 SendProcessInput(ProcessHandle_t hProcess, ch * pBuf,
                               i32 nBufLen) = 0;

  // Methods used to read	output back from a process
  virtual i32 GetProcessOutputSize(ProcessHandle_t hProcess) = 0;
  virtual i32 GetProcessOutput(ProcessHandle_t hProcess, ch * pBuf,
                               i32 nBufLen) = 0;

  // Returns the exit code for the process. Doesn't work unless the process is
  // complete
  virtual i32 GetProcessExitCode(ProcessHandle_t hProcess) = 0;
};

#endif  // SOURCE_VSTDLIB_IPROCESSUTILS_H_
