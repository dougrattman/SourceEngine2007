// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: random steam class

#include "vstdlib/random.h"

#include "cdll_int.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
//
// implementation of IUniformRandomStream
//
//-----------------------------------------------------------------------------
class CEngineUniformRandomStream : public IUniformRandomStream {
 public:
  // Sets the seed of the random number generator
  void SetSeed(int iSeed) {
    // Never call this from the client or game!
    Assert(0);
  }

  // Generates random numbers
  float RandomFloat(float flMinVal = 0.0f, float flMaxVal = 1.0f) {
    return ::RandomFloat(flMinVal, flMaxVal);
  }

  float RandomFloatExp(float flMinVal = 0.0f, float flMaxVal = 1.0f,
                       float flExponent = 1.0f) {
    return ::RandomFloatExp(flMinVal, flMaxVal, flExponent);
  }

  int RandomInt(int iMinVal, int iMaxVal) {
    return ::RandomInt(iMinVal, iMaxVal);
  }
};

static CEngineUniformRandomStream s_EngineRandomStream;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEngineUniformRandomStream,
                                  IUniformRandomStream,
                                  VENGINE_CLIENT_RANDOM_INTERFACE_VERSION,
                                  s_EngineRandomStream);
