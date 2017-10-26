// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "disp_helpers.h"

#include "tier0/memdbgon.h"

void CalcMaxNumVertsAndIndices(int power, int *nVerts, int *nIndices) {
  int sideLength = (1 << power) + 1;
  *nVerts = sideLength * sideLength;
  *nIndices = (sideLength - 1) * (sideLength - 1) * 2 * 3;
}
