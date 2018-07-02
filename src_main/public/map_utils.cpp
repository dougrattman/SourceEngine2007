// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "map_utils.h"

#include "bspfile.h"
#include "mathlib/vector.h"

#include "tier0/include/memdbgon.h"

void SetupLightNormalFromProps(const QAngle &angles, float angle, float pitch,
                               Vector &output) {
  if (angle == ANGLE_UP) {
    output[0] = output[1] = 0.0f;
    output[2] = 1;
  } else if (angle == ANGLE_DOWN) {
    output[0] = output[1] = 0.0f;
    output[2] = -1;
  } else {
    // if we don't have a specific "angle" use the "angles" YAW
    if (!angle) angle = angles[YAW];

    const f32 angle_rad{DEG2RAD(angle)};

    output[2] = 0.0f;
    output[0] = cos(angle_rad);
    output[1] = sin(angle_rad);
  }

  // if we don't have a specific "pitch" use the "angles" PITCH
  if (!pitch) pitch = angles[PITCH];

  const f32 pitch_rad{DEG2RAD(pitch)};

  output[2] = sin(pitch_rad);
  output[0] *= cos(pitch_rad);
  output[1] *= cos(pitch_rad);
}
