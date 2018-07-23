// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include "net_ws_headers.h"
#include "net_ws_queued_packet_sender.h"

#include "tier1/utlpriorityqueue.h"
#include "tier1/utlvector.h"

#include "tier0/include/memdbgon.h"

ConVar net_queued_packet_thread{"net_queued_packet_thread", "1", 0,
                                "Use a high priority thread to send queued "
                                "packets out instead of sending them each "
                                "frame."};
ConVar net_queue_trace{"net_queue_trace", "0", 0};

class CQueuedPacketSender : public CThread, public IQueuedPacketSender {
 public:
  CQueuedPacketSender();
  ~CQueuedPacketSender();

  // IQueuedPacketSender

  bool Setup() override;
  void Shutdown() override;
  bool IsRunning() override { return CThread::IsAlive(); }

  void ClearQueuedPacketsForChannel(INetChannel *pChan) override;
  void QueuePacket(INetChannel *pChan, SOCKET s, const char FAR *buf, int len,
                   const struct sockaddr FAR *to, int tolen,
                   u32 msecDelay) override;

 private:
  // CThread Overrides
  bool Start(u32 nBytesStack = 0) override;
  int Run() override;

 private:
  struct QueuedPacket {
    u32 UnsendTime;
    const void *Channel;  // We don't actually use the channel
    SOCKET Socket;
    CUtlVector<char> To;  // sockaddr
    CUtlVector<char> Buffer;

    // We want the list sorted in ascending order, so note that we return >
    // rather than <
    static bool LessFunc(QueuedPacket *const &lhs, QueuedPacket *const &rhs) {
      return lhs->UnsendTime > rhs->UnsendTime;
    }
  };

  CUtlPriorityQueue<QueuedPacket *> queued_packets_;
  CThreadMutex queued_packets_mutex_;
  CThreadEvent thread_Event_;
  volatile bool should_thread_exit_;
};

static CQueuedPacketSender g_QueuedPacketSender;
IQueuedPacketSender *g_pQueuedPackedSender = &g_QueuedPacketSender;

CQueuedPacketSender::CQueuedPacketSender()
    : queued_packets_{0, 0, QueuedPacket::LessFunc},
      should_thread_exit_{false} {}

CQueuedPacketSender::~CQueuedPacketSender() { Shutdown(); }

bool CQueuedPacketSender::Setup() { return Start(); }

bool CQueuedPacketSender::Start(unsigned nBytesStack) {
  Shutdown();

  const bool is_started{CThread::Start(nBytesStack)};

  if (is_started) {
  // Ahhh the perfect cross-platformness of the threads library.
#ifdef OS_WIN
    SetPriority(THREAD_PRIORITY_HIGHEST);
#elif OS_POSIX
  // SetPriority( PRIORITY_MAX );
#endif

    should_thread_exit_ = false;
  }

  return is_started;
}

void CQueuedPacketSender::Shutdown() {
  if (!IsAlive()) return;

  should_thread_exit_ = true;
  thread_Event_.Set();

  Join();  // Wait for the thread to exit.

  while (queued_packets_.Count() > 0) {
    delete queued_packets_.ElementAtHead();
    queued_packets_.RemoveAtHead();
  }

  queued_packets_.Purge();
}

void CQueuedPacketSender::ClearQueuedPacketsForChannel(INetChannel *channel) {
  queued_packets_mutex_.Lock();

  for (int i = queued_packets_.Count() - 1; i >= 0; i--) {
    QueuedPacket *p = queued_packets_.Element(i);

    if (p->Channel == channel) {
      queued_packets_.RemoveAt(i);
      delete p;
    }
  }

  queued_packets_mutex_.Unlock();
}

void CQueuedPacketSender::QueuePacket(INetChannel *pChan, SOCKET s,
                                      const char FAR *buf, int len,
                                      const struct sockaddr FAR *to, int tolen,
                                      u32 msecDelay) {
  queued_packets_mutex_.Lock();

  // We'll pull all packets we should have sent by now and send them out right
  // away
  const u32 now_milliseconds{Plat_MSTime()};
  const int max_queued_packets{1024};

  if (queued_packets_.Count() < max_queued_packets) {
    // Add this packet to the queue.
    auto *packet = new QueuedPacket{now_milliseconds + msecDelay, pChan, s};
    packet->To.CopyArray((char *)to, tolen);
    packet->Buffer.CopyArray((char *)buf, len);

    queued_packets_.Insert(packet);
  } else {
    static int nWarnings = 5;

    if (--nWarnings > 0) {
      Warning(
          "CQueuedPacketSender: num queued packets >= nMaxQueuedPackets. Not "
          "queueing anymore.\n");
    }
  }

  // Tell the thread that we have a queued packet.
  thread_Event_.Set();
  queued_packets_mutex_.Unlock();
}

extern int NET_SendToImpl(SOCKET s, const char FAR *buf, int len,
                          const struct sockaddr FAR *to, int tolen,
                          int iGameDataLength);

int CQueuedPacketSender::Run() {
  // Normally TT_INFINITE but we wakeup every 50ms just in case.
  const u32 waitIntervalNoPackets{50};
  u32 waitInterval{waitIntervalNoPackets};

  while (true) {
    if (thread_Event_.Wait(waitInterval)) {
      // Someone signaled the thread. Either we're being told to exit or
      // we're being told that a packet was just queued.
      if (should_thread_exit_) return 0;
    }

    // Assume nothing to do and that we'll sleep again
    waitInterval = waitIntervalNoPackets;

    // OK, now send a packet.
    queued_packets_mutex_.Lock();

    // We'll pull all packets we should have sent by now and send them out right
    // away
    const u32 now_milliseconds{Plat_MSTime()};
    const bool do_trace{net_queue_trace.GetInt() ==
                        NET_QUEUED_PACKET_THREAD_DEBUG_VALUE};

    while (queued_packets_.Count() > 0) {
      QueuedPacket *packet = queued_packets_.ElementAtHead();

      if (packet->UnsendTime > now_milliseconds) {
        // Sleep until next we need this packet
        waitInterval = packet->UnsendTime - now_milliseconds;

        if (do_trace) {
          Warning("SQ:  sleeping for %u msecs at %f\n", waitInterval,
                  Plat_FloatTime());
        }
        break;
      }

      // If it's a bot, don't do anything. Note: we DO want this code deep here
      // because bots only try to send packets when sv_stressbots is set, in
      // which case we want it to act as closely as a real player as possible.
      sockaddr_in *pInternetAddr = (sockaddr_in *)packet->To.Base();

#ifdef OS_WIN
      if (pInternetAddr->sin_addr.S_un.S_addr != 0
#else
      if (pInternetAddr->sin_addr.s_addr != 0
#endif
          && pInternetAddr->sin_port != 0) {
        if (do_trace) {
          Warning("SQ:  sending %d bytes at %f\n", packet->Buffer.Count(),
                  Plat_FloatTime());
        }

        NET_SendToImpl(packet->Socket, packet->Buffer.Base(),
                       packet->Buffer.Count(), (sockaddr *)packet->To.Base(),
                       packet->To.Count(), -1);
      }

      delete packet;
      queued_packets_.RemoveAtHead();
    }

    queued_packets_mutex_.Unlock();
  }
}
