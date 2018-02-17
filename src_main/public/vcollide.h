// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef VCOLLIDE_H
#define VCOLLIDE_H

class CPhysCollide;

struct vcollide_t {
  unsigned short solidCount : 15;
  unsigned short isPacked : 1;
  unsigned short descSize;
  // VPhysicsSolids
  CPhysCollide **solids;
  char *pKeyValues;
};

#endif  // VCOLLIDE_H
