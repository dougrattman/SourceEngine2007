// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "blockingudpsocket.h"

#include <cassert>

#include "base/include/macros.h"
#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"

#include <winsock.h>

inline int getsocketerrno() { return WSAGetLastError(); }
#elif OS_POSIX
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define closesocket close
inline int getsocketerrno() { return errno; }
#endif  // OS_WIN

#include "tier0/include/vcrmode.h"

struct CBlockingUDPSocket::CImpl {
  sockaddr_in socketIp;
  fd_set fdSet;
};

CBlockingUDPSocket::CBlockingUDPSocket()
    : m_pImpl{new CImpl}, m_cserIP{}, m_Socket{0} {
  [[maybe_unused]] const int error_code { CreateSocket() };
  assert(error_code != SOCKET_ERROR);
}

CBlockingUDPSocket::~CBlockingUDPSocket() {
  [[maybe_unused]] const int error_code { closesocket(m_Socket) };
  assert(error_code != SOCKET_ERROR);

  delete m_pImpl;
}

int CBlockingUDPSocket::CreateSocket() {
  m_Socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  bool can_create{m_Socket != INVALID_SOCKET};

  if (can_create) {
    sockaddr_in address = m_pImpl->socketIp;
    can_create =
        bind(m_Socket, (sockaddr *)&address, sizeof(address)) != SOCKET_ERROR;
  }

  if (can_create) {
#ifdef OS_WIN
    if (m_pImpl->socketIp.sin_addr.S_un.S_addr == INADDR_ANY) {
      m_pImpl->socketIp.sin_addr.S_un.S_addr = 0L;
    }
#elif OS_POSIX
    if (m_pImpl->socketIp.sin_addr.s_addr == INADDR_ANY) {
      m_pImpl->socketIp.sin_addr.s_addr = 0L;
    }
#endif
  }

  return can_create ? 0 : getsocketerrno();
}

bool CBlockingUDPSocket::WaitForMessage(f32 timeout_seconds) {
  FD_ZERO(&m_pImpl->fdSet);
  FD_SET(m_Socket, &m_pImpl->fdSet);  // lint !e717

  timeval tv{0};
  tv.tv_sec = (long)timeout_seconds;
  tv.tv_usec = (long)((timeout_seconds - (long)timeout_seconds) * 1000000 +
                      0.5f); /* micro seconds */

  if (SOCKET_ERROR ==
      select(m_Socket + 1, &m_pImpl->fdSet, nullptr, nullptr, &tv)) {
    return false;
  }

  return FD_ISSET(m_Socket, &m_pImpl->fdSet);
}

u32 CBlockingUDPSocket::ReceiveSocketMessage(sockaddr_in *packet_from,
                                             u8 *buffer, usize size) {
  if (!packet_from) return 0u;

  memset(packet_from, 0, sizeof(*packet_from));

  sockaddr from_address;
  i32 from_size = sizeof(from_address);
  i32 bytes_received = VCRHook_recvfrom(m_Socket, (char *)buffer, size, 0,
                                        &from_address, &from_size);

  if (bytes_received != SOCKET_ERROR) {
    // In case it's parsed as a string.
    buffer[bytes_received] = 0;
    // Copy over the received address.
    *packet_from = bit_cast<sockaddr_in>(from_address);

    return implicit_cast<u32>(bytes_received);
  }

  return 0u;
}

u32 CBlockingUDPSocket::SendSocketMessage(const sockaddr_in &packet_to,
                                          const u8 *buffer, usize size) {
  const i32 bytes_sent =
      sendto(m_Socket, (const char *)buffer, size, 0,
             reinterpret_cast<const sockaddr *>(&packet_to), sizeof(packet_to));
  return bytes_sent != SOCKET_ERROR ? implicit_cast<u32>(bytes_sent) : 0u;
}
