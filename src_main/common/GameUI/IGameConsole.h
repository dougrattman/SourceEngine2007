// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef IGAMECONSOLE_H
#define IGAMECONSOLE_H

#include "tier1/interface.h"

// Purpose: interface to game/dev console
abstract_class IGameConsole : public IBaseInterface {
 public:
  // activates the console, makes it visible and brings it to the foreground
  virtual void Activate() = 0;

  virtual void Initialize() = 0;

  // hides the console
  virtual void Hide() = 0;

  // clears the console
  virtual void Clear() = 0;

  // return true if the console has focus
  virtual bool IsConsoleVisible() = 0;

  virtual void SetParent(int parent) = 0;
};

#define GAMECONSOLE_INTERFACE_VERSION "GameConsole004"

#endif  // IGAMECONSOLE_H
