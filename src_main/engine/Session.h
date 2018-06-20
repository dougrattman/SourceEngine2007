// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SESSION_H
#define SESSION_H

#include "const.h"
#include "engine/imatchmaking.h"
#include "net.h"

// valid session states
enum SESSION_STATE {
  SESSION_STATE_NONE,
  SESSION_STATE_CREATING,
  SESSION_STATE_MIGRATING,
  SESSION_STATE_IDLE,
  SESSION_STATE_WAITING_FOR_REGISTRATION,
  SESSION_STATE_REGISTERING,
  SESSION_STATE_REGISTERED,
  SESSION_STATE_STARTING,
  SESSION_STATE_IN_GAME,
  SESSION_STATE_ENDING,
  SESSION_STATE_FINISHED,
  SESSION_STATE_DELETING
};

// slot types for the session
enum SLOTS {
  SLOTS_TOTALPUBLIC,
  SLOTS_TOTALPRIVATE,
  SLOTS_FILLEDPUBLIC,
  SLOTS_FILLEDPRIVATE,
  SLOTS_LAST
};

class CClientInfo {
 public:
  uint64_t m_id;                         // machine id
  netadr_t m_adr;                        // IP and Port
  XNADDR m_xnaddr;                       // XNADDR
  XUID m_xuids[MAX_PLAYERS_PER_CLIENT];  // XUIDs
  bool m_bInvited;                       // use private slots
  bool m_bRegistered;                    // registered for arbitration
  bool m_bMigrated;                      // successfully completed migration
  bool m_bModified;                      // completed session modification
  bool m_bReportedStats;                 // reported session stats to Live
  bool m_bLoaded;                        // map load is complete
  uint8_t m_cVoiceState[MAX_PLAYERS_PER_CLIENT];  // has voice permission
  char m_cPlayers;  // number of players on this client
  char m_iControllers[MAX_PLAYERS_PER_CLIENT];  // the controller (user index)
                                                // for each player
  char m_iTeam[MAX_PLAYERS_PER_CLIENT];         // each player's team
  char m_szGamertags[MAX_PLAYERS_PER_CLIENT][MAX_PLAYER_NAME_LENGTH];
  char mutable m_numPrivateSlotsUsed;  // number of private slots used by this
                                       // client if invited

  CClientInfo() { Clear(); }
  void Clear() {
    Q_memset(this, 0, sizeof(CClientInfo));
    Q_memset(&m_iTeam, -1, sizeof(m_iTeam));
  }
};

class CMatchmaking;
class CSession {
 public:
  CSession();
  ~CSession();

  void ResetSession();
  void SetParent(CMatchmaking *pParent);
  void RunFrame();
  bool CreateSession();
  void CancelCreateSession();
  void DestroySession();
  void RegisterForArbitration();
  bool MigrateHost();

  void JoinLocal(const CClientInfo *pClient);
  void JoinRemote(const CClientInfo *pClient);
  void RemoveLocal(const CClientInfo *pClient);
  void RemoveRemote(const CClientInfo *pClient);

  // Accessors
  HANDLE GetSessionHandle();
  void SetSessionInfo(XSESSION_INFO *pInfo);
  void SetNewSessionInfo(XSESSION_INFO *pInfo);
  void GetSessionInfo(XSESSION_INFO *pInfo);
  void GetNewSessionInfo(XSESSION_INFO *pInfo);
  void SetSessionNonce(int64_t nonce);
  uint64_t GetSessionNonce();
  XNKID GetSessionId();
  void SetSessionSlots(unsigned int nSlot, unsigned int nPlayers);
  uint32_t GetSessionSlots(unsigned int nSlot);
  void SetSessionFlags(uint32_t flags);
  uint32_t GetSessionFlags();
  int GetPlayerCount();
  void SetFlag(uint32_t dwFlag);
  void SetIsHost(bool bHost);
  void SetIsSystemLink(bool bSystemLink);
  void SetOwnerId(uint32_t id);
  bool IsHost();
  bool IsFull();
  bool IsArbitrated();
  bool IsSystemLink();

  void SwitchToState(SESSION_STATE newState);

  void SetContext(const uint32_t nContextId, const uint32_t nContextValue,
                  const bool bAsync = true);
  void SetProperty(const uint32_t nPropertyId, const uint32_t cbValue,
                   const void *pvValue, const bool bAsync = true);

  XSESSION_REGISTRATION_RESULTS *GetRegistrationResults() {
    return m_pRegistrationResults;
  }

 private:
  // Update functions
  void UpdateCreating();
  void UpdateMigrating();
  void UpdateRegistering();
  void SendNotification(SESSION_NOTIFY notification);
  void UpdateSlots(const CClientInfo *pClient, bool bAddPlayers);
  double GetTime();

  HANDLE m_hSession;               // Session handle
  bool m_bIsHost;                  // Is hosting
  bool m_bIsArbitrated;            // Is Arbitrated
  bool m_bUsingQoS;                // Is the QoS listener enabled
  bool m_bIsSystemLink;            // Is this a system link session
  XSESSION_INFO m_SessionInfo;     // Session ID, key, and host address
  XSESSION_INFO m_NewSessionInfo;  // Session ID, key, and host address
  uint64_t m_SessionNonce;         // Nonce of the session
  uint32_t m_nSessionFlags;        // Session creation flags
  uint32_t m_nOwnerId;             // Which player created the session
  uint32_t m_SessionState;
  double m_fOperationStartTime;
  CMatchmaking *m_pParent;

  XSESSION_REGISTRATION_RESULTS *m_pRegistrationResults;

  // public/private slots for the session
  uint32_t m_nPlayerSlots[SLOTS_LAST];
};

#endif  // SESSION_H
