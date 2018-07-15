// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "render_pch.h"

#include "gl_shader.h"

#include "materialproxyfactory.h"
#include "materialsystem/imaterialsystem.h"
#include "sysexternal.h"
#include "tier0/include/icommandline.h"
#include "tier2/tier2.h"

#include "tier0/include/memdbgon.h"

namespace {
static CMaterialProxyFactory s_MaterialProxyFactory;

// Connects to other
bool Shader_ConnectTheRest() {
  // NOTE: These two interfaces have been connected in the tier2 library
  if (!g_pMaterialSystemHardwareConfig) {
    Shader_Disconnect();
    Sys_Error("Connection Material System Hardware Config failure.");
    return false;
  }

  if (!g_pMaterialSystemDebugTextureInfo) {
    Shader_Disconnect();
    Sys_Error("Connection Material System Debug Texture Info failure.");
    return false;
  }

  return true;
}
}  // namespace

// Connect to interfaces we need
bool Shader_Connect(bool do_set_proxy_factory) {
  if (materials) {
    const int adapter_idx{CommandLine()->ParmValue("-adapter", 0)};
    int material_mode_flags = MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;

    if (CommandLine()->FindParm("-ref")) {
      material_mode_flags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
    }

    materials->SetAdapter(adapter_idx, material_mode_flags);

    if (do_set_proxy_factory) {
      materials->SetMaterialProxyFactory(&s_MaterialProxyFactory);
    }

    return Shader_ConnectTheRest();
  }

  return false;
}

void Shader_Disconnect() {}

#ifndef SWDS
void Shader_SwapBuffers() {
  assert(materials);

  materials->SwapBuffers();
}

void Shader_BeginRendering() {}
#endif
