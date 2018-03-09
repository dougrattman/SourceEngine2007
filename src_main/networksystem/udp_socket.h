// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include "tier0/include/basetypes.h"
#include "tier1/netadr.h"

class CUtlBuffer;

// Creates a non-blocking UPD socket
class CUDPSocket {
 public:
  CUDPSocket();
  ~CUDPSocket();

  bool Init(unsigned short bindToPort);
  void Shutdown();

  bool RecvFrom(netadr_t &packet_from, CUtlBuffer &data);
  bool SendTo(const netadr_t &recipient, const CUtlBuffer &data);
  bool SendTo(const netadr_t &recipient, const byte *data, size_t datalength);

  bool IsValid() const { return m_Socket != 0; }

 protected:
  bool CreateSocket(void);

  struct CImpl;
  CImpl *m_pImpl;

  netadr_t m_socketIP;
  unsigned int m_Socket;
  unsigned short m_Port;
};

#endif  // UDP_SOCKET_H
