// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DECAL_H
#define DECAL_H

#include "draw.h"
#include "gl_model_private.h"

void Decal_Init(void);
void Decal_Shutdown(void);
IMaterial *Draw_DecalMaterial(int index);
int Draw_DecalIndexFromName(char *name, bool *found);
const char *Draw_DecalNameFromIndex(int index);
void Draw_DecalSetName(int decal, char *name);
void R_DecalShoot(int textureIndex, int entity, const model_t *model,
                  const Vector &position, const float *saxis, int flags,
                  const color32 &rgbaColor);
int Draw_DecalMax(void);

#endif  // DECAL_H
