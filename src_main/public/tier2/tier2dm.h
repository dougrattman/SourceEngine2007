// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#ifndef SOURCE_TIER2_TIER2DM_H_
#define SOURCE_TIER2_TIER2DM_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier2/tier2.h"

// Set up methods related to datamodel interfaces

bool ConnectDataModel(CreateInterfaceFn factory);
InitReturnVal_t InitDataModel();
void ShutdownDataModel();
void DisconnectDataModel();

// Helper empty implementation of an IAppSystem for tier2 libraries.
template <typename IInterface, int ConVarFlag = 0>
class CTier2DmAppSystem : public CTier2AppSystem<IInterface, ConVarFlag> {
  using BaseClass = CTier2AppSystem<IInterface, ConVarFlag>;

 public:
  CTier2DmAppSystem(bool bIsPrimaryAppSystem = true)
      : BaseClass(bIsPrimaryAppSystem) {}

  bool Connect(CreateInterfaceFn factory) override {
    if (BaseClass::Connect(factory)) {
      ConnectDataModel(factory);

      return true;
    }

    return false;
  }

  InitReturnVal_t Init() override {
    InitReturnVal_t return_val{BaseClass::Init()};

    if (return_val == INIT_OK) {
      return_val = InitDataModel();
    }

    return return_val;
  }

  void Shutdown() override {
    ShutdownDataModel();
    BaseClass::Shutdown();
  }

  void Disconnect() override {
    DisconnectDataModel();
    BaseClass::Disconnect();
  }
};

#endif  // SOURCE_TIER2_TIER2DM_H_
