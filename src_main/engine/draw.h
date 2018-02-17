// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:	Some little utility drawing methods

#ifndef DRAW_H
#define DRAW_H

class IMaterial;
IMaterial *GL_LoadMaterial(const char *pName, const char *pTextureGroupName);
void GL_UnloadMaterial(IMaterial *pMaterial);

#endif  // DRAW_H
