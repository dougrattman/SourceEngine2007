// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IGAMECLIENTEXPORTS_H
#define IGAMECLIENTEXPORTS_H

#include "tier1/interface.h"

//-----------------------------------------------------------------------------
// Purpose: Exports a set of functions for the GameUI interface to interact with
// the game client
//-----------------------------------------------------------------------------
abstract_class IGameClientExports : public IBaseInterface {
 public:
#ifndef _XBOX
  // ingame voice manipulation
  virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
  virtual void MutePlayerGameVoice(int playerIndex) = 0;
  virtual void UnmutePlayerGameVoice(int playerIndex) = 0;
#endif
};

#define GAMECLIENTEXPORTS_INTERFACE_VERSION "GameClientExports001"

#endif  // IGAMECLIENTEXPORTS_H
