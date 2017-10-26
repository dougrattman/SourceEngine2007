// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Font effects that operate on linear rgba data.

#ifndef SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTEFFECTS_H_
#define SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTEFFECTS_H_

void ApplyScanlineEffectToTexture(int rgbaWide, int rgbaTall,
                                  unsigned char *rgba, int iScanLines);
void ApplyGaussianBlurToTexture(int rgbaWide, int rgbaTall, unsigned char *rgba,
                                int iBlur);
void ApplyDropShadowToTexture(int rgbaWide, int rgbaTall, unsigned char *rgba,
                              int iDropShadowOffset);
void ApplyOutlineToTexture(int rgbaWide, int rgbaTall, unsigned char *rgba,
                           int iOutlineSize);
void ApplyRotaryEffectToTexture(int rgbaWide, int rgbaTall, unsigned char *rgba,
                                bool bRotary);

#endif  // SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTEFFECTS_H_