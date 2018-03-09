// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "cbase.h"

#include "BaseAnimatedTextureProxy.h"

 
#include "tier0/include/memdbgon.h"

class CAnimatedTextureProxy : public CBaseAnimatedTextureProxy {
 public:
  CAnimatedTextureProxy() {}
  virtual ~CAnimatedTextureProxy() {}
  virtual float GetAnimationStartTime(void* pBaseEntity);
};

EXPOSE_INTERFACE(CAnimatedTextureProxy, IMaterialProxy,
                 "AnimatedTexture" IMATERIAL_PROXY_INTERFACE_VERSION);

float CAnimatedTextureProxy::GetAnimationStartTime(void* pBaseEntity) {
  return 0;
}
