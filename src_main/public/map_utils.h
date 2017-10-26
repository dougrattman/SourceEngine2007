// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef MAP_UTILS_H
#define MAP_UTILS_H

#include "mathlib/vector.h"

// angles comes from the "angles" property
//
// yaw and pitch will override the values in angles if they are nonzero
// yaw comes from the (obsolete) "angle" property
// pitch comes from the "pitch" property
void SetupLightNormalFromProps(const QAngle &angles, float yaw, float pitch,
                               Vector &output);

#endif  // MAP_UTILS_H
