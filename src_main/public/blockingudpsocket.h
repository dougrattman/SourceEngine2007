// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BLOCKINGUDPSOCKET_H
#define BLOCKINGUDPSOCKET_H

#include "base/include/base_types.h"
#include "tier1/netadr.h"

struct sockaddr_in;

class CBlockingUDPSocket {
 public:
  explicit CBlockingUDPSocket();
  virtual ~CBlockingUDPSocket();

  bool WaitForMessage(f32 timeout_seconds);
  u32 ReceiveSocketMessage(sockaddr_in *packet_from, u8 *buffer, usize size);
  u32 SendSocketMessage(const sockaddr_in &packet_to, const u8 *buffer,
                        usize size);

  bool IsValid() const { return m_Socket != 0; }

 protected:
  int CreateSocket();

  struct CImpl;
  CImpl *m_pImpl;

  netadr_t m_cserIP;
#ifdef OS_WIN
  uintptr_t m_Socket;
#elif OS_POSIX
  int m_Socket;
#endif  // OS_WIN
};

#endif  // BLOCKINGUDPSOCKET_H
