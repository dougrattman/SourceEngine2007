// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASESERVER_H
#define BASESERVER_H

#include "baseclient.h"
#include "bitbuf.h"
#include "event_system.h"
#include "iserver.h"
#include "net.h"
#include "netadr.h"
#include "netmessages.h"
#include "steam/isteammasterserverupdater.h"
#include "tier1/UtlVector.h"

class CNetworkStringTableContainer;
class PackedEntity;
class ServerClass;
class INetworkStringTable;

enum server_state_t {
  ss_dead = 0,  // Dead
  ss_loading,   // Spawning
  ss_active,    // Running
  ss_paused,    // Running, but paused
};

// See baseserver.cpp for #define which controls this
bool AllowDebugDedicatedServerOutsideSteam();

// MAX_CHALLENGES is made large to prevent a denial of service attack that could
// cycle all of them out before legitimate users connected
#define MAX_CHALLENGES 16384

// Time a challenge is valid for, in seconds
#define CHALLENGE_LIFETIME 60 * 60.0f

// MAX_DELTA_TICKS defines the maximum delta difference allowed for delta
// compression, if clients request on older tick as delta baseline, send a full
// update.
#define MAX_DELTA_TICKS 192  // this is about 3 seconds

struct challenge_t {
  netadr_t adr;   // Address where challenge value was sent to.
  int challenge;  // To connect, adr IP address must respond with this #
  float time;     // # is valid for only a short duration.
};

class CBaseServer : public IServer {
  friend class CMaster;

 public:
  CBaseServer();
  virtual ~CBaseServer();

  bool RestartOnLevelChange() { return m_bRestartOnLevelChange; }

 public:                                  // IServer implementation
  virtual int GetNumClients(void) const;  // returns current number of clients
  virtual int GetNumProxies(
      void) const;  // returns number of attached HLTV proxies
  virtual int GetNumFakeClients() const;  // returns number of fake clients/bots
  virtual int GetMaxClients() const {
    return m_nMaxclients;
  }  // returns current client limit
  virtual int GetUDPPort() const { return NET_GetUDPPort(m_Socket); }
  virtual IClient *GetClient(int index) {
    return m_Clients[index];
  }  // returns interface to client
  virtual int GetClientCount() const {
    return m_Clients.Count();
  }  // for iteration;
  virtual float GetTime(void) const;
  virtual int GetTick() const { return m_nTickCount; }
  virtual float GetTickInterval() const { return m_flTickInterval; }
  virtual const ch *GetName(void) const;
  virtual const ch *GetMapName() const { return m_szMapname; }
  virtual int GetSpawnCount() const { return m_nSpawnCount; }
  virtual int GetNumClasses() const { return serverclasses; }
  virtual int GetClassBits() const { return serverclassbits; }
  virtual void GetNetStats(float &avgIn, float &avgOut);
  virtual int GetNumPlayers();
  virtual bool GetPlayerInfo(int nClientIndex, player_info_t *pinfo);
  virtual float GetCPUUsage() { return m_fCPUPercent; }

  virtual bool IsActive() const { return m_State >= ss_active; }
  virtual bool IsLoading() const { return m_State == ss_loading; }
  virtual bool IsDedicated() const { return m_bIsDedicated; }
  virtual bool IsPaused() const { return m_State == ss_paused; }
  virtual bool IsMultiplayer() const { return m_nMaxclients > 1; }
  virtual bool IsPausable() const { return false; }
  virtual bool IsHLTV() const { return false; }

  virtual void BroadcastMessage(INetMessage &msg, bool onlyActive = false,
                                bool reliable = false);
  virtual void BroadcastMessage(INetMessage &msg, IRecipientFilter &filter);
  virtual void BroadcastPrintf(const ch *fmt, ...);

  virtual const ch *GetPassword() const;

  virtual void SetMaxClients(int number);
  virtual void SetPaused(bool paused);
  virtual void SetPassword(const ch *password);

  virtual void DisconnectClient(IClient *client, const ch *reason);

  virtual void WriteDeltaEntities(CBaseClient *client, CClientFrame *to,
                                  CClientFrame *from, bf_write &pBuf);
  virtual void WriteTempEntities(CBaseClient *client, CFrameSnapshot *to,
                                 CFrameSnapshot *from, bf_write &pBuf,
                                 int nMaxEnts);

 public:  // IConnectionlessPacketHandler implementation
  virtual bool ProcessConnectionlessPacket(netpacket_t *packet);

  virtual void Init(bool isDedicated);
  virtual void Clear(void);
  virtual void Shutdown(void);
  virtual CBaseClient *CreateFakeClient(const ch *name);
  virtual void RemoveClientFromGame(CBaseClient *client){};
  virtual void SendClientMessages(bool bSendSnapshots);
  virtual void FillServerInfo(SVC_ServerInfo &serverinfo);
  virtual void UserInfoChanged(int nClientIndex);

  bool GetClassBaseline(ServerClass *pClass, void const **pData, int *pDatalen);
  void RunFrame(void);
  void InactivateClients(void);
  void ReconnectClients(void);
  void CheckTimeouts();
  void UpdateUserSettings();
  void SendPendingServerInfo();

  const ch *CompressPackedEntity(ServerClass *pServerClass, const ch *data,
                                 int &bits);
  const ch *UncompressPackedEntity(PackedEntity *pPackedEntity, int &size);

  INetworkStringTable *GetInstanceBaselineTable(void);
  INetworkStringTable *GetLightStyleTable(void);
  INetworkStringTable *GetUserInfoTable(void);

  virtual void RejectConnection(const netadr_t &adr, const ch *fmt, ...);

  float GetFinalTickTime(void) const;

  virtual bool CheckIPRestrictions(const netadr_t &adr, int nAuthProtocol);

  void SetMasterServerRulesDirty();
  void SendQueryPortToClient(netadr_t &adr);

  void RecalculateTags(void);
  void AddTag(const ch *pszTag);
  void RemoveTag(const ch *pszTag);

 protected:
  virtual IClient *ConnectClient(netadr_t &adr, int protocol, int challenge,
                                 int authProtocol, const ch *name,
                                 const ch *password, const ch *hashedCDkey,
                                 int cdKeyLen);

  virtual CBaseClient *GetFreeClient(netadr_t &adr);

  virtual CBaseClient *CreateNewClient(int slot) {
    return NULL;
  };  // must be derived

  virtual bool FinishCertificateCheck(netadr_t &adr, int nAuthProtocol,
                                      const ch *szRawCertificate) {
    return true;
  };

  virtual int GetChallengeNr(netadr_t &adr);
  virtual int GetChallengeType(netadr_t &adr);

  virtual bool CheckProtocol(netadr_t &adr, int nProtocol);
  virtual bool CheckChallengeNr(netadr_t &adr, int nChallengeValue);
  virtual bool CheckChallengeType(CBaseClient *client, int nNewUserID,
                                  netadr_t &adr, int nAuthProtocol,
                                  const ch *pchLogonCookie, int cbCookie);
  virtual bool CheckPassword(netadr_t &adr, const ch *password, const ch *name);
  virtual bool CheckIPConnectionReuse(netadr_t &adr);

  virtual void ReplyChallenge(netadr_t &adr);
  virtual void ReplyServerChallenge(netadr_t &adr);

  virtual void CalculateCPUUsage();

  // Keep the master server data updated.
  virtual bool ShouldUpdateMasterServer();

  void CheckMasterServerRequestRestart();
  void UpdateMasterServer();
  void UpdateMasterServerRules();
  virtual void UpdateMasterServerPlayers() {}
  void UpdateMasterServerBasicData();
  void ForwardPacketsFromMasterServerUpdater();

  void SetRestartOnLevelChange(bool state) { m_bRestartOnLevelChange = state; }

  bool RequireValidChallenge(netadr_t &adr);
  bool ValidChallenge(netadr_t &adr, int challengeNr);
  bool ValidInfoChallenge(netadr_t &adr, const ch *nugget);

  // Data
 public:
  server_state_t m_State;  // some actions are only valid during load
  int m_Socket;            // network socket
  int m_nTickCount;        // current server tick
  ch m_szMapname[64];      // map name without path and extension
  ch m_szSkyname[64];      // skybox name
  ch m_Password[32];       // server password

  CRC32_t worldmapCRC;   // For detecting that client has a hacked local copy of
                         // map, the client will be dropped if this occurs.
  CRC32_t clientDllCRC;  // The dll that this server is expecting clients to be
                         // using.

  CNetworkStringTableContainer
      *m_StringTables;  // newtork string table container

  INetworkStringTable *m_pInstanceBaselineTable;
  INetworkStringTable *m_pLightStyleTable;
  INetworkStringTable *m_pUserInfoTable;
  INetworkStringTable *m_pServerStartupTable;
  INetworkStringTable *m_pDownloadableFileTable;

  // This will get set to NET_MAX_PAYLOAD if the server is MP.
  bf_write m_Signon;
  CUtlMemory<byte> m_SignonBuffer;

  int serverclasses;    // number of unique server classes
  int serverclassbits;  // log2 of serverclasses

 private:
  // Gets the next user ID mod SHRT_MAX and unique (not used by any active
  // clients).
  int GetNextUserID();
  int m_nUserid;  // increases by one with every new client

 protected:
  int m_nMaxclients;  // Current max #
  int m_nSpawnCount;  // Number of servers spawned since start,
  // used to check late spawns (e.g., when d/l'ing lots of
  // data)
  float m_flTickInterval;  // time for 1 tick in seconds

  CUtlVector<CBaseClient *>
      m_Clients;  // array of up to [maxclients] client slots.

  bool m_bIsDedicated;

  CUtlVector<challenge_t> m_ServerQueryChallenges;  // prevent spoofed IP's from
                                                    // server queries/connecting

  float m_fCPUPercent;
  float m_fStartTime;
  float m_fLastCPUCheckTime;

  // This is only used for Steam's master server updater to refer to this server
  // uniquely.
  bool m_bRestartOnLevelChange;

  bool m_bMasterServerRulesDirty;
  double m_flLastMasterServerUpdateTime;
};

#endif  // BASESERVER_H
