// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Simple TCP socket API for communicating as a TCP client over a TEXT
// connection

#ifndef SOURCE_SMTPMAIL_SIMPLE_SOCKET_H_
#define SOURCE_SMTPMAIL_SIMPLE_SOCKET_H_

// opaque socket type
using HSOCKET = struct socket_s *;

// Socket reporting function
using REPORTFUNCTION = void (*)(HSOCKET socket, const char *pString);

extern HSOCKET SocketOpen(const char *pServerName, unsigned short port);
extern void SocketClose(HSOCKET socket);
extern void SocketSendString(HSOCKET socket, const char *pString);

// This should probably return the data so we can handle errors and receive data
extern void SocketWait(HSOCKET socket, const char *pString);

// sets the reporting function
extern void SocketReport(REPORTFUNCTION pReportFunction);

#endif  // SOURCE_SMTPMAIL_SIMPLE_SOCKET_H_
