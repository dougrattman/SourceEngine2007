// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef DISPINFO_HELPERS_H
#define DISPINFO_HELPERS_H

#include "disp_defs.h"

// Figure out the max number of vertices and indices for a displacement
// of the specified power.
void CalcMaxNumVertsAndIndices(int power, int *nVerts, int *nIndices);

#endif  // DISPINFO_HELPERS_H
