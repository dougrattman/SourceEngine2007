// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: VCR mode records a client's game and allows you to
// play it back and reproduce it exactly. When playing it back, nothing
// is simulated on the server, but all server packets are recorded.
//
// Most of the VCR mode functionality is accomplished through hooks
// called at various points in the engine.

#ifndef SOURCE_TIER0_INCLUDE_VCRMODE_H_
#define SOURCE_TIER0_INCLUDE_VCRMODE_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#ifdef OS_WIN
#include <process.h>
#endif

#include "tier0/include/platform.h"
#include "tier0/include/vcr_shared.h"

#ifdef OS_POSIX
void BuildCmdLine(i32 argc, ch **argv);
ch *GetCommandLine();
#endif

// Enclose lines of code in this if you don't want anything in them written to
// or read from the VCR file.
#ifndef NO_VCR
#define NOVCR(x)      \
                      \
  {                   \
    VCRSetEnabled(0); \
    x;                \
    VCRSetEnabled(1); \
  }
#else
#define NOVCR(x) \
                 \
  { x; }
#endif

struct InputEvent_t;

enum VCRMode_t { VCR_Invalid = -1, VCR_Disabled = 0, VCR_Record, VCR_Playback };

abstract_class IVCRHelpers {
 public:
  virtual void ErrorMessage(const ch *pMsg) = 0;
  virtual void *GetMainWindow() = 0;
};

// Used by the vcrtrace program.
abstract_class IVCRTrace {
 public:
  virtual VCREvent ReadEvent() = 0;
  virtual void Read(void *pDest, i32 size) = 0;
};

typedef struct VCR_s {
  // Start VCR record or play.
  BOOL (*Start)(ch const *pFilename, bool bRecord, IVCRHelpers *pHelpers);
  void (*End)();

  // Used by the VCR trace app.
  IVCRTrace *(*GetVCRTraceInterface)();

  // Get the current mode the VCR is in.
  VCRMode_t (*GetMode)();

  // This can be used to block out areas of code that are unpredictable (like
  // things triggered by WM_TIMER messages). Note: this enables/disables VCR
  // mode usage on a PER-THREAD basis. The assumption is that you're marking out
  // specific sections of code that you don't want to use VCR mode inside of,
  // but you're not intending to stop all the other threads from using VCR mode.
  void (*SetEnabled)(i32 bEnabled);

  // This can be called any time to put in a debug check to make sure things are
  // synchronized.
  void (*SyncToken)(ch const *pToken);

  // Hook for Sys_FloatTime().
  f64 (*Hook_Sys_FloatTime)(f64 time);

  // Note: this makes no guarantees about msg.hwnd being the same on playback.
  // If it needs to be, then we need to add an ID system for Windows and store
  // the ID like in Goldsrc.
  i32 (*Hook_PeekMessage)(struct tagMSG *msg, void *hWnd, u32 wMsgFilterMin,
                          u32 wMsgFilterMax, u32 wRemoveMsg);

  // Call this to record game messages.
  void (*Hook_RecordGameMsg)(const InputEvent_t &event);
  void (*Hook_RecordEndGameMsg)();

  // Call this to playback game messages until it returns false.
  bool (*Hook_PlaybackGameMsg)(InputEvent_t *pEvent);

  // Hook for recvfrom() calls. This replaces the recvfrom() call.
  i32 (*Hook_recvfrom)(i32 s, ch *buf, i32 len, i32 flags,
                       struct sockaddr *from, i32 *fromlen);

  void (*Hook_GetCursorPos)(struct tagPOINT *pt);
  void (*Hook_ScreenToClient)(void *hWnd, struct tagPOINT *pt);

  void (*Hook_Cmd_Exec)(ch **f);

  ch *(*Hook_GetCommandLine)();

  // Registry hooks.
  long (*Hook_RegOpenKeyEx)(void *hKey, const ch *lpSubKey,
                            unsigned long ulOptions, unsigned long samDesired,
                            void *pHKey);
  long (*Hook_RegSetValueEx)(void *hKey, ch const *lpValueName,
                             unsigned long Reserved, unsigned long dwType,
                             u8 const *lpData, unsigned long cbData);
  long (*Hook_RegQueryValueEx)(void *hKey, ch const *lpValueName,
                               unsigned long *lpReserved, unsigned long *lpType,
                               u8 *lpData, unsigned long *lpcbData);
  long (*Hook_RegCreateKeyEx)(void *hKey, ch const *lpSubKey,
                              unsigned long Reserved, ch *lpClass,
                              unsigned long dwOptions, unsigned long samDesired,
                              void *lpSecurityAttributes, void *phkResult,
                              unsigned long *lpdwDisposition);
  void (*Hook_RegCloseKey)(void *hKey);

  // hInput is a HANDLE.
  i32 (*Hook_GetNumberOfConsoleInputEvents)(void *hInput,
                                            unsigned long *pNumEvents);

  // hInput is a HANDLE.
  // pRecs is an INPUT_RECORD pointer.
  i32 (*Hook_ReadConsoleInput)(void *hInput, void *pRecs, i32 nMaxRecs,
                               unsigned long *pNumRead);

  // This calls time() then gives you localtime()'s result.
  void (*Hook_LocalTime)(struct tm *today);

  short (*Hook_GetKeyState)(i32 nVirtKey);

  // TCP calls.
  i32 (*Hook_recv)(i32 s, ch *buf, i32 len, i32 flags);
  i32 (*Hook_send)(i32 s, const ch *buf, i32 len, i32 flags);

  // These can be used to add events without having to modify VCR mode.
  // pEventName is used for verification to make sure it's playing back
  // correctly. If pEventName is 0, then verification is not performed.
  void (*GenericRecord)(const ch *pEventName, const void *pData, i32 len);

  // Returns the number of bytes written in the generic event.
  // If bForceLenSame is true, then it will error out unless the value in the
  // VCR file is the same as maxLen.
  i32 (*GenericPlayback)(const ch *pEventName, void *pOutData, i32 maxLen,
                         bool bForceLenSame);

  // If you just want to record and playback a value and not worry about whether
  // or not you're recording or playing back, use this. It also will do nothing
  // if you're not recording or playing back.
  //
  // NOTE: also see GenericValueVerify, which allows you to have it VERIFY that
  // pData's contents are the same upon playback (rather than just copying
  // whatever is in the VCR file into pData).
  void (*GenericValue)(const ch *pEventName, void *pData, i32 maxLen);

  // Get the current percent (0.0 - 1.0) that it's played back through the file
  // (only valid in playback).
  f64 (*GetPercentCompleted)();

  // If you use this, then any VCR stuff the thread does will work with VCR
  // mode. This mirrors the Windows API CreateThread function and returns a
  // HANDLE the same way.
  void *(*Hook_CreateThread)(void *lpThreadAttributes,
                             unsigned long dwStackSize, void *lpStartAddress,
                             void *lpParameter, unsigned long dwCreationFlags,
                             unsigned long *lpThreadID);

  unsigned long (*Hook_WaitForSingleObject)(void *handle,
                                            unsigned long dwMilliseconds);

  void (*Hook_EnterCriticalSection)(void *pCS);

  void (*Hook_Time)(long *pTime);

  // String value. Playback just verifies that the incoming string is the same
  // as it was when recording.
  void (*GenericString)(const ch *pEventName, const ch *pString);

  // Works like GenericValue, except upon playback it will verify that pData's
  // contents are the same as it was during recording.
  void (*GenericValueVerify)(const ch *pEventName, const void *pData,
                             i32 maxLen);

  unsigned long (*Hook_WaitForMultipleObjects)(u32 handles_count,
                                               const void **handles,
                                               BOOL is_wait_all,
                                               u32 milliseconds);

} VCR_t;

#ifndef NO_VCR

// In the launcher, this is created by vcrmode.c.
// In the engine, this is set when the launcher initializes its DLL.
PLATFORM_INTERFACE VCR_t *g_pVCR;

#endif

#ifndef NO_VCR
#define VCRStart g_pVCR->Start
#define VCREnd g_pVCR->End
#define VCRGetVCRTraceInterface g_pVCR->GetVCRTraceInterface
#define VCRGetMode g_pVCR->GetMode
#define VCRSetEnabled g_pVCR->SetEnabled
#define VCRSyncToken g_pVCR->SyncToken
#define VCRGenericString g_pVCR->GenericString
#define VCRGenericValueVerify g_pVCR->GenericValueVerify
#define VCRHook_Sys_FloatTime g_pVCR->Hook_Sys_FloatTime
#define VCRHook_PeekMessage g_pVCR->Hook_PeekMessage
#define VCRHook_RecordGameMsg g_pVCR->Hook_RecordGameMsg
#define VCRHook_RecordEndGameMsg g_pVCR->Hook_RecordEndGameMsg
#define VCRHook_PlaybackGameMsg g_pVCR->Hook_PlaybackGameMsg
#define VCRHook_recvfrom g_pVCR->Hook_recvfrom
#define VCRHook_GetCursorPos g_pVCR->Hook_GetCursorPos
#define VCRHook_ScreenToClient g_pVCR->Hook_ScreenToClient
#define VCRHook_Cmd_Exec g_pVCR->Hook_Cmd_Exec
#define VCRHook_GetCommandLine g_pVCR->Hook_GetCommandLine
#define VCRHook_RegOpenKeyEx g_pVCR->Hook_RegOpenKeyEx
#define VCRHook_RegSetValueEx g_pVCR->Hook_RegSetValueEx
#define VCRHook_RegQueryValueEx g_pVCR->Hook_RegQueryValueEx
#define VCRHook_RegCreateKeyEx g_pVCR->Hook_RegCreateKeyEx
#define VCRHook_RegCloseKey g_pVCR->Hook_RegCloseKey
#define VCRHook_GetNumberOfConsoleInputEvents \
  g_pVCR->Hook_GetNumberOfConsoleInputEvents
#define VCRHook_ReadConsoleInput g_pVCR->Hook_ReadConsoleInput
#define VCRHook_LocalTime g_pVCR->Hook_LocalTime
#define VCRHook_GetKeyState g_pVCR->Hook_GetKeyState
#define VCRHook_recv g_pVCR->Hook_recv
#define VCRHook_send g_pVCR->Hook_send
#define VCRGenericRecord g_pVCR->GenericRecord
#define VCRGenericPlayback g_pVCR->GenericPlayback
#define VCRGenericValue g_pVCR->GenericValue
#define VCRGetPercentCompleted g_pVCR->GetPercentCompleted
#define VCRHook_CreateThread g_pVCR->Hook_CreateThread
#define VCRHook_WaitForSingleObject g_pVCR->Hook_WaitForSingleObject
#define VCRHook_EnterCriticalSection g_pVCR->Hook_EnterCriticalSection
#define VCRHook_Time g_pVCR->Hook_Time
#define VCRHook_WaitForMultipleObjects(a, b, c, d) \
  g_pVCR->Hook_WaitForMultipleObjects(a, (const void **)b, c, d)
#else
#define VCRStart(a, b, c) (1)
#define VCREnd ((void)(0))
#define VCRGetVCRTraceInterface (nullptr)
#define VCRGetMode() (VCR_Disabled)
#define VCRSetEnabled(a) ((void)(0))
#define VCRSyncToken(a) ((void)(0))
#define VCRGenericRecord MUST_IFDEF_OUT_GenericRecord
#define VCRGenericPlayback MUST_IFDEF_OUT_GenericPlayback
#define VCRGenericValue MUST_IFDEF_OUT_GenericValue
#define VCRGenericString MUST_IFDEF_OUT_GenericString
#define VCRGenericValueVerify MUST_IFDEF_OUT_GenericValueVerify
#define VCRGetPercentCompleted() (0.0f)
#define VCRHook_Sys_FloatTime Sys_FloatTime
#define VCRHook_PeekMessage PeekMessage
#define VCRHook_RecordGameMsg RecordGameMsg
#define VCRHook_RecordEndGameMsg RecordEndGameMsg
#define VCRHook_PlaybackGameMsg PlaybackGameMsg
#define VCRHook_recvfrom recvfrom
#define VCRHook_GetCursorPos GetCursorPos
#define VCRHook_ScreenToClient ScreenToClient
#define VCRHook_Cmd_Exec(a) ((void)(0))
#define VCRHook_GetCommandLine GetCommandLine
#define VCRHook_RegOpenKeyEx RegOpenKeyEx
#define VCRHook_RegSetValueEx RegSetValueEx
#define VCRHook_RegQueryValueEx RegQueryValueEx
#define VCRHook_RegCreateKeyEx RegCreateKeyEx
#define VCRHook_RegCloseKey RegCloseKey
#define VCRHook_GetNumberOfConsoleInputEvents GetNumberOfConsoleInputEvents
#define VCRHook_ReadConsoleInput ReadConsoleInput
#define VCRHook_LocalTime(a) memset(a, 0, sizeof(*a));
#define VCRHook_GetKeyState GetKeyState
#define VCRHook_recv recv
#define VCRHook_send send
#define VCRHook_CreateThread (void *)_beginthreadex
#define VCRHook_WaitForSingleObject WaitForSingleObject
#define VCRHook_EnterCriticalSection EnterCriticalSection
#define VCRHook_WaitForMultipleObjects(a, b, c, d) \
  WaitForMultipleObjects(a, (const HANDLE *)b, c, d)
#define VCRHook_Time Time
#endif

#endif  // SOURCE_TIER0_INCLUDE_VCRMODE_H_
