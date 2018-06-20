// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"  // for CloseHandle()
#endif

#include "Session.h"

#include "hl2orange.spa.h"
#include "matchmaking.h"
#include "strtools.h"
#include "tier0/include/dbg.h"
#include "tier0/include/tslist.h"
#include "tier1/UtlLinkedList.h"

#include "tier0/include/memdbgon.h"

#define ASYNC_OK 0
#define ASYNC_FAIL 1

//-----------------------------------------------------------------------------
// Purpose: CSession class
//-----------------------------------------------------------------------------
CSession::CSession() {
  m_pParent = NULL;
  m_hSession = INVALID_HANDLE_VALUE;
  m_SessionState = SESSION_STATE_NONE;
  m_pRegistrationResults = NULL;

  ResetSession();
}

CSession::~CSession() { ResetSession(); }

//-----------------------------------------------------------------------------
// Purpose: Reset a session to it's initial state
//-----------------------------------------------------------------------------
void CSession::ResetSession() {
  // Cleanup first
  switch (m_SessionState) {
    case SESSION_STATE_CREATING:
      CancelCreateSession();
      break;

    case SESSION_STATE_MIGRATING:
      // X360TBD:
      break;
  }

  if (m_hSession != INVALID_HANDLE_VALUE) {
    Msg("ResetSession: Destroying current session.\n");

    DestroySession();
    m_hSession = INVALID_HANDLE_VALUE;
  }

  SwitchToState(SESSION_STATE_NONE);

  m_bIsHost = false;
  m_bIsArbitrated = false;
  m_bUsingQoS = false;
  m_bIsSystemLink = false;
  Q_memset(&m_nPlayerSlots, 0, sizeof(m_nPlayerSlots));
  Q_memset(&m_SessionInfo, 0, sizeof(m_SessionInfo));

  delete m_pRegistrationResults;

  m_nSessionFlags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set session contexts and properties
//-----------------------------------------------------------------------------
void CSession::SetContext(const uint32_t nContextId,
                          const uint32_t nContextValue, const bool bAsync) {}
void CSession::SetProperty(const uint32_t nPropertyId, const uint32_t cbValue,
                           const void *pvValue, const bool bAsync) {}

//-----------------------------------------------------------------------------
// Purpose: Send a notification to GameUI
//-----------------------------------------------------------------------------
void CSession::SendNotification(SESSION_NOTIFY notification) {
  Assert(m_pParent);
  m_pParent->SessionNotification(notification);
}

//-----------------------------------------------------------------------------
// Purpose: Update the number of player slots filled
//-----------------------------------------------------------------------------
void CSession::UpdateSlots(const CClientInfo *pClient, bool bAddPlayers) {
  int cPlayers = pClient->m_cPlayers;
  if (bAddPlayers) {
    if (pClient->m_bInvited) {
      // Fill private slots first, then overflow into public slots
      m_nPlayerSlots[SLOTS_FILLEDPRIVATE] += cPlayers;
      pClient->m_numPrivateSlotsUsed = cPlayers;
      cPlayers = 0;
      if (m_nPlayerSlots[SLOTS_FILLEDPRIVATE] >
          m_nPlayerSlots[SLOTS_TOTALPRIVATE]) {
        cPlayers = m_nPlayerSlots[SLOTS_FILLEDPRIVATE] -
                   m_nPlayerSlots[SLOTS_TOTALPRIVATE];
        pClient->m_numPrivateSlotsUsed -= cPlayers;
        m_nPlayerSlots[SLOTS_FILLEDPRIVATE] =
            m_nPlayerSlots[SLOTS_TOTALPRIVATE];
      }
    }
    m_nPlayerSlots[SLOTS_FILLEDPUBLIC] += cPlayers;
    if (m_nPlayerSlots[SLOTS_FILLEDPUBLIC] >
        m_nPlayerSlots[SLOTS_TOTALPUBLIC]) {
      // Handle error
      Warning("Too many players!\n");
    }
  } else {
    m_nPlayerSlots[SLOTS_FILLEDPRIVATE] =
        std::max(0u, m_nPlayerSlots[SLOTS_FILLEDPRIVATE] -
                         pClient->m_numPrivateSlotsUsed);
    m_nPlayerSlots[SLOTS_FILLEDPUBLIC] = std::max(
        0u, m_nPlayerSlots[SLOTS_FILLEDPUBLIC] -
                (pClient->m_cPlayers - pClient->m_numPrivateSlotsUsed));
  }
}

//-----------------------------------------------------------------------------
// Purpose: Join players on the local client
//-----------------------------------------------------------------------------
void CSession::JoinLocal(const CClientInfo *pClient) {
  uint32_t nUserIndex[MAX_PLAYERS_PER_CLIENT] = {0};
  bool bPrivate[MAX_PLAYERS_PER_CLIENT] = {0};

  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    nUserIndex[i] = pClient->m_iControllers[i];
    bPrivate[i] = pClient->m_bInvited;
  }

  UpdateSlots(pClient, true);
}

//-----------------------------------------------------------------------------
// Purpose: Join players on a remote client
//-----------------------------------------------------------------------------
void CSession::JoinRemote(const CClientInfo *pClient) {
  XUID xuids[MAX_PLAYERS_PER_CLIENT] = {0};
  bool bPrivate[MAX_PLAYERS_PER_CLIENT] = {0};

  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    xuids[i] = pClient->m_xuids[i];
    bPrivate[i] = pClient->m_bInvited;
  }

  UpdateSlots(pClient, true);
}

//-----------------------------------------------------------------------------
// Purpose: Remove players on the local client
//-----------------------------------------------------------------------------
void CSession::RemoveLocal(const CClientInfo *pClient) {
  uint32_t nUserIndex[MAX_PLAYERS_PER_CLIENT] = {0};

  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    nUserIndex[i] = pClient->m_iControllers[i];
  }

  UpdateSlots(pClient, false);
}

//-----------------------------------------------------------------------------
// Purpose: Remove players on a remote client
//-----------------------------------------------------------------------------
void CSession::RemoveRemote(const CClientInfo *pClient) {
  XUID xuids[MAX_PLAYERS_PER_CLIENT] = {0};

  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    xuids[i] = pClient->m_xuids[i];
  }

  UpdateSlots(pClient, false);
}

//-----------------------------------------------------------------------------
// Purpose: Create a new session
//-----------------------------------------------------------------------------
bool CSession::CreateSession() {
  if (INVALID_HANDLE_VALUE != m_hSession) {
    Warning("CreateSession called on existing session!");
    DestroySession();
    m_hSession = INVALID_HANDLE_VALUE;
  }

  uint32_t flags = m_nSessionFlags;
  if (m_bIsHost) {
    flags |= XSESSION_CREATE_HOST;
  }

  if (flags & XSESSION_CREATE_USES_ARBITRATION) {
    m_bIsArbitrated = true;
  }

  SwitchToState(SESSION_STATE_CREATING);

  return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion while creating a new session
//-----------------------------------------------------------------------------
void CSession::UpdateCreating() {
  SESSION_STATE nextState = SESSION_STATE_IDLE;

  // Operation completed
  SESSION_NOTIFY notification =
      IsHost() ? SESSION_NOTIFY_CREATED_HOST : SESSION_NOTIFY_CREATED_CLIENT;

  SendNotification(notification);
  SwitchToState(nextState);
}

//-----------------------------------------------------------------------------
// Purpose: Cancel async session creation
//-----------------------------------------------------------------------------
void CSession::CancelCreateSession() {
#ifndef OS_POSIX
  if (m_SessionState != SESSION_STATE_CREATING) return;

  if (INVALID_HANDLE_VALUE != m_hSession) {
    CloseHandle(m_hSession);
    m_hSession = INVALID_HANDLE_VALUE;
  }
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Close an existing session
//-----------------------------------------------------------------------------
void CSession::DestroySession() {}

//-----------------------------------------------------------------------------
// Purpose: Register for arbritation in a ranked match
//-----------------------------------------------------------------------------
void CSession::RegisterForArbitration() {
  Warning("Failed registering for arbitration\n");
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion of arbitration registration
//-----------------------------------------------------------------------------
void CSession::UpdateRegistering() {
  SESSION_STATE nextState = SESSION_STATE_IDLE;
  // Operation completed
  SESSION_NOTIFY notification = SESSION_NOTIFY_REGISTER_COMPLETED;

  SendNotification(notification);
  SwitchToState(nextState);
}

//-----------------------------------------------------------------------------
// Purpose: Migrate the session to a new host
//-----------------------------------------------------------------------------
bool CSession::MigrateHost() {
  if (IsHost()) {
    // Migrate call will fill this in for us
    memcpy(&m_NewSessionInfo, &m_SessionInfo, sizeof(m_NewSessionInfo));
  }

  return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion while migrating a session
//-----------------------------------------------------------------------------
void CSession::UpdateMigrating() {
  SESSION_STATE nextState = SESSION_STATE_IDLE;
  // Operation completed
  SESSION_NOTIFY notification = SESSION_NOTIFY_MIGRATION_COMPLETED;

  SendNotification(notification);
  SwitchToState(nextState);
}

//-----------------------------------------------------------------------------
// Purpose: Change state
//-----------------------------------------------------------------------------
void CSession::SwitchToState(SESSION_STATE newState) {
  m_SessionState = newState;
}

//-----------------------------------------------------------------------------
// Purpose: Per-frame update
//-----------------------------------------------------------------------------
void CSession::RunFrame() {
  switch (m_SessionState) {
    case SESSION_STATE_CREATING:
      UpdateCreating();
      break;

    case SESSION_STATE_REGISTERING:
      UpdateRegistering();
      break;

    case SESSION_STATE_MIGRATING:
      UpdateMigrating();
      break;

    default:
      break;
  }
}

//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
HANDLE CSession::GetSessionHandle() { return m_hSession; }
void CSession::SetSessionInfo(XSESSION_INFO *pInfo) {
  memcpy(&m_SessionInfo, pInfo, sizeof(XSESSION_INFO));
}
void CSession::SetNewSessionInfo(XSESSION_INFO *pInfo) {
  memcpy(&m_NewSessionInfo, pInfo, sizeof(XSESSION_INFO));
}
void CSession::GetSessionInfo(XSESSION_INFO *pInfo) {
  Assert(pInfo);
  memcpy(pInfo, &m_SessionInfo, sizeof(XSESSION_INFO));
}
void CSession::GetNewSessionInfo(XSESSION_INFO *pInfo) {
  Assert(pInfo);
  memcpy(pInfo, &m_NewSessionInfo, sizeof(XSESSION_INFO));
}
void CSession::SetSessionNonce(int64_t nonce) { m_SessionNonce = nonce; }
uint64_t CSession::GetSessionNonce() { return m_SessionNonce; }
XNKID CSession::GetSessionId() { return m_SessionInfo.sessionID; }
void CSession::SetSessionSlots(unsigned int nSlot, unsigned int nPlayers) {
  Assert(nSlot < SLOTS_LAST);
  m_nPlayerSlots[nSlot] = nPlayers;
}
unsigned int CSession::GetSessionSlots(unsigned int nSlot) {
  Assert(nSlot < SLOTS_LAST);
  return m_nPlayerSlots[nSlot];
}
void CSession::SetSessionFlags(uint32_t flags) { m_nSessionFlags = flags; }
uint32_t CSession::GetSessionFlags() { return m_nSessionFlags; }
int CSession::GetPlayerCount() {
  return m_nPlayerSlots[SLOTS_FILLEDPRIVATE] +
         m_nPlayerSlots[SLOTS_FILLEDPUBLIC];
}
void CSession::SetFlag(uint32_t nFlag) { m_nSessionFlags |= nFlag; }
void CSession::SetIsHost(bool bHost) { m_bIsHost = bHost; }
void CSession::SetIsSystemLink(bool bSystemLink) {
  m_bIsSystemLink = bSystemLink;
}
void CSession::SetOwnerId(uint32_t id) { m_nOwnerId = id; }
bool CSession::IsHost() { return m_bIsHost; }
bool CSession::IsFull() {
  return (GetSessionSlots(SLOTS_TOTALPRIVATE) ==
              GetSessionSlots(SLOTS_FILLEDPRIVATE) &&
          GetSessionSlots(SLOTS_TOTALPUBLIC) ==
              GetSessionSlots(SLOTS_FILLEDPUBLIC));
}
bool CSession::IsArbitrated() { return m_bIsArbitrated; }
bool CSession::IsSystemLink() { return m_bIsSystemLink; }
void CSession::SetParent(CMatchmaking *pParent) { m_pParent = pParent; }
double CSession::GetTime() { return Plat_FloatTime(); }
