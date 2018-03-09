// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async
// system and Live functions by either providing a handle for the caller to
// check results or handling automatic cleanup of the async data when the caller
// doesn't care about the results.

#ifndef IXBOXSYSTEM_H
#define IXBOXSYSTEM_H

#include "xbox/xboxstubs.h"

typedef void *AsyncHandle_t;
typedef void *XboxHandle_t;

#ifdef OS_POSIX
typedef void *HANDLE;
#define ERROR_SUCCESS 0
#define ERROR_IO_PENDING 1
#define ERROR_IO_INCOMPLETE 2
#define ERROR_INSUFFICIENT_BUFFER 3
#endif

//-----------------------------------------------------------------------------
// Xbox system interface
//-----------------------------------------------------------------------------
the_interface IXboxSystem {
 public:
  virtual AsyncHandle_t CreateAsyncHandle(void) = 0;
  virtual void ReleaseAsyncHandle(AsyncHandle_t handle) = 0;
  virtual int GetOverlappedResult(AsyncHandle_t handle, uint32_t * pResultCode,
                                  bool bWait) = 0;
  virtual void CancelOverlappedOperation(AsyncHandle_t handle) = 0;

  // Save/Load
  virtual void GetModSaveContainerNames(const char *pchModName,
                                        const wchar_t **ppchDisplayName,
                                        const char **ppchName) = 0;
  virtual uint32_t GetContainerRemainingSpace(void) = 0;
  virtual bool DeviceCapacityAdequate(DWORD nStorageID,
                                      const char *pModName) = 0;
  virtual DWORD DiscoverUserData(DWORD nUserID, const char *pModName) = 0;

  // XUI
  virtual bool ShowDeviceSelector(bool bForce, uint32_t *pStorageID,
                                  AsyncHandle_t *pHandle) = 0;
  virtual void ShowSigninUI(uint32_t nPanes, uint32_t nFlags) = 0;

  // Rich Presence and Matchmaking
  virtual int UserSetContext(uint32_t nUserIdx, uint32_t nContextID,
                             uint32_t nContextValue, bool bAsync = true,
                             AsyncHandle_t *pHandle = NULL) = 0;
  virtual int UserSetProperty(uint32_t nUserIndex, uint32_t nPropertyId,
                              uint32_t nBytes, const void *pvValue,
                              bool bAsync = true,
                              AsyncHandle_t *pHandle = NULL) = 0;

  // Matchmaking
  virtual int CreateSession(uint32_t nFlags, uint32_t nUserIdx,
                            uint32_t nMaxPublicSlots, uint32_t nMaxPrivateSlots,
                            uint64_t * pNonce, void *pSessionInfo,
                            XboxHandle_t *pSessionHandle, bool bAsync,
                            AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual uint32_t DeleteSession(XboxHandle_t hSession, bool bAsync,
                                 AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual uint32_t SessionSearch(
      uint32_t nProcedureIndex, uint32_t nUserIndex, uint32_t nNumResults,
      uint32_t nNumUsers, uint32_t nNumProperties, uint32_t nNumContexts,
      XUSER_PROPERTY * pSearchProperties, XUSER_CONTEXT * pSearchContexts,
      uint32_t * pcbResultsBuffer,
      XSESSION_SEARCHRESULT_HEADER * pSearchResults, bool bAsync,
      AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual uint32_t SessionStart(XboxHandle_t hSession, uint32_t nFlags,
                                bool bAsync,
                                AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual uint32_t SessionEnd(XboxHandle_t hSession, bool bAsync,
                              AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionJoinLocal(XboxHandle_t hSession, uint32_t nUserCount,
                               const uint32_t *pUserIndexes,
                               const bool *pPrivateSlots, bool bAsync,
                               AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionJoinRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                const XUID *pXuids, const bool *pPrivateSlots,
                                bool bAsync,
                                AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionLeaveLocal(XboxHandle_t hSession, uint32_t nUserCount,
                                const uint32_t *pUserIndexes, bool bAsync,
                                AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionLeaveRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                 const XUID *pXuids, bool bAsync,
                                 AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionMigrate(XboxHandle_t hSession, uint32_t nUserIndex,
                             void *pSessionInfo, bool bAsync,
                             AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual int SessionArbitrationRegister(
      XboxHandle_t hSession, uint32_t nFlags, uint64_t nonce, uint32_t * pBytes,
      void *pBuffer, bool bAsync, AsyncHandle_t *pAsyncHandle = NULL) = 0;

  // Stats
  virtual int WriteStats(XboxHandle_t hSession, XUID xuid, uint32_t nViews,
                         void *pViews, bool bAsync,
                         AsyncHandle_t *pAsyncHandle = NULL) = 0;

  // Achievements
  virtual int EnumerateAchievements(
      uint32_t nUserIdx, uint64_t xuid, uint32_t nStartingIdx, uint32_t nCount,
      void *pBuffer, uint32_t nBufferBytes, bool bAsync = true,
      AsyncHandle_t *pAsyncHandle = NULL) = 0;
  virtual void AwardAchievement(uint32_t nUserIdx, uint32_t nAchievementId) = 0;

  virtual void FinishContainerWrites(void) = 0;
  virtual uint32_t GetContainerOpenResult(void) = 0;
  virtual uint32_t OpenContainers(void) = 0;
  virtual void CloseContainers(void) = 0;
};

#define XBOXSYSTEM_INTERFACE_VERSION "XboxSystemInterface001"

#endif  // IXBOXSYSTEM_H
