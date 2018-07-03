// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Wraps pointers to basic vgui interfaces.

#ifndef SOURCE_VGUI_INTERNAL_H_
#define SOURCE_VGUI_INTERNAL_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "tier3/tier3.h"
#include "vgui/VGUI.h"

namespace vgui {
bool VGui_InternalLoadInterfaces(CreateInterfaceFn *factoryList,
                                 int numFactories);

// <vgui/IInputInternal.h> header
extern class IInputInternal *g_pInput;

// <vgui/IScheme.h> header
extern class ISchemeManager *g_pScheme;

// <vgui/ISurface.h> header
extern class ISurface *g_pSurface;

// <vgui/ISystem.h> header
extern class ISystem *g_pSystem;

// <vgui/IVGui.h> header
extern class IVGui *g_pIVgui;

// <vgui/IPanel.h> header
extern class IPanel *g_pIPanel;

// methods
void vgui_strcpy(char *dst, int dstLen, const char *src);
}  // namespace vgui

#endif  // SOURCE_VGUI_INTERNAL_H_
