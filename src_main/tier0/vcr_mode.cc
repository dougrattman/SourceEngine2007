// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/vcrmode.h"

#ifdef OS_WIN
#include <winsock.h>
#endif

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "inputsystem/include/inputenums.h"  // FIXME: We totally have a bad tier dependency here
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

#ifndef NO_VCR

#define PvRealloc realloc
#define PvAlloc malloc

#define VCR_RuntimeAssert(x) VCR_RuntimeAssertFn(x, #x)

f64 g_flLastVCRFloatTimeValue;
bool g_bExpectingWindowProcCalls = false;

IVCRHelpers *g_pHelpers = nullptr;
FILE *g_pVCRFile = nullptr;

VCRMode_t g_VCRMode = VCR_Disabled;
// Stored temporarily between
// SetEnabled(0)/SetEnabled(1) blocks.
VCRMode_t g_OldVCRMode = VCR_Invalid;

i32 g_iCurEvent = 0;
// So it knows when we're done playing back.
isize g_CurFilePos = 0, g_FileLen = 0;

// Last VCR_ReadEvent() call.
VCREvent g_LastReadEvent = (VCREvent)-1;
// The thread index of the thread that g_LastReadEvent
// is intended for.
i32 g_LastEventThread;
i32 g_bVCREnabled = 0;

// These wrappers exist because for some reason thread-blocking functions nuke
// the last function on the call stack, so it's very hard to debug without these
// wrappers.
inline unsigned long Wrap_WaitForSingleObject(HANDLE handle,
                                              unsigned long milliseconds) {
  return WaitForSingleObject(handle, milliseconds);
}

inline unsigned long Wrap_WaitForMultipleObjects(unsigned long handles_count,
                                                 const HANDLE *handles,
                                                 BOOL is_wait_all,
                                                 unsigned long milliseconds) {
  return WaitForMultipleObjects(handles_count, handles, is_wait_all,
                                milliseconds);
}

_Acquires_lock_(*pSection) inline void Wrap_EnterCriticalSection(
    CRITICAL_SECTION *critical_section) {
  EnterCriticalSection(critical_section);
}

// Thread-safe debugging file output.
FILE *g_pDebugFile = 0;
CRITICAL_SECTION g_DebugFileCS;

struct CCSInit {
  CCSInit() { InitializeCriticalSection(&g_DebugFileCS); }
  ~CCSInit() { DeleteCriticalSection(&g_DebugFileCS); }
} g_DebugFileCS222;

void VCR_Debug(const ch *message, ...) {
  va_list marker;
  va_start(marker, message);

  EnterCriticalSection(&g_DebugFileCS);

  if (!g_pDebugFile) g_pDebugFile = fopen("c:\\vcrdebug.txt", "wt");

  if (g_pDebugFile) {
    vfprintf(g_pDebugFile, message, marker);
    fflush(g_pDebugFile);
  }

  LeaveCriticalSection(&g_DebugFileCS);

  va_end(marker);
}

// VCR threading support. It uses 2 methods to implement threading, depending
// on whether you're recording or not.
//
// If you're recording, it uses critical sections to control access to the
// events written into the file.
//
// During playback, every thread waits on a windows event handle. When a VCR
// event is done being read out, it peeks ahead and sees which thread should get
// the next VCR event and it wakes up that thread.

#define MAX_VCR_THREADS 512
struct CVCRThreadInfo {
  CVCRThreadInfo() : CVCRThreadInfo{0, nullptr, false} {}
  CVCRThreadInfo(unsigned long the_thread_id, HANDLE the_wait_event,
                 bool the_is_enabled)
      : thread_id{the_thread_id},
        wait_event{the_wait_event},
        is_enabled{the_is_enabled} {}

  // The Windows thread ID.
  unsigned long thread_id;
  // Used to get the signal that there is an event for
  // this thread.
  HANDLE wait_event;
  // By default, this is true, but it can be set to false to
  // temporarily disable a thread's VCR usage.
  bool is_enabled;
};
CVCRThreadInfo *g_pVCRThreads = nullptr;  // This gets allocated to
                                          // MAX_VCR_THREADS size if we're doing
                                          // any VCR recording or playback.
u16 g_nVCRThreads = 0;

// Used to avoid writing the thread ID into events that are for the main thread.
unsigned long g_VCRMainThreadID = 0;

// Set to true if VCR_Start is ever called.
bool g_bVCRStartCalled = false;

u16 GetCurrentVCRThreadIndex() {
  const unsigned long tid = GetCurrentThreadId();

  for (u16 i = 0; i < g_nVCRThreads; i++) {
    if (g_pVCRThreads[i].thread_id == tid) return i;
  }

  Error("GetCurrentVCRThreadInfo: no matching thread.");
  return 0;
}

CVCRThreadInfo *GetCurrentVCRThreadInfo() {
  return &g_pVCRThreads[GetCurrentVCRThreadIndex()];
}

static void VCR_SignalNextEvent();

// This manages which thread gets the next event.
CRITICAL_SECTION g_VCRCriticalSection;

class CVCRThreadSafe {
 public:
  _Acquires_lock_(g_VCRCriticalSection) CVCRThreadSafe() {
    m_bSignalledNextEvent = false;

    if (g_VCRMode == VCR_Record) {
      Wrap_EnterCriticalSection(&g_VCRCriticalSection);
    } else if (g_VCRMode == VCR_Playback) {
      // Wait until our event is signalled, telling us that we are the next guy
      // in line for an event.
      WaitForSingleObject(GetCurrentVCRThreadInfo()->wait_event, INFINITE);
    }
  }
  _Releases_lock_(g_VCRCriticalSection) ~CVCRThreadSafe() {
    if (g_VCRMode == VCR_Record) {
      LeaveCriticalSection(&g_VCRCriticalSection);
    } else if (g_VCRMode == VCR_Playback && !m_bSignalledNextEvent) {
      // Set the event for the next thread's VCR event.
      VCR_SignalNextEvent();
    }
  }
  void SignalNextEvent() {
    VCR_SignalNextEvent();
    m_bSignalledNextEvent = true;
  }

 private:
  bool m_bSignalledNextEvent;
};

class CVCRThreadSafeInitter {
 public:
  CVCRThreadSafeInitter() { InitializeCriticalSection(&g_VCRCriticalSection); }
  ~CVCRThreadSafeInitter() { DeleteCriticalSection(&g_VCRCriticalSection); }
} g_VCRThreadSafeInitter;

#define VCR_THREADSAFE CVCRThreadSafe vcrThreadSafe;

// Internal functions.
static void VCR_Error(const ch *error, ...) {
#ifndef NDEBUG
  // Figure out which thread we're in, for the debugger.
  u32 current_thread_index = GetCurrentVCRThreadIndex();
  NOTE_UNUSED(current_thread_index);

  DebuggerBreak();
#endif

  ch error_message[256];
  va_list marker;
  va_start(marker, error);
  _snprintf_s(error_message, ARRAYSIZE(error_message), error, marker);
  va_end(marker);

  g_pHelpers->ErrorMessage(error_message);
  VCREnd();
}

static void VCR_RuntimeAssertFn(bool assert, ch const *message) {
  if (!assert) {
    VCR_Error("*** VCR ASSERT FAILED: %s ***\n", message);
  }
}

static void VCR_Read(void *pDest, usize size) {
  if (!g_pVCRFile) {
    memset(pDest, 0, size);
    return;
  }

  fread(pDest, 1, size, g_pVCRFile);

  g_CurFilePos += size;

  VCR_RuntimeAssert(g_CurFilePos <= g_FileLen);

  if (g_CurFilePos >= g_FileLen) {
    VCREnd();
  }
}

template <class T>
static void VCR_ReadVal(T &val) {
  VCR_Read(&val, sizeof(val));
}

static void VCR_Write(void const *pSrc, usize size) {
  fwrite(pSrc, 1, size, g_pVCRFile);
  fflush(g_pVCRFile);
}

template <class T>
static void VCR_WriteVal(T &val) {
  VCR_Write(&val, sizeof(val));
}

void VCR_SignalNextEvent() {
  // When this function is called, we know that we are the only thread that is
  // accessing the VCR file.
  u8 event;
  VCR_Read(&event, 1);

  // Verify that we're in the correct thread for this event.
  u16 threadID;
  if (event & 0x80) {
    VCR_ReadVal(threadID);
    event &= ~0x80;
  } else {
    threadID = 0;
  }

  // Must be a valid thread ID.
  if (threadID >= g_nVCRThreads) {
    Error("VCR_ReadEvent: invalid threadID (%d).", threadID);
  }

  // Now signal the next thread.
  g_LastReadEvent = (VCREvent)event;
  g_LastEventThread = threadID;
  SetEvent(g_pVCRThreads[threadID].wait_event);
}

static VCREvent VCR_ReadEvent() { return g_LastReadEvent; }

static void VCR_WriteEvent(VCREvent event) {
  u8 the_event = static_cast<u8>(event);
  const u16 tid = GetCurrentVCRThreadIndex();

  if (tid == 0) {
    VCR_Write(&the_event, sizeof(the_event));
  } else {
    the_event |= 0x80;
    VCR_Write(&the_event, sizeof(the_event));
    VCR_WriteVal(tid);
  }
}

static void VCR_IncrementEvent() { ++g_iCurEvent; }

static void VCR_Event(VCREvent type) {
  if (g_VCRMode == VCR_Disabled) return;

  VCR_IncrementEvent();

  if (g_VCRMode == VCR_Record) {
    VCR_WriteEvent(type);
  } else {
    VCREvent currentEvent = VCR_ReadEvent();
    VCR_RuntimeAssert(currentEvent == type);
  }
}

// VCR trace interface.
class CVCRTrace : public IVCRTrace {
 public:
  virtual VCREvent ReadEvent() { return VCR_ReadEvent(); }

  virtual void Read(void *pDest, i32 size) { VCR_Read(pDest, size); }
};

static CVCRTrace g_VCRTrace;

static BOOL VCR_StartWrite(const ch *vcr_file_path) {
  const ch *command_line = GetCommandLine();
  if (!strstr(command_line, "-nosound"))
    Error("VCR record: must use -nosound.");

  g_pVCRFile = fopen(vcr_file_path, "wb");
  if (g_pVCRFile) {
    // Write the version.
    u32 version = VCRFILE_VERSION;
    VCR_Write(&version, sizeof(version));

    g_VCRMode = VCR_Record;
    return TRUE;
  }

  return FALSE;
}

static BOOL VCR_StartRead(const ch *vcr_file_path) {
  g_pVCRFile = fopen(vcr_file_path, "rb");
  if (g_pVCRFile) {
    // Get the file length.
    fseek(g_pVCRFile, 0, SEEK_END);
    g_FileLen = ftell(g_pVCRFile);
    fseek(g_pVCRFile, 0, SEEK_SET);
    g_CurFilePos = 0;

    u32 version;
    // Verify the file version.
    VCR_Read(&version, sizeof(version));
    if (version != VCRFILE_VERSION) {
      assert(!"VCR_Start: invalid file version");
      VCREnd();
      return FALSE;
    }

    g_VCRMode = VCR_Playback;
    // Signal the first thread for its event.
    VCR_SignalNextEvent();
    return TRUE;
  }

  return FALSE;
}

// VCR interface.
static BOOL VCR_Start(ch const *vcr_file_path, bool should_record,
                      IVCRHelpers *vcr_helpers) {
  g_VCRMainThreadID = GetCurrentThreadId();
  g_bVCRStartCalled = true;

  // Setup the initial VCR thread list.
  g_pVCRThreads = new CVCRThreadInfo[MAX_VCR_THREADS];
  g_pVCRThreads[0] = CVCRThreadInfo{
      GetCurrentThreadId(), CreateEventW(nullptr, FALSE, FALSE, nullptr), true};
  g_nVCRThreads = 1;

  g_pHelpers = vcr_helpers;

  VCREnd();

  g_OldVCRMode = VCR_Invalid;

  return should_record ? VCR_StartWrite(vcr_file_path)
                       : VCR_StartRead(vcr_file_path);
}

static void VCR_End() {
  if (g_pVCRFile) {
    fclose(g_pVCRFile);
    g_pVCRFile = nullptr;
  }

  if (g_VCRMode == VCR_Playback) {
    DebuggerBreakIfDebugging();

    // It's going to get screwy now, especially if we have threads, so just
    // exit.
    TerminateProcess(GetCurrentProcess(), 1);
  }

  g_VCRMode = VCR_Disabled;
}

static IVCRTrace *VCR_GetVCRTraceInterface() { return &g_VCRTrace; }

static VCRMode_t VCR_GetMode() { return g_VCRMode; }

static void VCR_SetEnabled(i32 bEnabled) {
  if (g_VCRMode != VCR_Disabled) {
    g_pVCRThreads[GetCurrentVCRThreadIndex()].is_enabled = (bEnabled != 0);
  }
}

inline bool IsVCRModeEnabledForThisThread() {
  if (g_VCRMode == VCR_Disabled || !g_bVCRStartCalled) return false;

  return g_pVCRThreads[GetCurrentVCRThreadIndex()].is_enabled;
}

static void VCR_SyncToken(ch const *pToken) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  u8 len;

  VCR_THREADSAFE;
  VCR_Event(VCREvent_SyncToken);

  if (g_VCRMode == VCR_Record) {
    usize intLen = strlen(pToken);
    assert(intLen <= 255);

    len = static_cast<u8>(intLen);

    VCR_Write(&len, 1);
    VCR_Write(pToken, len);
  } else if (g_VCRMode == VCR_Playback) {
    ch test[256];

    VCR_Read(&len, 1);
    VCR_Read(test, len);

    VCR_RuntimeAssert(len == static_cast<u8>(strlen(pToken)));
    VCR_RuntimeAssert(memcmp(pToken, test, len) == 0);
  }
}

static f64 VCR_Hook_Sys_FloatTime(f64 time) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return time;

  VCR_THREADSAFE;
  VCR_Event(VCREvent_Sys_FloatTime);

  if (g_VCRMode == VCR_Record) {
    VCR_Write(&time, sizeof(time));
  } else if (g_VCRMode == VCR_Playback) {
    VCR_Read(&time, sizeof(time));
    g_flLastVCRFloatTimeValue = time;
  }

  return time;
}

static i32 VCR_Hook_PeekMessage(struct tagMSG *msg, void *hWnd,
                                u32 wMsgFilterMin, u32 wMsgFilterMax,
                                u32 wRemoveMsg) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return PeekMessage((MSG *)msg, (HWND)hWnd, wMsgFilterMin, wMsgFilterMax,
                       wRemoveMsg);

  VCR_THREADSAFE;

  if (g_VCRMode == VCR_Record) {
    // The trapped windowproc calls should be flushed by the time we get here.
    i32 ret;
    ret = PeekMessage((MSG *)msg, (HWND)hWnd, wMsgFilterMin, wMsgFilterMax,
                      wRemoveMsg);

    // NOTE: this must stay AFTER the trapped window proc calls or things get
    // read back in the wrong order.
    VCR_Event(VCREvent_PeekMessage);

    VCR_WriteVal(ret);
    if (ret) VCR_Write(msg, sizeof(MSG));

    return ret;
  } else {
    Assert(g_VCRMode == VCR_Playback);

    // Playback any windows messages that got trapped.
    VCR_Event(VCREvent_PeekMessage);

    i32 ret;
    VCR_ReadVal(ret);
    if (ret) VCR_Read(msg, sizeof(MSG));

    return ret;
  }
}

void VCR_Hook_RecordGameMsg(const InputEvent_t &event) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;

  if (g_VCRMode == VCR_Record) {
    VCR_Event(VCREvent_GameMsg);

    ch val = 1;
    VCR_WriteVal(val);
    VCR_WriteVal(event.m_nType);
    VCR_WriteVal(event.m_nData);
    VCR_WriteVal(event.m_nData2);
    VCR_WriteVal(event.m_nData3);
  }
}

void VCR_Hook_RecordEndGameMsg() {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;

  if (g_VCRMode == VCR_Record) {
    VCR_Event(VCREvent_GameMsg);
    ch val = 0;
    VCR_WriteVal(val);  // record that there are no more messages.
  }
}

bool VCR_Hook_PlaybackGameMsg(InputEvent_t *pEvent) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return false;

  VCR_THREADSAFE;

  if (g_VCRMode == VCR_Playback) {
    VCR_Event(VCREvent_GameMsg);

    ch bMsg;
    VCR_ReadVal(bMsg);
    if (bMsg) {
      VCR_ReadVal(pEvent->m_nType);
      VCR_ReadVal(pEvent->m_nData);
      VCR_ReadVal(pEvent->m_nData2);
      VCR_ReadVal(pEvent->m_nData3);
      return true;
    }
  }

  return false;
}

static void VCR_Hook_GetCursorPos(struct tagPOINT *pt) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    GetCursorPos(pt);
    return;
  }

  VCR_THREADSAFE;

  VCR_Event(VCREvent_GetCursorPos);

  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(*pt);
  } else {
    GetCursorPos(pt);

    if (g_VCRMode == VCR_Record) {
      VCR_WriteVal(*pt);
    }
  }
}

static void VCR_Hook_ScreenToClient(void *hWnd, struct tagPOINT *pt) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    ScreenToClient((HWND)hWnd, pt);
    return;
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_ScreenToClient);

  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(*pt);
  } else {
    ScreenToClient((HWND)hWnd, pt);

    if (g_VCRMode == VCR_Record) {
      VCR_WriteVal(*pt);
    }
  }
}

static i32 VCR_Hook_recvfrom(i32 s, ch *buf, i32 len, i32 flags,
                             struct sockaddr *from, i32 *fromlen) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    return recvfrom((SOCKET)s, buf, len, flags, from, fromlen);
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_recvfrom);

  i32 ret;
  if (g_VCRMode == VCR_Playback) {
    // Get the result from our file.
    VCR_Read(&ret, sizeof(ret));
    if (ret == SOCKET_ERROR) {
      i32 err;
      VCR_ReadVal(err);
      WSASetLastError(err);
    } else {
      VCR_Read(buf, ret);

      ch bFrom;
      VCR_ReadVal(bFrom);
      if (bFrom) {
        VCR_Read(from, *fromlen);
      }
    }
  } else {
    ret = recvfrom((SOCKET)s, buf, len, flags, from, fromlen);

    if (g_VCRMode == VCR_Record) {
      // Record the result.
      VCR_Write(&ret, sizeof(ret));
      if (ret == SOCKET_ERROR) {
        i32 err = WSAGetLastError();
        VCR_WriteVal(err);
      } else {
        VCR_Write(buf, ret);

        ch bFrom = !!from;
        VCR_WriteVal(bFrom);
        if (bFrom) VCR_Write(from, *fromlen);
      }
    }
  }

  return ret;
}

static i32 VCR_Hook_recv(i32 s, ch *buf, i32 len, i32 flags) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    return recv((SOCKET)s, buf, len, flags);
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_recv);

  i32 ret;
  if (g_VCRMode == VCR_Playback) {
    // Get the result from our file.
    VCR_Read(&ret, sizeof(ret));
    if (ret == SOCKET_ERROR) {
      i32 err;
      VCR_ReadVal(err);
      WSASetLastError(err);
    } else {
      VCR_Read(buf, ret);
    }
  } else {
    ret = recv((SOCKET)s, buf, len, flags);

    if (g_VCRMode == VCR_Record) {
      // Record the result.
      VCR_Write(&ret, sizeof(ret));
      if (ret == SOCKET_ERROR) {
        i32 err = WSAGetLastError();
        VCR_WriteVal(err);
      } else {
        VCR_Write(buf, ret);
      }
    }
  }

  return ret;
}

static i32 VCR_Hook_send(i32 s, const ch *buf, i32 len, i32 flags) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    return send((SOCKET)s, buf, len, flags);
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_send);

  i32 ret;
  if (g_VCRMode == VCR_Playback) {
    // Get the result from our file.
    VCR_Read(&ret, sizeof(ret));
    if (ret == SOCKET_ERROR) {
      i32 err;
      VCR_ReadVal(err);
      WSASetLastError(err);
    }
  } else {
    ret = send((SOCKET)s, buf, len, flags);

    if (g_VCRMode == VCR_Record) {
      // Record the result.
      VCR_Write(&ret, sizeof(ret));
      if (ret == SOCKET_ERROR) {
        i32 err = WSAGetLastError();
        VCR_WriteVal(err);
      }
    }
  }

  return ret;
}

static void VCR_Hook_Cmd_Exec(ch **f) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;
  VCR_Event(VCREvent_Cmd_Exec);

  if (g_VCRMode == VCR_Playback) {
    i32 len;

    VCR_Read(&len, sizeof(len));
    if (len == -1) {
      *f = nullptr;
    } else {
      *f = (ch *)PvAlloc(len);
      VCR_Read(*f, len);
    }
  } else if (g_VCRMode == VCR_Record) {
    i32 len;
    ch *str = *f;

    if (str) {
      len = (i32)strlen(str) + 1;
      VCR_Write(&len, sizeof(len));
      VCR_Write(str, len);
    } else {
      len = -1;
      VCR_Write(&len, sizeof(len));
    }
  }
}

static ch *VCR_Hook_GetCommandLine() {
  // This function is special in that it can be called before VCR mode is
  // initialized. In this special case, just return the command line.
  if (!g_pVCRThreads) return GetCommandLine();

  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return GetCommandLine();

  VCR_THREADSAFE;
  VCR_Event(VCREvent_CmdLine);

  i32 len;
  ch *ret;

  if (g_VCRMode == VCR_Playback) {
    VCR_Read(&len, sizeof(len));
    ret = new ch[len];
    VCR_Read(ret, len);
  } else {
    ret = GetCommandLine();

    if (g_VCRMode == VCR_Record) {
      len = (i32)strlen(ret) + 1;
      VCR_WriteVal(len);
      VCR_Write(ret, len);
    }
  }

  return ret;
}

static long VCR_Hook_RegOpenKeyEx(void *hKey, const ch *lpSubKey,
                                  unsigned long ulOptions,
                                  unsigned long samDesired, void *pHKey) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return RegOpenKeyEx((HKEY)hKey, lpSubKey, ulOptions, samDesired,
                        (PHKEY)pHKey);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_RegOpenKeyEx);

  long ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);  // (don't actually write anything to the person's
                       // registry when playing back).
  } else {
    ret =
        RegOpenKeyEx((HKEY)hKey, lpSubKey, ulOptions, samDesired, (PHKEY)pHKey);

    if (g_VCRMode == VCR_Record) VCR_WriteVal(ret);
  }

  return ret;
}

static long VCR_Hook_RegSetValueEx(void *hKey, ch const *lpValueName,
                                   unsigned long Reserved, unsigned long dwType,
                                   u8 const *lpData, unsigned long cbData) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return RegSetValueEx((HKEY)hKey, lpValueName, 0, dwType, lpData, cbData);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_RegSetValueEx);

  long ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);  // (don't actually write anything to the person's
                       // registry when playing back).
  } else {
    ret = RegSetValueEx((HKEY)hKey, lpValueName, 0, dwType, lpData, cbData);

    if (g_VCRMode == VCR_Record) VCR_WriteVal(ret);
  }

  return ret;
}

static long VCR_Hook_RegQueryValueEx(void *hKey, ch const *lpValueName,
                                     unsigned long *lpReserved,
                                     unsigned long *lpType, u8 *lpData,
                                     unsigned long *lpcbData) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return RegQueryValueEx((HKEY)hKey, lpValueName, lpReserved, lpType, lpData,
                           lpcbData);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_RegQueryValueEx);

  // Doesn't support this being 0 right now (although it would be trivial to
  // add support).
  assert(lpData);

  long ret;
  unsigned long dummy = 0;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);
    VCR_ReadVal(lpType ? *lpType : dummy);
    VCR_ReadVal(*lpcbData);
    VCR_Read(lpData, *lpcbData);
  } else {
    ret = RegQueryValueEx((HKEY)hKey, lpValueName, lpReserved, lpType, lpData,
                          lpcbData);

    if (g_VCRMode == VCR_Record) {
      VCR_WriteVal(ret);
      VCR_WriteVal(lpType ? *lpType : dummy);
      VCR_WriteVal(*lpcbData);
      VCR_Write(lpData, *lpcbData);
    }
  }

  return ret;
}

static long VCR_Hook_RegCreateKeyEx(void *hKey, ch const *lpSubKey,
                                    unsigned long, ch *lpClass,
                                    unsigned long dwOptions,
                                    unsigned long samDesired,
                                    void *lpSecurityAttributes, void *phkResult,
                                    unsigned long *lpdwDisposition) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return RegCreateKeyEx((HKEY)hKey, lpSubKey, 0, lpClass, dwOptions,
                          samDesired,
                          (LPSECURITY_ATTRIBUTES)lpSecurityAttributes,
                          (HKEY *)phkResult, lpdwDisposition);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_RegCreateKeyEx);

  long ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);  // (don't actually write anything to the person's
                       // registry when playing back).
  } else {
    ret =
        RegCreateKeyEx((HKEY)hKey, lpSubKey, 0, lpClass, dwOptions, samDesired,
                       (LPSECURITY_ATTRIBUTES)lpSecurityAttributes,
                       (HKEY *)phkResult, lpdwDisposition);

    if (g_VCRMode == VCR_Record) VCR_WriteVal(ret);
  }

  return ret;
}

static void VCR_Hook_RegCloseKey(void *hKey) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    RegCloseKey((HKEY)hKey);
    return;
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_RegCloseKey);

  if (g_VCRMode == VCR_Playback) {
  } else {
    RegCloseKey((HKEY)hKey);
  }
}

i32 VCR_Hook_GetNumberOfConsoleInputEvents(void *hInput,
                                           unsigned long *pNumEvents) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return GetNumberOfConsoleInputEvents((HANDLE)hInput, pNumEvents);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_GetNumberOfConsoleInputEvents);

  ch ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);
    VCR_ReadVal(*pNumEvents);
  } else {
    ret = (ch)GetNumberOfConsoleInputEvents((HANDLE)hInput, pNumEvents);

    if (g_VCRMode == VCR_Record) {
      VCR_WriteVal(ret);
      VCR_WriteVal(*pNumEvents);
    }
  }

  return ret;
}

i32 VCR_Hook_ReadConsoleInput(void *hInput, void *pRecs, i32 nMaxRecs,
                              unsigned long *pNumRead) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return ReadConsoleInput((HANDLE)hInput, (INPUT_RECORD *)pRecs, nMaxRecs,
                            pNumRead);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_ReadConsoleInput);

  ch ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);
    if (ret) {
      VCR_ReadVal(*pNumRead);
      VCR_Read(pRecs, *pNumRead * sizeof(INPUT_RECORD));
    }
  } else {
    ret = (ch)ReadConsoleInput((HANDLE)hInput, (INPUT_RECORD *)pRecs, nMaxRecs,
                               pNumRead);

    if (g_VCRMode == VCR_Record) {
      VCR_WriteVal(ret);
      if (ret) {
        VCR_WriteVal(*pNumRead);
        VCR_Write(pRecs, *pNumRead * sizeof(INPUT_RECORD));
      }
    }
  }

  return ret;
}

void VCR_Hook_LocalTime(struct tm *today) {
  // We just provide a wrapper on this function so we can protect access to
  // time() everywhere.
  time_t ltime;
  time(&ltime);
  tm *pTime = localtime(&ltime);
  memcpy(today, pTime, sizeof(*today));

  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;

  VCR_Event(VCREvent_LocalTime);
  if (g_VCRMode == VCR_Playback) {
    VCR_Read(today, sizeof(*today));
  } else if (g_VCRMode == VCR_Record) {
    VCR_Write(today, sizeof(*today));
  }
}

void VCR_Hook_Time(long *today) {
  // We just provide a wrapper on this function so we can protect access to
  // time() everywhere. NOTE: For 64-bit systems we should eventually get a
  // function that takes a time_t, but we should have
  //       until about 2038 to do that before we overflow a long.
  time_t curTime;
  time(&curTime);

  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    *today = (long)curTime;
    return;
  }

  VCR_THREADSAFE;

  VCR_Event(VCREvent_Time);
  if (g_VCRMode == VCR_Playback) {
    VCR_Read(&curTime, sizeof(curTime));
  } else if (g_VCRMode == VCR_Record) {
    VCR_Write(&curTime, sizeof(curTime));
  }

  *today = (long)curTime;
}

short VCR_Hook_GetKeyState(i32 nVirtKey) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return ::GetKeyState(nVirtKey);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_GetKeyState);

  short ret;
  if (g_VCRMode == VCR_Playback) {
    VCR_ReadVal(ret);
  } else {
    ret = ::GetKeyState(nVirtKey);
    if (g_VCRMode == VCR_Record) VCR_WriteVal(ret);
  }

  return ret;
}

void VCR_GenericRecord(const ch *pEventName, const void *pData, i32 len) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;
  VCR_Event(VCREvent_Generic);

  if (g_VCRMode != VCR_Record)
    Error("VCR_GenericRecord(%s): not recording a VCR file", pEventName);

  // Write the event name (or 255 if none).
  usize nameLen = 255;
  if (pEventName) {
    nameLen = strlen(pEventName) + 1;
    if (nameLen >= 255) {
      VCR_Error("VCR_GenericRecord(%s): nameLen too long (%zu)", pEventName,
                nameLen);
      return;
    }
  }

  u8 ucNameLen = (u8)nameLen;
  VCR_WriteVal(ucNameLen);

  if (pEventName) {
    VCR_Write(pEventName, ucNameLen);
  }

  // Write the data.
  VCR_WriteVal(len);
  VCR_Write(pData, len);
}

i32 VCR_GenericPlaybackInternal(const ch *pEventName, void *pOutData,
                                i32 maxLen, bool bForceSameLen,
                                bool bForceSameContents) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread() || g_VCRMode != VCR_Playback)
    Error("VCR_Playback( %s ): not playing back a VCR file", pEventName);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_Generic);

  u8 nameLen;
  VCR_ReadVal(nameLen);
  if (nameLen != 255) {
    ch testName[512];
    VCR_Read(testName, nameLen);
    if (strcmp(pEventName, testName) != 0) {
      VCR_Error("VCR_GenericPlayback( %s ) - event name does not match '%s'",
                pEventName, testName);
      return 0;
    }
  }

  i32 dataLen;
  VCR_ReadVal(dataLen);
  if (dataLen > maxLen) {
    VCR_Error(
        "VCR_GenericPlayback( %s ) - generic data too long (greater than "
        "maxLen: %d)",
        pEventName, maxLen);
    return 0;
  } else if (bForceSameLen && dataLen != maxLen) {
    VCR_Error(
        "VCR_GenericPlayback( %s ) - data size in file (%d) different than "
        "desired (%d)",
        pEventName, dataLen, maxLen);
    return 0;
  }

  if (bForceSameContents) {
    if (!bForceSameLen)
      Error("bForceSameContents and !bForceSameLen not allowed.");

    static ch *pTempData = new ch[dataLen];
    static i32 tempDataLen = dataLen;
    if (tempDataLen < dataLen) {
      delete[] pTempData;
      pTempData = new ch[dataLen];
      tempDataLen = dataLen;
    }

    VCR_Read(pTempData, dataLen);
    if (memcmp(pTempData, pOutData, dataLen) != 0) {
      VCR_Error("VCR_GenericPlayback: data doesn't match on playback.");
    }
  } else {
    VCR_Read(pOutData, dataLen);
  }

  return dataLen;
}

i32 VCR_GenericPlayback(const ch *pEventName, void *pOutData, i32 maxLen,
                        bool bForceSameLen) {
  return VCR_GenericPlaybackInternal(pEventName, pOutData, maxLen,
                                     bForceSameLen, false);
}

void VCR_GenericValue(const ch *pEventName, void *pData, i32 maxLen) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  if (!pEventName) pEventName = "";

  if (g_VCRMode == VCR_Record)
    VCR_GenericRecord(pEventName, pData, maxLen);
  else if (g_VCRMode == VCR_Playback)
    VCR_GenericPlaybackInternal(pEventName, pData, maxLen, true, false);
}

void VCR_GenericValueVerify(const ch *pEventName, const void *pData,
                            i32 maxLen) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  if (!pEventName) pEventName = "";

  if (g_VCRMode == VCR_Record)
    VCR_GenericRecord(pEventName, pData, maxLen);
  else if (g_VCRMode == VCR_Playback)
    VCR_GenericPlaybackInternal(pEventName, (void *)pData, maxLen, true, true);
}

void WriteShortString(const ch *pStr) {
  usize len = strlen(pStr) + 1;
  if (len >= 0xFFFF) {
    Error("VCR_WriteShortString, string too long (%d characters).", len);
  }

  u16 twobytes = (u16)len;
  VCR_WriteVal(twobytes);
  VCR_Write(pStr, len);
}

void ReadAndVerifyShortString(const ch *pStr) {
  usize len = strlen(pStr) + 1;

  u16 incomingSize;
  VCR_ReadVal(incomingSize);

  if (incomingSize != len)
    VCR_Error("ReadAndVerifyShortString (%s), lengths different.", pStr);

  static ch *pTempData = 0;
  static usize tempDataLen = 0;
  if (tempDataLen < len) {
    delete[] pTempData;
    pTempData = new ch[len];
    tempDataLen = len;
  }

  VCR_Read(pTempData, len);
  if (memcmp(pTempData, pStr, len) != 0) {
    VCR_Error("ReadAndVerifyShortString: strings different ('%s' vs '%s').",
              pStr, pTempData);
  }
}

void VCR_GenericRecordString(const ch *pEventName, const ch *pString) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  VCR_THREADSAFE;
  VCR_Event(VCREvent_GenericString);

  if (g_VCRMode != VCR_Record)
    Error("VCR_GenericRecordString( %s ): not recording a VCR file",
          pEventName);

  // Write the event name (or 255 if none).
  WriteShortString(pEventName);
  WriteShortString(pString);
}

void VCR_GenericPlaybackString(const ch *pEventName, const ch *pString) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread() || g_VCRMode != VCR_Playback)
    Error("VCR_GenericPlaybackString( %s ): not playing back a VCR file",
          pEventName);

  VCR_THREADSAFE;
  VCR_Event(VCREvent_GenericString);

  ReadAndVerifyShortString(pEventName);
  ReadAndVerifyShortString(pString);
}

void VCR_GenericString(const ch *pEventName, const ch *pString) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) return;

  if (!pEventName) pEventName = "";

  if (!pString) pString = "";

  if (g_VCRMode == VCR_Record)
    VCR_GenericRecordString(pEventName, pString);
  else if (g_VCRMode == VCR_Playback)
    VCR_GenericPlaybackString(pEventName, pString);
}

f64 VCR_GetPercentCompleted() {
  if (g_VCRMode == VCR_Playback) {
    return (f64)g_CurFilePos / g_FileLen;
  } else {
    return 0;
  }
}

void *VCR_CreateThread(void *lpThreadAttributes, unsigned long dwStackSize,
                       void *lpStartAddress, void *lpParameter,
                       unsigned long dwCreationFlags,
                       unsigned long *lpThreadID) {
  u32 dwThreadID = 0;

  // Use _beginthreadex because it sets up C runtime
  // correctly, and is safer than _beginthread. See MSDN.

  // Preamble.
  if (!IsVCRModeEnabledForThisThread()) {
    if (g_VCRMode == VCR_Disabled) {
      HANDLE hThread = (void *)_beginthreadex(
          (LPSECURITY_ATTRIBUTES)lpThreadAttributes, dwStackSize,
          (u32(__stdcall *)(void *))lpStartAddress, lpParameter,
          dwCreationFlags, &dwThreadID);

      if (lpThreadID) *lpThreadID = dwThreadID;

      return hThread;
    } else {
      Error("VCR_CreateThread: VCR mode disabled in calling thread.");
    }
  }

  // We could make this work without too much pain.
  if (GetCurrentThreadId() != g_VCRMainThreadID) {
    Error("VCR_CreateThread called outside main thread.");
  }

  if (g_nVCRThreads >= MAX_VCR_THREADS) {
    // This is easy to fix if we ever hit it.. just allow more threads.
    Error("VCR_CreateThread: g_nVCRThreads >= MAX_VCR_THREADS.");
  }

  // Write out the VCR event saying this thread is being created.
  VCR_THREADSAFE;
  VCR_Event(VCREvent_CreateThread);

  // Create the thread.
  HANDLE hThread = (void *)_beginthreadex(
      (LPSECURITY_ATTRIBUTES)lpThreadAttributes, dwStackSize,
      (u32(__stdcall *)(void *))lpStartAddress, lpParameter,
      dwCreationFlags | CREATE_SUSPENDED, &dwThreadID);

  if (lpThreadID) *lpThreadID = dwThreadID;

  if (!hThread) {
    // We don't handle this case in VCR mode (but we could pretty easily).
    if (g_VCRMode == VCR_Playback || g_VCRMode == VCR_Record)
      Error("VCR_CreateThread: CreateThread() failed.");

    return nullptr;
  }

  // Register this thread so we can write its ID into future VCR events.
  i32 iNewThread = g_nVCRThreads++;
  g_pVCRThreads[iNewThread].thread_id = dwThreadID;
  g_pVCRThreads[iNewThread].wait_event =
      CreateEvent(nullptr, false, false, nullptr);
  g_pVCRThreads[iNewThread].is_enabled = true;

  // Now resume the thread.
  if (!(dwCreationFlags & CREATE_SUSPENDED)) {
    ResumeThread(hThread);
  }

  return hThread;
}

unsigned long VCR_WaitForSingleObject(void *handle,
                                      unsigned long dwMilliseconds) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return Wrap_WaitForSingleObject(handle, dwMilliseconds);
  // Error( "VCR_WaitForSingleObject: VCR mode disabled in calling thread." );

  // We have to do the wait here BEFORE we acquire the VCR mutex, otherwise, we
  // could freeze the thread that's supposed to signal "handle".
  unsigned long ret = 0;
  if (g_VCRMode == VCR_Record) {
    ret = Wrap_WaitForSingleObject(handle, dwMilliseconds);
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_WaitForSingleObject);

  ch val = 1;
  if (g_VCRMode == VCR_Record) {
    if (ret == WAIT_ABANDONED)
      val = 2;
    else if (ret == WAIT_TIMEOUT)
      val = 3;

    VCR_WriteVal(val);
    return ret;
  } else {
    Assert(g_VCRMode == VCR_Playback);

    VCR_ReadVal(val);
    if (val == 1) {
      // Hack job.. let other threads start reading events now.. we're basically
      // saying here that we're finished reading our VCR event. If we didn't
      // pass the buck onto the next one, if the event hadn't already been
      // signalled, it might never get signalled.
      vcrThreadSafe.SignalNextEvent();

      // If it wrote 1, then we know that this call has to signal the object, so
      // just wait until it gets signalled.
      ret = Wrap_WaitForSingleObject(handle, INFINITE);
      if (ret == WAIT_ABANDONED || ret == WAIT_TIMEOUT) {
        Error("VCR_WaitForSingleObject: got inconsistent value on playback.");
      }

      return ret;
    } else {
      // Return whatever the function returned while it was recording.
      return (val == 2) ? WAIT_ABANDONED : WAIT_TIMEOUT;
    }
  }
}

unsigned long VCR_WaitForMultipleObjects(u32 handles_count,
                                         const void **handles, BOOL is_wait_all,
                                         u32 milliseconds) {
  // Preamble.
  if (!IsVCRModeEnabledForThisThread())
    return Wrap_WaitForMultipleObjects(handles_count,
                                       const_cast<const HANDLE *>(handles),
                                       is_wait_all, milliseconds);

  AssertMsg(0, "Need to implement VCR_WaitForMultipleObjects");
  return 0;
}

void VCR_EnterCriticalSection(void *cs) {
  CRITICAL_SECTION *critical_section = static_cast<CRITICAL_SECTION *>(cs);

  if (!IsVCRModeEnabledForThisThread()) {
    Wrap_EnterCriticalSection(critical_section);
    return;
  }

  // While recording, let's get the critical section first.
  if (g_VCRMode == VCR_Record) {
    Wrap_EnterCriticalSection(critical_section);
  }

  VCR_THREADSAFE;
  VCR_Event(VCREvent_EnterCriticalSection);

  if (g_VCRMode == VCR_Playback) {
    // When playing back, we want to grab the CS -after- the event has been read
    // out, because it means that we're the only thread that is at this spot
    // now. If we tried to grab the CS before calling VCR_Event, then it might
    // let the wrong thread have the CS on playback.
    Wrap_EnterCriticalSection(critical_section);
  }
}

// The global VCR interface.
VCR_t g_VCR = {
    VCR_Start,
    VCR_End,
    VCR_GetVCRTraceInterface,
    VCR_GetMode,
    VCR_SetEnabled,
    VCR_SyncToken,
    VCR_Hook_Sys_FloatTime,
    VCR_Hook_PeekMessage,
    VCR_Hook_RecordGameMsg,
    VCR_Hook_RecordEndGameMsg,
    VCR_Hook_PlaybackGameMsg,
    VCR_Hook_recvfrom,
    VCR_Hook_GetCursorPos,
    VCR_Hook_ScreenToClient,
    VCR_Hook_Cmd_Exec,
    VCR_Hook_GetCommandLine,
    VCR_Hook_RegOpenKeyEx,
    VCR_Hook_RegSetValueEx,
    VCR_Hook_RegQueryValueEx,
    VCR_Hook_RegCreateKeyEx,
    VCR_Hook_RegCloseKey,
    VCR_Hook_GetNumberOfConsoleInputEvents,
    VCR_Hook_ReadConsoleInput,
    VCR_Hook_LocalTime,
    VCR_Hook_GetKeyState,
    VCR_Hook_recv,
    VCR_Hook_send,
    VCR_GenericRecord,
    VCR_GenericPlayback,
    VCR_GenericValue,
    VCR_GetPercentCompleted,
    VCR_CreateThread,
    VCR_WaitForSingleObject,
    VCR_EnterCriticalSection,
    VCR_Hook_Time,
    VCR_GenericString,
    VCR_GenericValueVerify,
    VCR_WaitForMultipleObjects,
};

VCR_t *g_pVCR = &g_VCR;

#endif  // LINUX
