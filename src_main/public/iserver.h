// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ISERVER_H
#define ISERVER_H

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "bitvec.h"
#include "const.h"
#include "inetmsghandler.h"

class INetMessage;
class IRecipientFilter;
class IClient;

using player_info_t = struct player_info_s;

the_interface IServer : public IConnectionlessPacketHandler {
 public:
  virtual ~IServer() {}

  // returns current number of clients
  virtual int GetNumClients() const = 0;
  // returns number of attached HLTV proxies
  virtual int GetNumProxies() const = 0;
  // returns number of fake clients/bots
  virtual int GetNumFakeClients() const = 0;
  virtual int GetMaxClients() const = 0;  // returns current client limit

  virtual IClient *GetClient(int index) = 0;  // returns interface to client
  // returns number of clients slots (used & unused)
  virtual int GetClientCount() const = 0;

  virtual int GetUDPPort() const = 0;        // returns current used UDP port
  virtual f32 GetTime() const = 0;           // returns game world time
  virtual int GetTick() const = 0;           // returns game world tick
  virtual f32 GetTickInterval() const = 0;   // tick interval in seconds
  virtual const ch *GetName() const = 0;     // public server name
  virtual const ch *GetMapName() const = 0;  // current map name (BSP)
  virtual int GetSpawnCount() const = 0;
  virtual int GetNumClasses() const = 0;
  virtual int GetClassBits() const = 0;
  // total net in/out in bytes/sec
  virtual void GetNetStats(f32 & avgIn, f32 & avgOut) = 0;
  virtual int GetNumPlayers() = 0;
  virtual bool GetPlayerInfo(int nClientIndex, player_info_t *pinfo) = 0;

  virtual bool IsActive() const = 0;
  virtual bool IsLoading() const = 0;
  virtual bool IsDedicated() const = 0;
  virtual bool IsPaused() const = 0;
  virtual bool IsMultiplayer() const = 0;
  virtual bool IsPausable() const = 0;
  virtual bool IsHLTV() const = 0;

  // returns the password or NULL if none set
  virtual const ch *GetPassword() const = 0;

  virtual void SetPaused(bool paused) = 0;
  // set password (NULL to disable)
  virtual void SetPassword(const ch *password) = 0;

  virtual void BroadcastMessage(INetMessage & msg, bool onlyActive = false,
                                bool reliable = false) = 0;
  virtual void BroadcastMessage(INetMessage & msg,
                                IRecipientFilter & filter) = 0;

  virtual void DisconnectClient(IClient * client, const ch *reason) = 0;
};

#endif  // ISERVER_H
