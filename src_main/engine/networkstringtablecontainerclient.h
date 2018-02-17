// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef NETWORKSTRINGTABLECONTAINERCLIENT_H
#define NETWORKSTRINGTABLECONTAINERCLIENT_H

// #include "inetworkstringtableclient.h"
#include "netmessages.h"
#include "networkstringtabledefs.h"

class CUtlBuffer;
class CNetworkStringTable;

//-----------------------------------------------------------------------------
// Purpose: Client implementation of string list
//-----------------------------------------------------------------------------
class CNetworkStringTableContainerClient : public INetworkStringTableContainer {
 public:
  // Construction
  CNetworkStringTableContainerClient(void);
  ~CNetworkStringTableContainerClient(void);

 public:
  // implemenatation INetworkStringTableContainer
  virtual INetworkStringTable *CreateStringTable(const char *tableName,
                                                 int maxentries);
  virtual void RemoveAllTables(void);

  virtual INetworkStringTable *FindTable(const char *tableName) const;
  virtual INetworkStringTable *GetTable(TABLEID stringTable) const;
  virtual int GetNumTables(void) const;

  // Print contents to console
  void Dump(void);

  void WriteStringTables(CUtlBuffer &buf);
  bool ReadStringTables(CUtlBuffer &buf);

 private:
  CUtlVector<CNetworkStringTable *> m_Tables;
};

extern CNetworkStringTableContainerClient *networkStringTableContainerClient;

#endif  // NETWORKSTRINGTABLECONTAINERCLIENT_H
