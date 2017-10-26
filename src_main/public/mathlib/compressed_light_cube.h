// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_
#define SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_

#include "mathlib/mathlib.h"

struct CompressedLightCube {
  DECLARE_BYTESWAP_DATADESC();
  ColorRGBExp32 m_Color[6];
};

#endif  // SOURCE_MATHLIB_COMPRESSED_LIGHT_CUBE_H_
