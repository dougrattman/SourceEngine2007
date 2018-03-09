// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IHAMMER_H
#define IHAMMER_H

#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"

typedef struct tagMSG MSG;

class IStudioDataCache;

// Return values for RequestNewConfig
enum RequestRetval_t { REQUEST_OK = 0, REQUEST_QUIT };

// Interface used to drive hammer
the_interface IHammer : public IAppSystem {
 public:
  virtual bool HammerPreTranslateMessage(MSG * pMsg) = 0;
  virtual bool HammerIsIdleMessage(MSG * pMsg) = 0;
  virtual bool HammerOnIdle(long count) = 0;

  virtual void RunFrame() = 0;

  // Returns the mod and the game to initially start up
  virtual const ch *GetDefaultMod() = 0;
  virtual const ch *GetDefaultGame() = 0;

  virtual bool InitSessionGameConfig(const ch *szGameDir) = 0;

  // Request a new config from hammer's config system
  virtual RequestRetval_t RequestNewConfig() = 0;

  // Returns the full path to the mod and the game to initially start up
  virtual const ch *GetDefaultModFullPath() = 0;

  virtual i32 MainLoop() = 0;
};

#define INTERFACEVERSION_HAMMER "Hammer001"

#endif  // IHAMMER_H
