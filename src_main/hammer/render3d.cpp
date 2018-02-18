// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "Render3D.h"
#include "Render3DMS.h"
#include "base/include/windows/windows_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

CRender3D *CreateRender3D(Render3DType_t eRender3DType) {
  switch (eRender3DType) {
    case Render3DTypeMaterialSystem: {
      return (new CRender3DMS());
    }
  }

  return NULL;
}
