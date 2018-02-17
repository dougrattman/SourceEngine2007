// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Main header file for the serializers DLL

#ifndef IDMSERIALIZERS_H
#define IDMSERIALIZERS_H

#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// Interface
//-----------------------------------------------------------------------------
class IDmSerializers : public IAppSystem {};

//-----------------------------------------------------------------------------
// Used only by applications to hook in DmSerializers
//-----------------------------------------------------------------------------
#define DMSERIALIZERS_INTERFACE_VERSION "VDmSerializers001"

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern IDmSerializers *g_pDmSerializers;

#endif  // DMSERIALIZERS_H
