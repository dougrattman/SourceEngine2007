// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DISP_VRAD_H
#define DISP_VRAD_H

#include "builddisp.h"

// Blend the normals of neighboring displacement surfaces so they match at edges
// and corners.
void SmoothNeighboringDispSurfNormals(CCoreDispInfo **ppListBase, int listSize);

#endif  // DISP_VRAD_H
