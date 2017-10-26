// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER1_NETADR_H_
#define SOURCE_TIER1_NETADR_H_

#include "tier0/platform.h"
#undef SetPort

typedef enum {
  NA_NULL = 0,
  NA_LOOPBACK,
  NA_BROADCAST,
  NA_IP,
} netadrtype_t;

typedef struct netadr_s {
 public:
  netadr_s() {
    SetIP(0);
    SetPort(0);
    SetType(NA_IP);
  }
  netadr_s(uint32_t unIP, uint16_t usPort) {
    SetIP(unIP);
    SetPort(usPort);
    SetType(NA_IP);
  }
  netadr_s(const char *pch) { SetFromString(pch); }
  void Clear();  // invalids Address

  void SetType(netadrtype_t type);
  void SetPort(unsigned short port);
  bool SetFromSockadr(const struct sockaddr *s);
  void SetIP(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
  void SetIP(uint32_t unIP);  // Sets IP.  unIP is in host order (little-endian)
  void SetIPAndPort(uint32_t unIP, unsigned short usPort) {
    SetIP(unIP);
    SetPort(usPort);
  }
  void SetFromString(const char *pch, bool bUseDNS = false);  // if bUseDNS is
                                                              // true then do a
                                                              // DNS lookup if
                                                              // needed

  bool CompareAdr(const netadr_s &a, bool onlyBase = false) const;
  bool CompareClassBAdr(const netadr_s &a) const;
  bool CompareClassCAdr(const netadr_s &a) const;

  netadrtype_t GetType() const;
  unsigned short GetPort() const;
  const char *ToString(
      bool onlyBase = false) const;  // returns xxx.xxx.xxx.xxx:ppppp
  void ToSockadr(struct sockaddr *s) const;
  unsigned int GetIP() const;

  bool IsLocalhost() const;    // true, if this is the localhost IP
  bool IsLoopback() const;     // true if engine loopback buffers are used
  bool IsReservedAdr() const;  // true, if this is a private LAN IP
  bool IsValid() const;        // ip & port != 0
  void SetFromSocket(int hSocket);
  // These function names are decorated because the Xbox360 defines macros for
  // ntohl and htonl
  unsigned long addr_ntohl() const;
  unsigned long addr_htonl() const;
  bool operator==(const netadr_s &netadr) const { return (CompareAdr(netadr)); }
  bool operator<(const netadr_s &netadr) const;

 public:  // members are public to avoid to much changes
  netadrtype_t type;
  unsigned char ip[4];
  unsigned short port;
} netadr_t;

#endif  // SOURCE_TIER1_NETADR_H_
