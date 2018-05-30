// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VCOLLIDE_H
#define VCOLLIDE_H

class CPhysCollide;

struct vcollide_t {
  u16 solidCount : 15;
  u16 isPacked : 1;
  u16 descSize;
  // VPhysicsSolids
  CPhysCollide **solids;
  char *pKeyValues;
};

#endif  // VCOLLIDE_H
