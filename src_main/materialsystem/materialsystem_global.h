// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATERIALSYSTEM_MATERIALSYSTEM_GLOBAL_H_
#define SOURCE_MATERIALSYSTEM_MATERIALSYSTEM_GLOBAL_H_

#include "imaterialsysteminternal.h"
#include "tier0/include/dbg.h"
#include "tier2/tier2.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ITextureInternal;
class IShaderAPI;
class IHardwareConfigInternal;
class IShaderUtil;
class IShaderShadow;
class IShaderDeviceMgr;
class IShaderDevice;
class IShaderSystemInternal;
class IMaterialInternal;
class IColorCorrectionSystem;
class IMaterialVar;

//-----------------------------------------------------------------------------
// Constants used by the system
//-----------------------------------------------------------------------------

#define MATERIAL_MAX_PATH 256

// GR - limits for blured image (HDR stuff)
#define MAX_BLUR_IMAGE_WIDTH 256
#define MAX_BLUR_IMAGE_HEIGHT 192

#define CLAMP_BLUR_IMAGE_WIDTH(_w) \
  (((_w) < MAX_BLUR_IMAGE_WIDTH) ? (_w) : MAX_BLUR_IMAGE_WIDTH)
#define CLAMP_BLUR_IMAGE_HEIGHT(_h) \
  (((_h) < MAX_BLUR_IMAGE_HEIGHT) ? (_h) : MAX_BLUR_IMAGE_HEIGHT)

//-----------------------------------------------------------------------------
// Global structures
//-----------------------------------------------------------------------------
extern MaterialSystem_Config_t g_config;
extern uint32_t g_nDebugVarsSignature;

// extern MaterialSystem_ErrorFunc_t	Error;
// extern MaterialSystem_WarningFunc_t Warning;

extern IShaderAPI* g_pShaderAPI;
extern IShaderDeviceMgr* g_pShaderDeviceMgr;
extern IShaderDevice* g_pShaderDevice;
extern IShaderShadow* g_pShaderShadow;

extern IMaterialInternal* g_pErrorMaterial;

IShaderSystemInternal* ShaderSystem();
inline IShaderSystemInternal* ShaderSystem() {
  extern IShaderSystemInternal* g_pShaderSystem;
  return g_pShaderSystem;
}

inline IHardwareConfigInternal* HardwareConfig() {
  extern IHardwareConfigInternal* g_pHWConfig;
  return g_pHWConfig;
}

//-----------------------------------------------------------------------------
// Accessor to get at the material system
//-----------------------------------------------------------------------------
inline IMaterialSystemInternal* MaterialSystem() {
  extern IMaterialSystemInternal* g_pInternalMaterialSystem;
  return g_pInternalMaterialSystem;
}

inline IShaderUtil* ShaderUtil() {
  extern IShaderUtil* g_pShaderUtil;
  return g_pShaderUtil;
}

extern IColorCorrectionSystem* g_pColorCorrectionSystem;
inline IColorCorrectionSystem* ColorCorrectionSystem() {
  return g_pColorCorrectionSystem;
}

//-----------------------------------------------------------------------------
// Global methods related to material vars
//-----------------------------------------------------------------------------
void EnableThreadedMaterialVarAccess(bool bEnable, IMaterialVar** ppParams,
                                     int nVarCount);

#endif  // SOURCE_MATERIALSYSTEM_MATERIALSYSTEM_GLOBAL_H_
