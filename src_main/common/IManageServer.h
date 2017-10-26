// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef IMANAGESERVER_H
#define IMANAGESERVER_H

#include "tier1/interface.h"

// Purpose: basic callback interface for the manage server list, to update
// status text et al
abstract_class IManageServer : public IBaseInterface {
 public:
  // activates the manage page
  virtual void ShowPage() = 0;
  // sets whether or not the server is remote
  virtual void SetAsRemoteServer(bool remote) = 0;
  // prints text to the console
  virtual void AddToConsole(const char *msg) = 0;
};

#define IMANAGESERVER_INTERFACE_VERSION "IManageServer002"

#endif  // IMANAGESERVER_H
