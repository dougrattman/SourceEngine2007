// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_SHADERRENDERBASE_H_
#define MATERIALSYSTEM_SHADERAPIDX9_SHADERRENDERBASE_H_

#include "shaderapi/ishaderapi.h"
#include "shaderapi_global.h"

#include "locald3dtypes.h"

// Colors for PIX graphs
#define PIX_VALVE_ORANGE 0xFFF5940F

// The Base implementation of the shader rendering interface
class CShaderAPIBase : public IShaderAPI {
 public:
  // constructor, destructor
  CShaderAPIBase();
  virtual ~CShaderAPIBase();

  // Called when the device is initializing or shutting down
  virtual bool OnDeviceInit() = 0;
  virtual void OnDeviceShutdown() = 0;

  // Pix events
  virtual void BeginPIXEvent(unsigned long color, const char* szName) = 0;
  virtual void EndPIXEvent() = 0;
  virtual void AdvancePIXFrame() = 0;

  // Release, reacquire objects
  virtual void ReleaseShaderObjects() = 0;
  virtual void RestoreShaderObjects() = 0;

  // Resets the render state to its well defined initial value
  virtual void ResetRenderState(bool bFullReset = true) = 0;

  // Returns a d3d texture associated with a texture handle
  virtual IDirect3DBaseTexture* GetD3DTexture(
      ShaderAPITextureHandle_t hTexture) = 0;

  // Queues a non-full reset of render state next BeginFrame.
  virtual void QueueResetRenderState() = 0;

  // Methods of IShaderDynamicAPI
  virtual void GetCurrentColorCorrection(ShaderColorCorrectionInfo_t* pInfo);
};

// Pix measurement class.
class CPixEvent {
 public:
  CPixEvent(unsigned long color, const char* szName) {
    g_pShaderAPI->BeginPIXEvent(color, szName);
  }

  ~CPixEvent() { g_pShaderAPI->EndPIXEvent(); }
};

#endif  // MATERIALSYSTEM_SHADERAPIDX9_SHADERRENDERBASE_H_
