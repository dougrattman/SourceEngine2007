// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "render_pch.h"

#include "gl_shader.h"

#include "common.h"
#include "const.h"
#include "decal_private.h"
#include "editor_sendcommand.h"
#include "enginestats.h"
#include "filesystem.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "gl_matsysiface.h"
#include "gl_model_private.h"
#include "iregistry.h"
#include "ivideomode.h"
#include "materialproxyfactory.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "modes.h"
#include "r_local.h"
#include "sys_dll.h"
#include "tier0/include/icommandline.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"
#include "traceinit.h"
#include "vmodes.h"

#ifndef HWND
#define HWND int
#endif

#include "igame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

static CMaterialProxyFactory s_MaterialProxyFactory;

// Connects to other
static void Shader_ConnectOtherMatSysInterfaces() {
  // NOTE: These two interfaces have been connected in the tier2 library
  if (!g_pMaterialSystemHardwareConfig) {
    Shader_Disconnect();
    Sys_Error(
        "Could not get the material system hardware config interface! (2)");
  }

  if (!g_pMaterialSystemDebugTextureInfo) {
    Shader_Disconnect();
    Sys_Error("Could not get the debug texture info interface!");
  }
}

// Connect to interfaces we need
bool Shader_Connect(bool bSetProxyFactory) {
  if (!materials) return false;

  int nAdapter = CommandLine()->ParmValue("-adapter", 0);
  int nModeFlags = MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
  if (CommandLine()->FindParm("-ref")) {
    nModeFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
  }

  materials->SetAdapter(nAdapter, nModeFlags);
  if (bSetProxyFactory)
    materials->SetMaterialProxyFactory(&s_MaterialProxyFactory);

  Shader_ConnectOtherMatSysInterfaces();
  return g_pMaterialSystemHardwareConfig && g_pMaterialSystemDebugTextureInfo;
}

// Connect to interfaces we need
void Shader_Disconnect() {}

#ifndef SWDS
void Shader_SwapBuffers() {
  assert(materials);
  materials->SwapBuffers();
}

void Shader_BeginRendering() {}
#endif
