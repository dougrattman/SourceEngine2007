// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "networkstringtableserver.h"

#include "LocalNetworkBackdoor.h"
#include "demo.h"
#include "eiface.h"
#include "framesnapshot.h"
#include "host.h"
#include "networkstringtable.h"
#include "networkstringtableitem.h"
#include "quakedef.h"
#include "server.h"
#include "tier1/UtlVector.h"
#include "tier1/utlsymbol.h"
#include "utlrbtree.h"

 
#include "tier0/include/memdbgon.h"

static CNetworkStringTableContainer s_NetworkStringTableServer;
CNetworkStringTableContainer *networkStringTableContainerServer =
    &s_NetworkStringTableServer;

// Expose interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainerServer,
                                  INetworkStringTableContainer,
                                  INTERFACENAME_NETWORKSTRINGTABLESERVER,
                                  s_NetworkStringTableServer);

#ifdef SHARED_NET_STRING_TABLES
// Expose same interface to client .dll as client string tables
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainer,
                                  INetworkStringTableContainer,
                                  INTERFACENAME_NETWORKSTRINGTABLECLIENT,
                                  s_NetworkStringTableServer);
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SV_CreateNetworkStringTables() {
  // Remove any existing tables
  s_NetworkStringTableServer.RemoveAllTables();

  // Unset timing guard and create tables
  s_NetworkStringTableServer.AllowCreation(true);

  // Create engine tables
  sv.CreateEngineStringTables();

  // Create game code tables
  serverGameDLL->CreateNetworkStringTables();

  s_NetworkStringTableServer.AllowCreation(false);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SV_PrintStringTables() { s_NetworkStringTableServer.Dump(); }
