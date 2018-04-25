// Copyright © 1996-2001, Valve LLC, All rights reserved.

#ifndef ISERVERREFRESHRESPONSE_H
#define ISERVERREFRESHRESPONSE_H

#include "tier1/interface.h"

struct serveritem_t;

// Purpose: Callback interface for server updates
the_interface IServerRefreshResponse {
 public:
  // called when the server has successfully responded
  virtual void ServerResponded(serveritem_t & server) = 0;

  // called when a server response has timed out
  virtual void ServerFailedToRespond(serveritem_t & server) = 0;

  // called when the current refresh list is complete
  virtual void RefreshComplete() = 0;
};

#endif  // ISERVERREFRESHRESPONSE_H
