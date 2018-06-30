// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#include "tier2/tier2dm.h"

#include "datamodel/idatamodel.h"
#include "dmserializers/idmserializers.h"
#include "tier2/tier2.h"

// Set up methods related to datamodel interfaces

bool ConnectDataModel(CreateInterfaceFn factory) {
  return g_pDataModel->Connect(factory) &&
         g_pDmElementFramework->Connect(factory) &&
         g_pDmSerializers->Connect(factory);
}

InitReturnVal_t InitDataModel() {
  InitReturnVal_t nRetVal = g_pDataModel->Init();

  if (nRetVal == INIT_OK) {
    nRetVal = g_pDmElementFramework->Init();
  }

  if (nRetVal == INIT_OK) {
    nRetVal = g_pDmSerializers->Init();
  }

  return nRetVal;
}

void ShutdownDataModel() {
  g_pDmSerializers->Shutdown();
  g_pDmElementFramework->Shutdown();
  g_pDataModel->Shutdown();
}

void DisconnectDataModel() {
  g_pDmSerializers->Disconnect();
  g_pDmElementFramework->Disconnect();
  g_pDataModel->Disconnect();
}
