// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A higher level link library for general use in the game and tools.

#include "tier3/tier3.h"

#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "movieobjects/idmemakefileutils.h"
#include "tier0/include/dbg.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vphysics_interface.h"

// These tier3 libraries must be set by any users of this library.
// They can be set by calling ConnectTier3Libraries.
// It is hoped that setting this, and using this library will be the common
// mechanism for allowing link libraries to access tier3 library interfaces

IStudioRender *g_pStudioRender{nullptr};
IStudioRender *studiorender{nullptr};
IMatSystemSurface *g_pMatSystemSurface{nullptr};
vgui::IInput *g_pVGuiInput{nullptr};
vgui::ISurface *g_pVGuiSurface{nullptr};
vgui::IPanel *g_pVGuiPanel{nullptr};
vgui::IVGui *g_pVGui{nullptr};
vgui::ILocalize *g_pVGuiLocalize{nullptr};
vgui::ISchemeManager *g_pVGuiSchemeManager{nullptr};
vgui::ISystem *g_pVGuiSystem{nullptr};
IDataCache *g_pDataCache{nullptr};
IMDLCache *g_pMDLCache{nullptr};
IMDLCache *mdlcache{nullptr};
IAvi *g_pAVI{nullptr};
IBik *g_pBIK{nullptr};
IDmeMakefileUtils *g_pDmeMakefileUtils{nullptr};
IPhysicsCollision *g_pPhysicsCollision{nullptr};
ISoundEmitterSystemBase *g_pSoundEmitterSystem{nullptr};

// Call this to connect to all tier 3 libraries.
// It's up to the caller to check the globals it cares about to see if ones are
// missing

void ConnectTier3Libraries(CreateInterfaceFn *pFactoryList,
                           usize nFactoryCount) {
  // Don't connect twice..
  Assert(!g_pStudioRender && !studiorender && !g_pMatSystemSurface &&
         !g_pVGui && !g_pVGuiPanel && !g_pVGuiInput && !g_pVGuiSurface &&
         !g_pDataCache && !g_pMDLCache && !mdlcache && !g_pAVI && !g_pBIK &&
         !g_pDmeMakefileUtils && !g_pPhysicsCollision && !g_pVGuiLocalize &&
         !g_pSoundEmitterSystem && !g_pVGuiSchemeManager && !g_pVGuiSystem);

  for (usize i = 0; i < nFactoryCount; ++i) {
    if (!g_pStudioRender) {
      g_pStudioRender = studiorender = (IStudioRender *)pFactoryList[i](
          STUDIO_RENDER_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGui) {
      g_pVGui =
          (vgui::IVGui *)pFactoryList[i](VGUI_IVGUI_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiInput) {
      g_pVGuiInput = (vgui::IInput *)pFactoryList[i](
          VGUI_INPUT_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiPanel) {
      g_pVGuiPanel = (vgui::IPanel *)pFactoryList[i](
          VGUI_PANEL_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiSurface) {
      g_pVGuiSurface = (vgui::ISurface *)pFactoryList[i](
          VGUI_SURFACE_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiSchemeManager) {
      g_pVGuiSchemeManager = (vgui::ISchemeManager *)pFactoryList[i](
          VGUI_SCHEME_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiSystem) {
      g_pVGuiSystem = (vgui::ISystem *)pFactoryList[i](
          VGUI_SYSTEM_INTERFACE_VERSION, nullptr);
    }
    if (!g_pVGuiLocalize) {
      g_pVGuiLocalize = (vgui::ILocalize *)pFactoryList[i](
          VGUI_LOCALIZE_INTERFACE_VERSION, nullptr);
    }
    if (!g_pMatSystemSurface) {
      g_pMatSystemSurface = (IMatSystemSurface *)pFactoryList[i](
          MAT_SYSTEM_SURFACE_INTERFACE_VERSION, nullptr);
    }
    if (!g_pDataCache) {
      g_pDataCache =
          (IDataCache *)pFactoryList[i](DATACACHE_INTERFACE_VERSION, nullptr);
    }
    if (!g_pMDLCache) {
      g_pMDLCache = mdlcache =
          (IMDLCache *)pFactoryList[i](MDLCACHE_INTERFACE_VERSION, nullptr);
    }
    if (!g_pAVI) {
      g_pAVI = (IAvi *)pFactoryList[i](AVI_INTERFACE_VERSION, nullptr);
    }
    if (!g_pBIK) {
      g_pBIK = (IBik *)pFactoryList[i](BIK_INTERFACE_VERSION, nullptr);
    }
    if (!g_pDmeMakefileUtils) {
      g_pDmeMakefileUtils = (IDmeMakefileUtils *)pFactoryList[i](
          DMEMAKEFILE_UTILS_INTERFACE_VERSION, nullptr);
    }
    if (!g_pPhysicsCollision) {
      g_pPhysicsCollision = (IPhysicsCollision *)pFactoryList[i](
          VPHYSICS_COLLISION_INTERFACE_VERSION, nullptr);
    }
    if (!g_pSoundEmitterSystem) {
      g_pSoundEmitterSystem = (ISoundEmitterSystemBase *)pFactoryList[i](
          SOUNDEMITTERSYSTEM_INTERFACE_VERSION, nullptr);
    }
  }
}

void DisconnectTier3Libraries() {
  g_pStudioRender = nullptr;
  studiorender = nullptr;
  g_pVGui = nullptr;
  g_pVGuiInput = nullptr;
  g_pVGuiPanel = nullptr;
  g_pVGuiSurface = nullptr;
  g_pVGuiLocalize = nullptr;
  g_pVGuiSchemeManager = nullptr;
  g_pVGuiSystem = nullptr;
  g_pMatSystemSurface = nullptr;
  g_pDataCache = nullptr;
  g_pMDLCache = nullptr;
  mdlcache = nullptr;
  g_pAVI = nullptr;
  g_pBIK = nullptr;
  g_pPhysicsCollision = nullptr;
  g_pDmeMakefileUtils = nullptr;
  g_pSoundEmitterSystem = nullptr;
}
