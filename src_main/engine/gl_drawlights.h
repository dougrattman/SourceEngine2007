// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef GL_DRAWLIGHTS_H
#define GL_DRAWLIGHTS_H

// Should we draw light sprites over visible lights?
bool ActivateLightSprites(bool bActive);

// Draws sprites over all visible lights
void DrawLightSprites(void);

// Draws lighting debugging information
void DrawLightDebuggingInfo(void);

float ComputeLightRadius(dworldlight_t *pLight, bool bIsHDR);

#endif  // GL_DRAWLIGHTS_H
