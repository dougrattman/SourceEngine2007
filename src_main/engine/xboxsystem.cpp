// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Interface to Xbox 360 system functions. Helps deal with the async
// system and Live functions by either providing a handle for the caller to
// check results or handling automatic cleanup of the async data
// when the caller doesn't care about the results.

#include "ixboxsystem.h"

#include "host.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"

#include "tier0/include/memdbgon.h"

static wchar_t
    g_szModSaveContainerDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH] = L"";
static char g_szModSaveContainerName[XCONTENT_MAX_FILENAME_LENGTH] = "";

//-----------------------------------------------------------------------------
// Implementation of IXboxSystem interface
//-----------------------------------------------------------------------------
class CXboxSystem : public IXboxSystem {
 public:
  CXboxSystem(void);

  virtual ~CXboxSystem(void);

  virtual AsyncHandle_t CreateAsyncHandle(void);
  virtual void ReleaseAsyncHandle(AsyncHandle_t handle);
  virtual int GetOverlappedResult(AsyncHandle_t handle, uint32_t *pResultCode,
                                  bool bWait);
  virtual void CancelOverlappedOperation(AsyncHandle_t handle);

  // Save/Load
  virtual void GetModSaveContainerNames(const char *pchModName,
                                        const wchar_t **ppchDisplayName,
                                        const char **ppchName);
  virtual uint GetContainerRemainingSpace(void);
  virtual bool DeviceCapacityAdequate(DWORD nDeviceID, const char *pModName);
  virtual DWORD DiscoverUserData(DWORD nUserID, const char *pModName);

  // XUI
  virtual bool ShowDeviceSelector(bool bForce, uint32_t *pStorageID,
                                  AsyncHandle_t *pHandle);
  virtual void ShowSigninUI(uint32_t nPanes, uint32_t nFlags);

  // Rich Presence and Matchmaking
  virtual int UserSetContext(uint32_t nUserIdx, uint32_t nContextID,
                             uint32_t nContextValue, bool bAsync,
                             AsyncHandle_t *pHandle);
  virtual int UserSetProperty(uint32_t nUserIndex, uint32_t nPropertyId,
                              uint32_t nBytes, const void *pvValue, bool bAsync,
                              AsyncHandle_t *pHandle);

  // Matchmaking
  virtual int CreateSession(uint32_t nFlags, uint32_t nUserIdx,
                            uint32_t nMaxPublicSlots, uint32_t nMaxPrivateSlots,
                            uint64_t *pNonce, void *pSessionInfo,
                            XboxHandle_t *pSessionHandle, bool bAsync,
                            AsyncHandle_t *pAsyncHandle);
  virtual uint DeleteSession(XboxHandle_t hSession, bool bAsync,
                             AsyncHandle_t *pAsyncHandle = NULL);
  virtual uint SessionSearch(uint32_t nProcedureIndex, uint32_t nUserIndex,
                             uint32_t nNumResults, uint32_t nNumUsers,
                             uint32_t nNumProperties, uint32_t nNumContexts,
                             XUSER_PROPERTY *pSearchProperties,
                             XUSER_CONTEXT *pSearchContexts,
                             uint32_t *pcbResultsBuffer,
                             XSESSION_SEARCHRESULT_HEADER *pSearchResults,
                             bool bAsync, AsyncHandle_t *pAsyncHandle);
  virtual uint SessionStart(XboxHandle_t hSession, uint32_t nFlags, bool bAsync,
                            AsyncHandle_t *pAsyncHandle);
  virtual uint SessionEnd(XboxHandle_t hSession, bool bAsync,
                          AsyncHandle_t *pAsyncHandle);
  virtual int SessionJoinLocal(XboxHandle_t hSession, uint32_t nUserCount,
                               const uint32_t *pUserIndexes,
                               const bool *pPrivateSlots, bool bAsync,
                               AsyncHandle_t *pAsyncHandle);
  virtual int SessionJoinRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                const XUID *pXuids, const bool *pPrivateSlot,
                                bool bAsync, AsyncHandle_t *pAsyncHandle);
  virtual int SessionLeaveLocal(XboxHandle_t hSession, uint32_t nUserCount,
                                const uint32_t *pUserIndexes, bool bAsync,
                                AsyncHandle_t *pAsyncHandle);
  virtual int SessionLeaveRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                 const XUID *pXuids, bool bAsync,
                                 AsyncHandle_t *pAsyncHandle);
  virtual int SessionMigrate(XboxHandle_t hSession, uint32_t nUserIndex,
                             void *pSessionInfo, bool bAsync,
                             AsyncHandle_t *pAsyncHandle);
  virtual int SessionArbitrationRegister(XboxHandle_t hSession, uint32_t nFlags,
                                         uint64_t nonce, uint32_t *pBytes,
                                         void *pBuffer, bool bAsync,
                                         AsyncHandle_t *pAsyncHandle);

  // Stats
  virtual int WriteStats(XboxHandle_t hSession, XUID xuid, uint32_t nViews,
                         void *pViews, bool bAsync,
                         AsyncHandle_t *pAsyncHandle);

  // Achievements
  virtual int EnumerateAchievements(uint32_t nUserIdx, uint64_t xuid,
                                    uint32_t nStartingIdx, uint32_t nCount,
                                    void *pBuffer, uint32_t nBufferBytes,
                                    bool bAsync, AsyncHandle_t *pAsyncHandle);
  virtual void AwardAchievement(uint32_t nUserIdx, uint32_t nAchievementId);

  virtual void FinishContainerWrites(void);
  virtual uint GetContainerOpenResult(void);
  virtual uint OpenContainers(void);
  virtual void CloseContainers(void);

 private:
  virtual uint CreateSavegameContainer(uint32_t nCreationFlags);
  virtual uint CreateUserSettingsContainer(uint32_t nCreationFlags);

  uint m_OpenContainerResult;
};

static CXboxSystem s_XboxSystem;
IXboxSystem *g_pXboxSystem = &s_XboxSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CXboxSystem, IXboxSystem,
                                  XBOXSYSTEM_INTERFACE_VERSION, s_XboxSystem);

#define ASYNC_RESULT(ph) ((AsyncResult_t *)*ph);

// Stubbed interface for win32
CXboxSystem::~CXboxSystem() {}
CXboxSystem::CXboxSystem() {}
AsyncHandle_t CXboxSystem::CreateAsyncHandle() { return NULL; }
void CXboxSystem::ReleaseAsyncHandle(AsyncHandle_t handle) {}
int CXboxSystem::GetOverlappedResult(AsyncHandle_t handle,
                                     uint32_t *pResultCode, bool bWait) {
  return 0;
}
void CXboxSystem::CancelOverlappedOperation(AsyncHandle_t handle){};
void CXboxSystem::GetModSaveContainerNames(const char *pchModName,
                                           const wchar_t **ppchDisplayName,
                                           const char **ppchName) {
  *ppchDisplayName = g_szModSaveContainerDisplayName;
  *ppchName = g_szModSaveContainerName;
}
DWORD CXboxSystem::DiscoverUserData(DWORD nUserID, const char *pModName) {
  return ((DWORD)-1);
}
bool CXboxSystem::DeviceCapacityAdequate(DWORD nDeviceID,
                                         const char *pModName) {
  return true;
}
uint CXboxSystem::GetContainerRemainingSpace() { return 0; }
bool CXboxSystem::ShowDeviceSelector(bool bForce, uint32_t *pStorageID,
                                     AsyncHandle_t *pHandle) {
  return false;
}
void CXboxSystem::ShowSigninUI(uint32_t nPanes, uint32_t nFlags) {}
int CXboxSystem::UserSetContext(uint32_t nUserIdx, uint32_t nContextID,
                                uint32_t nContextValue, bool bAsync,
                                AsyncHandle_t *pHandle) {
  return 0;
}
int CXboxSystem::UserSetProperty(uint32_t nUserIndex, uint32_t nPropertyId,
                                 uint32_t nBytes, const void *pvValue,
                                 bool bAsync, AsyncHandle_t *pHandle) {
  return 0;
}
int CXboxSystem::CreateSession(uint32_t nFlags, uint32_t nUserIdx,
                               uint32_t nMaxPublicSlots,
                               uint32_t nMaxPrivateSlots, uint64_t *pNonce,
                               void *pSessionInfo, XboxHandle_t *pSessionHandle,
                               bool bAsync, AsyncHandle_t *pAsyncHandle) {
  return 0;
}
uint CXboxSystem::DeleteSession(XboxHandle_t hSession, bool bAsync,
                                AsyncHandle_t *pAsyncHandle) {
  return 0;
}
uint CXboxSystem::SessionSearch(uint32_t nProcedureIndex, uint32_t nUserIndex,
                                uint32_t nNumResults, uint32_t nNumUsers,
                                uint32_t nNumProperties, uint32_t nNumContexts,
                                XUSER_PROPERTY *pSearchProperties,
                                XUSER_CONTEXT *pSearchContexts,
                                uint32_t *pcbResultsBuffer,
                                XSESSION_SEARCHRESULT_HEADER *pSearchResults,
                                bool bAsync, AsyncHandle_t *pAsyncHandle) {
  return 0;
}
uint CXboxSystem::SessionStart(XboxHandle_t hSession, uint32_t nFlags,
                               bool bAsync, AsyncHandle_t *pAsyncHandle) {
  return 0;
};
uint CXboxSystem::SessionEnd(XboxHandle_t hSession, bool bAsync,
                             AsyncHandle_t *pAsyncHandle) {
  return 0;
};
int CXboxSystem::SessionJoinLocal(XboxHandle_t hSession, uint32_t nUserCount,
                                  const uint32_t *pUserIndexes,
                                  const bool *pPrivateSlots, bool bAsync,
                                  AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::SessionJoinRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                   const XUID *pXuids, const bool *pPrivateSlot,
                                   bool bAsync, AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::SessionLeaveLocal(XboxHandle_t hSession, uint32_t nUserCount,
                                   const uint32_t *pUserIndexes, bool bAsync,
                                   AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::SessionLeaveRemote(XboxHandle_t hSession, uint32_t nUserCount,
                                    const XUID *pXuids, bool bAsync,
                                    AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::SessionMigrate(XboxHandle_t hSession, uint32_t nUserIndex,
                                void *pSessionInfo, bool bAsync,
                                AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::SessionArbitrationRegister(XboxHandle_t hSession,
                                            uint32_t nFlags, uint64_t nonce,
                                            uint32_t *pBytes, void *pBuffer,
                                            bool bAsync,
                                            AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::WriteStats(XboxHandle_t hSession, XUID xuid, uint32_t nViews,
                            void *pViews, bool bAsync,
                            AsyncHandle_t *pAsyncHandle) {
  return 0;
}
int CXboxSystem::EnumerateAchievements(uint32_t nUserIdx, uint64_t xuid,
                                       uint32_t nStartingIdx, uint32_t nCount,
                                       void *pBuffer, uint32_t nBufferBytes,
                                       bool bAsync,
                                       AsyncHandle_t *pAsyncHandle) {
  return 0;
}
void CXboxSystem::AwardAchievement(uint32_t nUserIdx, uint32_t nAchievementId) {
}
void CXboxSystem::FinishContainerWrites() {}
uint CXboxSystem::GetContainerOpenResult() { return 0; }
uint CXboxSystem::OpenContainers() { return 0; }
void CXboxSystem::CloseContainers() {}
uint CXboxSystem::CreateSavegameContainer(uint32_t nCreationFlags) { return 0; }
uint CXboxSystem::CreateUserSettingsContainer(uint32_t nCreationFlags) {
  return 0;
}
