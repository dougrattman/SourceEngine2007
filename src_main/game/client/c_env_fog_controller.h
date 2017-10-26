// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef C_ENV_FOG_CONTROLLER_H
#define C_ENV_FOG_CONTROLLER_H

#include "playernet_vars.h"

#define CFogController C_FogController

// Compares a set of integer inputs to the one main input
// Outputs true if they are all equivalant, false otherwise
class C_FogController : public C_BaseEntity {
 public:
  DECLARE_NETWORKCLASS();
  DECLARE_CLASS(C_FogController, C_BaseEntity);

  C_FogController();

 public:
  fogparams_t m_fog;
};

#endif  // C_ENV_FOG_CONTROLLER_H
