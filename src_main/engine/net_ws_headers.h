// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef NET_WS_HEADERS_H
#define NET_WS_HEADERS_H

#ifdef _WIN32
#include "base/include/windows/windows_light.h"
#endif

#include "convar.h"
#include "deps/bzip2/bzlib.h"
#include "filesystem_engine.h"
#include "inetmsghandler.h"
#include "matchmaking.h"
#include "net_chan.h"
#include "proto_oob.h"
#include "protocol.h"  // CONNECTIONLESS_HEADER
#include "sv_filter.h"
#include "sys.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/tslist.h"
#include "tier0/include/vcrmode.h"
#include "tier1/mempool.h"
#include "vstdlib/random.h"

#if defined(_WIN32)

#if !defined(_X360)
#include <winsock.h>
#endif

// #include <process.h>
typedef int socklen_t;

#else

#include <arpa/inet.h>
#include <errno.h>
#include <linux/tcp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEMSGSIZE EMSGSIZE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAECONNRESET ECONNRESET
#define WSAECONNREFUSED ECONNREFUSED
#define WSAEADDRINUSE EADDRINUSE
#define WSAENOTCONN ENOTCONN

#define ioctlsocket ioctl
#define closesocket close

#undef SOCKET
typedef int SOCKET;
#define FAR

#endif

#include "sv_rcon.h"
#ifndef SWDS
#include "cl_rcon.h"
#endif

#endif  // NET_WS_HEADERS_H
