// Copyright © 2005-2005, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#ifndef SOURCE_TIER3_TIER3DM_H_
#define SOURCE_TIER3_TIER3DM_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier2/tier2dm.h"
#include "tier3/tier3.h"

// Helper empty implementation of an IAppSystem for tier2 libraries
template <typename IInterface, int ConVarFlag = 0>
class CTier3DmAppSystem : public CTier2DmAppSystem<IInterface, ConVarFlag> {
  using BaseClass = CTier2DmAppSystem<IInterface, ConVarFlag>;

 public:
  CTier3DmAppSystem(bool bIsPrimaryAppSystem = true)
      : BaseClass{bIsPrimaryAppSystem} {}

  bool Connect(CreateInterfaceFn factory) override {
    if (BaseClass::Connect(factory)) {
      if (this->IsPrimaryAppSystem()) {
        ConnectTier3Libraries(&factory, 1);
      }
      return true;
    }

    return false;
  }

  void Disconnect() override {
    if (this->IsPrimaryAppSystem()) {
      DisconnectTier3Libraries();
    }

    BaseClass::Disconnect();
  }
};

#endif  // SOURCE_TIER3_TIER3DM_H_
