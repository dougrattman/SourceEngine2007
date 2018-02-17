// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SVCONNECTIONLESSHANDLER_H
#define SVCONNECTIONLESSHANDLER_H

#include "inetmsghandler.h"

class CServerConnectionlessHandler : public IConnectionlessPacketHandler {
 public:
  CServerConnectionlessHandler();
  virtual ~CServerConnectionlessHandler();

 public:
  bool ProcessConnectionlessPacket(netpacket_t* packet);

 private:
  bool ProcessLog(netpacket_t* packet);
  bool ProcessGetChallenge(netpacket_t* packet);
  bool ProcessConnect(netpacket_t* packet);
  bool ProcessInfo(netpacket_t* packet);
  bool ProcessDetails(netpacket_t* packet);
  bool ProcessPlayers(netpacket_t* packet);
  bool ProcessRules(netpacket_t* packet);
  bool ProcessRcon(netpacket_t* packet);
};

extern CServerConnectionlessHandler g_SVConnectionlessHandler;

#endif  // SVCONNECTIONLESSHANDLER_H
