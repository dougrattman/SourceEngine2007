// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Simple TCP socket API for communicating as a TCP client over a TEXT
// connection

#include "simple_socket.h"

#include <winsock.h>
#include <cstdio>

#include "base/include/windows/windows_errno_info.h"

static REPORTFUNCTION g_SocketReport{nullptr};

// Purpose: sets up a reporting function.
void SocketReport(REPORTFUNCTION pReportFunction) {
  g_SocketReport = pReportFunction;
}

// Opens a TCP socket & connect to a given server.
HSOCKET SocketOpen(const char *server_name, unsigned short port_number) {
  SOCKET s{socket(AF_INET, SOCK_STREAM, IPPROTO_IP)};
  if (s == INVALID_SOCKET) {
    fprintf(stderr, "open socket error: %s.\n",
            source::windows::make_windows_errno_info(
                source::windows::win32_to_windows_errno_code(WSAGetLastError()))
                .description);

    return nullptr;
  }

  SOCKADDR_IN sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = inet_addr(server_name);

  if (sock_addr.sin_addr.s_addr == INADDR_NONE) {
    hostent *host = gethostbyname(server_name);

    if (host != nullptr) {
      sock_addr.sin_addr.s_addr = ((IN_ADDR *)host->h_addr)->s_addr;
    } else {
      fprintf(
          stderr, "gethostbyname '%s' error: %s.\n", server_name,
          source::windows::make_windows_errno_info(
              source::windows::win32_to_windows_errno_code(WSAGetLastError()))
              .description);

      return nullptr;
    }
  }

  sock_addr.sin_port = htons((u_short)port_number);

  if (connect(s, (SOCKADDR *)&sock_addr, sizeof(sock_addr)) == SOCKET_ERROR) {
    fprintf(stderr, "socket connect to '%s:%hu' error: %s.\n", server_name,
            port_number,
            source::windows::make_windows_errno_info(
                source::windows::win32_to_windows_errno_code(WSAGetLastError()))
                .description);

    return nullptr;
  }

  return (HSOCKET)s;
}

// Closes the socket opened with SocketOpen().
void SocketClose(HSOCKET socket) {
  if (closesocket((SOCKET)socket) == SOCKET_ERROR) {
    fprintf(stderr, "closesocket error: %s.\n",
            source::windows::make_windows_errno_info(
                source::windows::win32_to_windows_errno_code(WSAGetLastError()))
                .description);
  }
}

// Writes a string to the socket.  String is nullptr terminated on
// input, but terminator is NOT written to the socket.
void SocketSendString(HSOCKET socket, const char *string) {
  if (!string) return;

  const std::size_t length{strlen(string)};

  if (!length) return;

  if (send((SOCKET)socket, string, length, 0) != SOCKET_ERROR) {
    if (g_SocketReport) g_SocketReport(socket, string);
  } else {
    fprintf(stderr, "send '%s' failed: %s.\n", string,
            source::windows::make_windows_errno_info(
                source::windows::win32_to_windows_errno_code(WSAGetLastError()))
                .description);
  }
}

// Receives input from a socket until a certain string is received.  Assume
// socket data is all text.
void SocketWait(HSOCKET socket, const char *string) {
  char buffer[1024];
  bool done{false};

  while (!done) {
    const int bytes_received_num{
        recv((SOCKET)socket, buffer, sizeof(buffer) - 1, 0)};

    if (bytes_received_num == SOCKET_ERROR) {
      fprintf(
          stderr, "recv error, continue to wait: %s.\n",
          source::windows::make_windows_errno_info(
              source::windows::win32_to_windows_errno_code(WSAGetLastError()))
              .description);
    } else {
      buffer[bytes_received_num] = '\0';

      if (g_SocketReport) g_SocketReport(socket, buffer);

      if (!string || strstr(buffer, string)) return;
    }
  }
}
