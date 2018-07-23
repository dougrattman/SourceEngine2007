// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef NET_WS_QUEUED_PACKET_SENDER_H
#define NET_WS_QUEUED_PACKET_SENDER_H

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "base/include/windows/windows_light.h"
#include "tier1/convar.h"

#include <winsock.h>

// Used to match against certain debug values of cvars.
#define NET_QUEUED_PACKET_THREAD_DEBUG_VALUE 581304

the_interface INetChannel;

the_interface IQueuedPacketSender {
 public:
  virtual bool Setup() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsRunning() = 0;
  virtual void ClearQueuedPacketsForChannel(INetChannel * cnannel) = 0;
  virtual void QueuePacket(INetChannel * cnannel, SOCKET s, const char FAR *buf,
                           int len, const struct sockaddr FAR *to, int tolen,
                           u32 msecDelay) = 0;
};

extern IQueuedPacketSender *g_pQueuedPackedSender;
extern ConVar net_queued_packet_thread;

#endif  // NET_WS_QUEUED_PACKET_SENDER_H
