// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#ifndef SOURCE_TIER1_TIER1_H_
#define SOURCE_TIER1_TIER1_H_

#include "appframework/IAppSystem.h"
#include "tier1/convar.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ICvar;
class IProcessUtils;

//-----------------------------------------------------------------------------
// These tier1 libraries must be set by any users of this library.
// They can be set by calling ConnectTier1Libraries.
// It is hoped that setting this, and using this library will be the common
// mechanism for allowing link libraries to access tier1 library interfaces
//-----------------------------------------------------------------------------
extern ICvar *cvar;
extern ICvar *g_pCVar;
extern IProcessUtils *g_pProcessUtils;

//-----------------------------------------------------------------------------
// Call this to connect to/disconnect from all tier 1 libraries.
// It's up to the caller to check the globals it cares about to see if ones are
// missing
//-----------------------------------------------------------------------------
void ConnectTier1Libraries(CreateInterfaceFn *pFactoryList, int nFactoryCount);
void DisconnectTier1Libraries();

//-----------------------------------------------------------------------------
// Helper empty implementation of an IAppSystem for tier2 libraries
//-----------------------------------------------------------------------------
template <class IInterface, int ConVarFlag = 0>
class CTier1AppSystem : public CTier0AppSystem<IInterface> {
  typedef CTier0AppSystem<IInterface> BaseClass;

 public:
  CTier1AppSystem(bool bIsPrimaryAppSystem = true)
      : BaseClass(bIsPrimaryAppSystem) {}

  bool Connect(CreateInterfaceFn factory) override {
    if (!BaseClass::Connect(factory)) return false;

    if (this->IsPrimaryAppSystem()) {
      ConnectTier1Libraries(&factory, 1);
    }
    return true;
  }

  void Disconnect() override {
    if (this->IsPrimaryAppSystem()) {
      DisconnectTier1Libraries();
    }
    BaseClass::Disconnect();
  }

  InitReturnVal_t Init() override {
    InitReturnVal_t nRetVal = BaseClass::Init();
    if (nRetVal != INIT_OK) return nRetVal;

    if (g_pCVar && this->IsPrimaryAppSystem()) {
      ConVar_Register(ConVarFlag);
    }
    return INIT_OK;
  }

  void Shutdown() override {
    if (g_pCVar && this->IsPrimaryAppSystem()) {
      ConVar_Unregister();
    }
    BaseClass::Shutdown();
  }
};

#endif  // SOURCE_TIER1_TIER1_H_
