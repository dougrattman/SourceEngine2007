// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef ENGINESOUNDINTERNAL_H
#define ENGINESOUNDINTERNAL_H

#include "engine/IEngineSound.h"

//-----------------------------------------------------------------------------
// Method to get at the singleton implementations of the engine sound interfaces
//-----------------------------------------------------------------------------
IEngineSound* EngineSoundClient();
IEngineSound* EngineSoundServer();

#endif  // SOUNDENGINEINTERNAL_H
