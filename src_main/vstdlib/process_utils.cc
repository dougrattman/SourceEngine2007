// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "vstdlib/iprocessutils.h"

#include "base/include/windows/windows_errno_info.h"
#include "base/include/windows/windows_light.h"

#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"

// At the moment, we can only run one process at a time
class CProcessUtils : public CTier1AppSystem<IProcessUtils> {
  using BaseClass = CTier1AppSystem<IProcessUtils>;

 public:
  CProcessUtils()
      : BaseClass{false},
        m_hCurrentProcess{PROCESS_HANDLE_INVALID},
        m_bInitialized{false} {}

  // Inherited from IAppSystem
  // Initialize, shutdown process system
  InitReturnVal_t Init() override {
    InitReturnVal_t nRetVal = BaseClass::Init();
    if (nRetVal != INIT_OK) return nRetVal;

    m_bInitialized = true;
    m_hCurrentProcess = PROCESS_HANDLE_INVALID;

    return INIT_OK;
  }

  void Shutdown() override {
    Assert(m_bInitialized);
    Assert(m_Processes.Count() == 0);

    if (m_Processes.Count() != 0) {
      AbortProcess(m_hCurrentProcess);
    }

    m_bInitialized = false;

    return BaseClass::Shutdown();
  }

  // Inherited from IProcessUtils
  // Options for compilation
  ProcessHandle_t StartProcess(const char *pCommandLine,
                               bool bConnectStdPipes) override {
    Assert(m_bInitialized);

    // NOTE: For the moment, we can only run one process at a time
    // although in the future, I expect to have a process queue.
    if (m_hCurrentProcess != PROCESS_HANDLE_INVALID) {
      WaitUntilProcessCompletes(m_hCurrentProcess);
    }

    ProcessInfo_t info;
    info.m_CommandLine = pCommandLine;

    if (!bConnectStdPipes) {
      info.m_hChildStderrWr = INVALID_HANDLE_VALUE;
      info.m_hChildStdinRd = info.m_hChildStdinWr = INVALID_HANDLE_VALUE;
      info.m_hChildStdoutRd = info.m_hChildStdoutWr = INVALID_HANDLE_VALUE;

      return CreateProcess(info, false);
    }

    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES)};
    // Set the bInheritHandle flag so pipe handles are inherited.
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // Create a pipe for the child's STDOUT.
    if (CreatePipe(&info.m_hChildStdoutRd, &info.m_hChildStdoutWr, &sa, 0)) {
      if (CreatePipe(&info.m_hChildStdinRd, &info.m_hChildStdinWr, &sa, 0)) {
        if (DuplicateHandle(GetCurrentProcess(), info.m_hChildStdoutWr,
                            GetCurrentProcess(), &info.m_hChildStderrWr, 0,
                            TRUE, DUPLICATE_SAME_ACCESS)) {
          ProcessHandle_t hProcess = CreateProcess(info, true);
          if (hProcess != PROCESS_HANDLE_INVALID) return hProcess;

          CloseHandle(info.m_hChildStderrWr);
        }
        CloseHandle(info.m_hChildStdinRd);
        CloseHandle(info.m_hChildStdinWr);
      }
      CloseHandle(info.m_hChildStdoutRd);
      CloseHandle(info.m_hChildStdoutWr);
    }
    return PROCESS_HANDLE_INVALID;
  }

  // Start up a process
  ProcessHandle_t StartProcess(int argc, const char **argv,
                               bool bConnectStdPipes) override {
    CUtlString commandLine;

    for (int i = 0; i < argc; ++i) {
      commandLine += argv[i];

      if (i != argc - 1) commandLine += " ";
    }

    return StartProcess(commandLine.Get(), bConnectStdPipes);
  }

  // Closes the process
  void CloseProcess(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);

    if (hProcess != PROCESS_HANDLE_INVALID) {
      WaitUntilProcessCompletes(hProcess);
      ShutdownProcess(hProcess);
    }
  }

  // Aborts the process
  void AbortProcess(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);

    if (hProcess != PROCESS_HANDLE_INVALID) {
      if (!IsProcessComplete(hProcess)) {
        ProcessInfo_t &info = m_Processes[hProcess];
        TerminateProcess(info.m_hProcess, 1);
      }

      ShutdownProcess(hProcess);
    }
  }

  // Returns true if the process is complete
  bool IsProcessComplete(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);
    Assert(hProcess != PROCESS_HANDLE_INVALID);

    if (m_hCurrentProcess != hProcess) return true;

    HANDLE h = m_Processes[hProcess].m_hProcess;
    return WaitForSingleObject(h, 0) != WAIT_TIMEOUT;
  }

  // Waits until a process is complete
  void WaitUntilProcessCompletes(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);

    // For the moment, we can only run one process at a time
    if ((hProcess == PROCESS_HANDLE_INVALID) || (m_hCurrentProcess != hProcess))
      return;

    ProcessInfo_t &info = m_Processes[hProcess];

    if (info.m_hChildStdoutRd == INVALID_HANDLE_VALUE) {
      WaitForSingleObject(info.m_hProcess, INFINITE);
    } else {
      // NOTE: The called process can block during writes to stderr + stdout
      // if the pipe buffer is empty. Therefore, waiting INFINITE is not
      // possible here. We must queue up messages received to allow the
      // process to continue
      while (WaitForSingleObject(info.m_hProcess, 100) == WAIT_TIMEOUT) {
        int nLen = GetActualProcessOutputSize(hProcess);
        if (nLen > 0) {
          int nPut = info.m_ProcessOutput.TellPut();
          info.m_ProcessOutput.EnsureCapacity(nPut + nLen);
          usize nBytesRead = GetActualProcessOutput(
              hProcess, (char *)info.m_ProcessOutput.PeekPut(), nLen);
          info.m_ProcessOutput.SeekPut(CUtlBuffer::SEEK_HEAD,
                                       nPut + nBytesRead);
        }
      }
    }

    m_hCurrentProcess = PROCESS_HANDLE_INVALID;
  }

  int SendProcessInput(ProcessHandle_t hProcess, char *pBuf,
                       int nBufLen) override {
    // Unimplemented yet
    Assert(0);
    return 0;
  }

  int GetProcessOutputSize(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);

    if (hProcess == PROCESS_HANDLE_INVALID) return 0;

    return GetActualProcessOutputSize(hProcess) +
           m_Processes[hProcess].m_ProcessOutput.TellPut();
  }

  int GetProcessOutput(ProcessHandle_t hProcess, char *pBuf,
                       int nBufLen) override {
    Assert(m_bInitialized);

    if (hProcess == PROCESS_HANDLE_INVALID) return 0;

    ProcessInfo_t &info = m_Processes[hProcess];
    int nCachedBytes = info.m_ProcessOutput.TellPut();
    usize nBytesRead = 0;

    if (nCachedBytes) {
      nBytesRead = std::min(nBufLen - 1, nCachedBytes);

      info.m_ProcessOutput.Get(pBuf, nBytesRead);
      pBuf[nBytesRead] = 0;

      nBufLen -= nBytesRead;
      pBuf += nBytesRead;

      if (info.m_ProcessOutput.GetBytesRemaining() == 0) {
        info.m_ProcessOutput.Purge();
      }

      if (nBufLen <= 1) return nBytesRead;
    }

    // Auto-nullptr terminate
    usize nActualCountRead = GetActualProcessOutput(hProcess, pBuf, nBufLen);
    pBuf[nActualCountRead] = 0;

    return nActualCountRead + nBytesRead + 1;
  }

  // Returns the exit code for the process. Doesn't work unless the process is
  // complete
  int GetProcessExitCode(ProcessHandle_t hProcess) override {
    Assert(m_bInitialized);

    ProcessInfo_t &info = m_Processes[hProcess];
    DWORD nExitCode;
    BOOL bOk = GetExitCodeProcess(info.m_hProcess, &nExitCode);

    if (!bOk || nExitCode == STILL_ACTIVE) return -1;

    return nExitCode;
  }

 private:
  struct ProcessInfo_t {
    HANDLE m_hChildStdinRd;
    HANDLE m_hChildStdinWr;
    HANDLE m_hChildStdoutRd;
    HANDLE m_hChildStdoutWr;
    HANDLE m_hChildStderrWr;
    HANDLE m_hProcess;
    CUtlString m_CommandLine;
    CUtlBuffer m_ProcessOutput;
  };

  // creates the process, adds it to the list and writes the windows HANDLE
  // into info.m_hProcess
  ProcessHandle_t CreateProcess(ProcessInfo_t &info, bool bConnectStdPipes) {
    STARTUPINFO si{sizeof(si)};
    if (bConnectStdPipes) {
      si.dwFlags = STARTF_USESTDHANDLES;
      si.hStdInput = info.m_hChildStdinRd;
      si.hStdError = info.m_hChildStderrWr;
      si.hStdOutput = info.m_hChildStdoutWr;
    }

    PROCESS_INFORMATION pi;
    if (::CreateProcess(nullptr, info.m_CommandLine.Get(), nullptr, nullptr,
                        TRUE, DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
      info.m_hProcess = pi.hProcess;
      m_hCurrentProcess = m_Processes.AddToTail(info);
      return m_hCurrentProcess;
    }

    Warning(
        "Could not execute the command:\n   %s\n"
        "Windows gave the error message:\n   \"%s\"\n",
        info.m_CommandLine.Get(),
        source::windows::windows_errno_info_last_error().description);

    return PROCESS_HANDLE_INVALID;
  }

  // Shuts down the process handle
  void ShutdownProcess(ProcessHandle_t hProcess) {
    ProcessInfo_t &info = m_Processes[hProcess];
    CloseHandle(info.m_hChildStderrWr);
    CloseHandle(info.m_hChildStdinRd);
    CloseHandle(info.m_hChildStdinWr);
    CloseHandle(info.m_hChildStdoutRd);
    CloseHandle(info.m_hChildStdoutWr);

    m_Processes.Remove(hProcess);
  }

  // Methods used to read	output back from a process
  int GetActualProcessOutputSize(ProcessHandle_t hProcess) {
    Assert(hProcess != PROCESS_HANDLE_INVALID);

    ProcessInfo_t &info = m_Processes[hProcess];
    if (info.m_hChildStdoutRd == INVALID_HANDLE_VALUE) return 0;

    DWORD dwCount = 0;
    if (!PeekNamedPipe(info.m_hChildStdoutRd, nullptr, 0, nullptr, &dwCount,
                       nullptr)) {
      Warning(
          "Could not read from pipe associated with command %s\n"
          "Windows gave the error message:\n   \"%s\"\n",
          info.m_CommandLine.Get(),
          source::windows::windows_errno_info_last_error().description);
      return 0;
    }

    // Add 1 for auto-nullptr termination
    return (dwCount > 0) ? (int)dwCount + 1 : 0;
  }

  usize GetActualProcessOutput(ProcessHandle_t handle, char *buffer,
                             usize buffer_size) {
    ProcessInfo_t &info = m_Processes[handle];
    if (info.m_hChildStdoutRd == INVALID_HANDLE_VALUE) return 0;

    DWORD dwCount = 0;

    // TODO(d.rattman): Is there a way of making pipes be text mode so we don't
    // get /n/rs back?
    char *temp_buffer{stack_alloc<ch>(buffer_size)};
    if (!PeekNamedPipe(info.m_hChildStdoutRd, nullptr, 0, nullptr, &dwCount,
                       nullptr)) {
      Warning(
          "ProcessUtils: Could not read from pipe associated with command "
          "'%s', %s\n",
          info.m_CommandLine.Get(),
          source::windows::windows_errno_info_last_error().description);
      return 0;
    }

    DWORD dwRead = 0;
    dwCount = std::min(dwCount, (DWORD)buffer_size - 1);
    ReadFile(info.m_hChildStdoutRd, temp_buffer, dwCount, &dwRead, nullptr);

    // Convert /n/r -> /n
    usize nActualCountRead = 0;
    for (u32 i = 0; i < dwRead; ++i) {
      char c = temp_buffer[i];
      if (c == '\r') {
        if ((i + 1 < dwRead) && (temp_buffer[i + 1] == '\n')) {
          buffer[nActualCountRead++] = '\n';
          ++i;
          continue;
        }
      }

      buffer[nActualCountRead++] = c;
    }

    return nActualCountRead;
  }

  CUtlFixedLinkedList<ProcessInfo_t> m_Processes;
  ProcessHandle_t m_hCurrentProcess;
  bool m_bInitialized;
};

static CProcessUtils s_ProcessUtils;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CProcessUtils, IProcessUtils,
                                  PROCESS_UTILS_INTERFACE_VERSION,
                                  s_ProcessUtils);