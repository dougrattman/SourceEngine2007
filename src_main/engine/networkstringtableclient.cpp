// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "networkstringtableclient.h"

#include "client.h"
#include "networkstringtable.h"
#include "quakedef.h"

#include "tier0/include/memdbgon.h"

#ifndef SHARED_NET_STRING_TABLES
static CNetworkStringTableContainer s_NetworkStringTableClient;
CNetworkStringTableContainer *networkStringTableContainerClient =
    &s_NetworkStringTableClient;

// Expose to client .dll
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainer,
                                  INetworkStringTableContainer,
                                  INTERFACENAME_NETWORKSTRINGTABLECLIENT,
                                  s_NetworkStringTableClient);
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CL_PrintStringTables(void) {
  if (cl.m_StringTableContainer) {
    cl.m_StringTableContainer->Dump();
  }
}
