// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef LOOPBACK_CHANNEL_H
#define LOOPBACK_CHANNEL_H

#include "ichannel.h"

// Loopback sockets receive the same data they send.
IChannel* CreateLoopbackChannel();

#endif  // LOOPBACK_CHANNEL_H
