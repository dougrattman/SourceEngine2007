// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef HLTVSERVER_H
#define HLTVSERVER_H

#include "base/include/base_types.h"
#include "baseserver.h"
#include "clientframe.h"
#include "convar.h"
#include "hltvclient.h"
#include "hltvclientstate.h"
#include "hltvdemo.h"
#include "ihltv.h"
#include "networkstringtable.h"

#define HLTV_BUFFER_DIRECTOR 0    // director commands
#define HLTV_BUFFER_RELIABLE 1    // reliable messages
#define HLTV_BUFFER_UNRELIABLE 2  // unreliable messages
#define HLTV_BUFFER_VOICE 3       // player voice data
#define HLTV_BUFFER_SOUNDS 4      // unreliable sounds
#define HLTV_BUFFER_TEMPENTS 5    // temporary/event entities
#define HLTV_BUFFER_MAX 6         // end marker

// proxy dispatch modes
#define DISPATCH_MODE_OFF 0
#define DISPATCH_MODE_AUTO 1
#define DISPATCH_MODE_ALWAYS 2

extern ConVar tv_debug;

class CHLTVFrame : public CClientFrame {
 public:
  CHLTVFrame();
  virtual ~CHLTVFrame();

  void Reset();  // resets all data & buffers
  void FreeBuffers();
  void AllocBuffers();
  bool HasData();
  void CopyHLTVData(CHLTVFrame &frame);
  virtual bool IsMemPoolAllocated() { return false; }

 public:
  // message buffers:
  bf_write m_Messages[HLTV_BUFFER_MAX];
};

struct CFrameCacheEntry_s {
  CClientFrame *pFrame;
  int nTick;
};

class CDeltaEntityCache {
  struct DeltaEntityEntry_s {
    DeltaEntityEntry_s *pNext;
    int nDeltaTick;
    int nBits;
  };

 public:
  CDeltaEntityCache();
  ~CDeltaEntityCache();

  void SetTick(int nTick, int nMaxEntities);
  u8 *FindDeltaBits(int nEntityIndex, int nDeltaTick, int &nBits);
  void AddDeltaBits(int nEntityIndex, int nDeltaTick, int nBits,
                    bf_write *pBuffer);
  void Flush();

 protected:
  int m_nTick;         // current tick
  int m_nMaxEntities;  // max entities = length of cache
  int m_nCacheSize;
  // array of pointers to delta entries
  DeltaEntityEntry_s *m_Cache[MAX_EDICTS];
};

class CGameClient;
class CGameServer;
class IHLTVDirector;

class CHLTVServer : public IGameEventListener2,
                    public CBaseServer,
                    public CClientFrameManager,
                    public IHLTVServer,
                    public IDemoPlayer {
  friend class CHLTVClientState;

 public:
  CHLTVServer();
  virtual ~CHLTVServer();

 public:  // CBaseServer interface:
  void Init(bool bIsDedicated);
  void Shutdown();
  void Clear();
  bool IsHLTV() const { return true; };
  bool IsMultiplayer() const { return true; };
  void FillServerInfo(SVC_ServerInfo &serverinfo);
  void GetNetStats(f32 &avgIn, f32 &avgOut);
  int GetChallengeType(netadr_t &adr);
  const ch *GetName() const;
  const ch *GetPassword() const;
  IClient *ConnectClient(netadr_t &adr, int protocol, int challenge,
                         int authProtocol, const ch *name, const ch *password,
                         const ch *hashedCDkey, int cdKeyLen);

 public:
  void FireGameEvent(IGameEvent *event);

 public:  // IHLTVServer interface:
  IServer *GetBaseServer();
  IHLTVDirector *GetDirector();
  int GetHLTVSlot();    // return entity index-1 of HLTV in game
  f32 GetOnlineTime();  // seconds since broadcast started
  void GetLocalStats(int &proxies, int &slots, int &clients);
  void GetGlobalStats(int &proxies, int &slots, int &clients);
  void GetRelayStats(int &proxies, int &slots, int &clients);

  bool IsMasterProxy();  // true, if this is the HLTV master proxy
  bool IsTVRelay();  // true if we're running a relay (i.e. this is the opposite
                     // of IsMasterProxy()).
  bool IsDemoPlayback();  // true if this is a HLTV demo

  const netadr_t *GetRelayAddress();  // returns relay address

  void BroadcastEvent(IGameEvent *event);

 public:  // IDemoPlayer interface
  CDemoFile *GetDemoFile();
  int GetPlaybackTick();
  int GetTotalTicks();

  bool StartPlayback(const ch *filename, bool bAsTimeDemo);

  bool IsPlayingBack();     // true if demo loaded and playing back
  bool IsPlaybackPaused();  // true if playback paused
  // true if playing back in timedemo mode
  bool IsPlayingTimeDemo() { return false; }
  // true, if demo player skiiping trough packets
  bool IsSkipping() { return false; };
  // true if demoplayer can skip backwards
  bool CanSkipBackwards() { return true; }

  void SetPlaybackTimeScale(f32 timescale);  // sets playback timescale
  f32 GetPlaybackTimeScale();                // get playback timescale

  void PausePlayback(f32 seconds){};
  void SkipToTick(int tick, bool bRelative, bool bPause){};
  void ResumePlayback(void){};
  void StopPlayback(void){};
  void InterpolateViewpoint(){};
  netpacket_t *ReadPacket() { return nullptr; }

  void ResetDemoInterpolation(void){};

 public:
  void StartMaster(CGameClient *client);  // start HLTV server as master proxy
  void ConnectRelay(const ch *address);   // connect to other HLTV proxy
  void StartDemo(const ch *filename);     // starts playing back a demo file
  void StartRelay();                      // start HLTV server as relay proxy
  bool SendNetMsg(INetMessage &msg, bool bForceReliable = false);
  void RunFrame();
  void SetMaxClients(int number);
  void Changelevel();

  void UserInfoChanged(int nClientIndex);
  void SendClientMessages(bool bSendSnapshots);
  // add new frame, returns HLTV's copy
  CClientFrame *AddNewFrame(CClientFrame *pFrame);
  void SignonComplete();
  void LinkInstanceBaselines();
  // broadcast event but not to relay proxies
  void BroadcastEventLocal(IGameEvent *event, bool bReliable);
  // broadcast event but not to relay proxies
  void BroadcastLocalChat(const ch *pszChat, const ch *pszGroup);
  // nullptr = broadcast to all
  void BroadcastLocalTitle(CHLTVClient *client = nullptr);
  bool DispatchToRelay(CHLTVClient *pClient);
  bf_write *GetBuffer(int nBuffer);
  CClientFrame *GetDeltaFrame(int nTick);

  inline CHLTVClient *Client(int i) {
    return static_cast<CHLTVClient *>(m_Clients[i]);
  }

 protected:
  virtual bool ShouldUpdateMasterServer();

 private:
  CBaseClient *CreateNewClient(int slot);
  void UpdateTick();
  void UpdateStats();
  void InstallStringTables();
  void RestoreTick(int tick);
  void EntityPVSCheck(CClientFrame *pFrame);
  void InitClientRecvTables();
  void FreeClientRecvTables();
  void ReadCompeleteDemoFile();
  void ResyncDemoClock();

 public:
  CGameClient *m_MasterClient;  // if != nullptr, this is the master HLTV
  CHLTVClientState m_ClientState;
  // HLTV demo object for recording and playback
  CHLTVDemoRecorder m_DemoRecorder;
  CGameServer *m_Server;       // pointer to source server (sv.)
  IHLTVDirector *m_Director;   // HTLV director exported by game.dll
  int m_nFirstTick;            // first known server tick;
  int m_nLastTick;             // last tick from AddFrame()
  CHLTVFrame *m_CurrentFrame;  // current delayed HLTV frame
  int m_nViewEntity;           // the current entity HLTV is tracking
  int m_nPlayerSlot;           // slot of HLTV client on game server
  // all incoming messages go here until Snapshot is made
  CHLTVFrame m_HLTVFrame;

  bool m_bSignonState;  // true if connecting to server
  f32 m_flStartTime;
  f32 m_flFPS;                  // FPS the proxy is running;
  int m_nGameServerMaxClients;  // max clients on game server
  f32 m_fNextSendUpdateTime;    // time to send next HLTV status messages
  RecvTable *m_pRecvTables[MAX_DATATABLES];
  usize m_nRecvTables;
  Vector m_vPVSOrigin;
  bool m_bMasterOnlyMode;

  netadr_t m_RootServer;  // HLTV root server
  int m_nGlobalSlots;
  int m_nGlobalClients;
  int m_nGlobalProxies;

  CNetworkStringTableContainer m_NetworkStringTables;

  CDeltaEntityCache m_DeltaCache;
  CUtlVector<CFrameCacheEntry_s> m_FrameCache;

  // demoplayer stuff:
  CDemoFile m_DemoFile;  // for demo playback
  int m_nStartTick;
  democmdinfo_t m_LastCmdInfo;
  bool m_bPlayingBack;
  bool m_bPlaybackPaused;  // true if demo is paused right now
  f32 m_flPlaybackRateModifier;
  int m_nSkipToTick;  // skip to tick ASAP, -1 = off
};

extern CHLTVServer *hltv;  // The global HLTV server/object. nullptr on xbox.

#endif  // HLTVSERVER_H
