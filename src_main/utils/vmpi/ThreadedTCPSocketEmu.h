// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef THREADEDTCPSOCKETEMU_H
#define THREADEDTCPSOCKETEMU_H

#include "tcpsocket.h"

// This creates a class that's based on IThreadedTCPSocket, but emulates the old
// ITCPSocket interface. This is used for stress-testing IThreadedTCPSocket.
ITCPSocket* CreateTCPSocketEmu();

ITCPListenSocket* CreateTCPListenSocketEmu(const unsigned short port,
                                           int nQueueLength = -1);

#endif  // THREADEDTCPSOCKETEMU_H
