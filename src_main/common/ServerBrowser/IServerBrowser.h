// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ISERVERBROWSER_H
#define ISERVERBROWSER_H

#include <cstdint>
#include "tier1/interface.h"

// Purpose: Interface to server browser module
abstract_class IServerBrowser {
 public:
  // activates the server browser window, brings it to the foreground
  virtual bool Activate() = 0;
  // joins a game directly
  virtual bool JoinGame(uint32_t unGameIP, uint16_t usGamePort) = 0;
  // joins a specified game - game info dialog will only be opened if the server
  // is fully or passworded
  virtual bool JoinGame(uint64_t ulSteamIDFriend) = 0;
  // opens a game info dialog to watch the specified server; associated with the
  // friend 'userName'
  virtual bool OpenGameInfoDialog(uint64_t ulSteamIDFriend) = 0;
  // forces the game info dialog closed
  virtual void CloseGameInfoDialog(uint64_t ulSteamIDFriend) = 0;
  // closes all the game info dialogs
  virtual void CloseAllGameInfoDialogs() = 0;
};

#define SERVERBROWSER_INTERFACE_VERSION "ServerBrowser003"

#endif  // ISERVERBROWSER_H
