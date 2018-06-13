// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEDX8_H_
#define MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEDX8_H_

#include "base/include/windows/windows_light.h"

#include <d3d9.h>
#include "base/include/base_types.h"
#include "shaderapidx8_global.h"
#include "shaderdevicebase.h"
#include "tier1/utlvector.h"

// Describes which D3DDEVTYPE to use
constexpr inline D3DDEVTYPE SOURCE_DX9_DEVICE_TYPE{D3DDEVTYPE_HAL};

// By default, PIX profiling is explicitly disallowed using the
// D3DPERF_SetOptions(1) API on PC. Uncomment to use PIX instrumentation:
//#define	PIX_INSTRUMENTATION

#define MAX_PIX_ERRORS 3

// The Base implementation of the shader device
class CShaderDeviceMgrDx8 : public CShaderDeviceMgrBase {
  using BaseClass = CShaderDeviceMgrBase;

 public:
  CShaderDeviceMgrDx8();
  virtual ~CShaderDeviceMgrDx8();

  // Methods of IAppSystem

  bool Connect(CreateInterfaceFn factory) override;
  void Disconnect() override;
  InitReturnVal_t Init() override;
  void Shutdown() override;

  // Methods of IShaderDeviceMgr

  int GetAdapterCount() const override;
  void GetAdapterInfo(int adapter, MaterialAdapterInfo_t &info) const override;

  int GetModeCount(int nAdapter) const override;
  void GetModeInfo(ShaderDisplayMode_t *pInfo, int nAdapter,
                   int mode) const override;
  void GetCurrentModeInfo(ShaderDisplayMode_t *pInfo,
                          int nAdapter) const override;

  bool SetAdapter(int nAdapter, int nFlags) override;
  CreateInterfaceFn SetMode(HWND hWnd, int nAdapter,
                            const ShaderDeviceInfo_t &mode) override;

  // Determines hardware caps from D3D
  bool ComputeCapsFromD3D(HardwareCaps_t *pCaps, u32 nAdapter);

  // Forces caps to a specific dx level
  void ForceCapsToDXLevel(HardwareCaps_t *pCaps, int nDxLevel,
                          const HardwareCaps_t &actualCaps);

  // Validates the mode...
  bool ValidateMode(int nAdapter, const ShaderDeviceInfo_t &info) const;

  // Returns the amount of video memory in bytes for a particular adapter
  virtual u64 GetVidMemBytes(u32 adapter_idx) const override;

  SOURCE_FORCEINLINE IDirect3D9 *D3D() const { return m_pD3D; }

 protected:
  // Determine capabilities
  bool DetermineHardwareCaps();

 private:
  // Initialize adapter information
  void InitAdapterInfo();

  // Code to detect support for ATI2N and ATI1N formats for normal map
  // compression
  void CheckNormalCompressionSupport(HardwareCaps_t *pCaps, int nAdapter);

  // Code to detect support for texture border mode (not a simple caps check)
  void CheckBorderColorSupport(HardwareCaps_t *pCaps, int nAdapter);

  // Vendor-dependent code to detect support for various flavors of shadow
  // mapping
  void CheckVendorDependentShadowMappingSupport(HardwareCaps_t *pCaps,
                                                int nAdapter);

  // Vendor-dependent code to detect Alpha To Coverage Backdoors
  void CheckVendorDependentAlphaToCoverage(HardwareCaps_t *pCaps, int nAdapter);

  // Compute the effective DX support level based on all the other caps
  void ComputeDXSupportLevel(HardwareCaps_t &caps);

  // Used to enumerate adapters, attach to windows
  IDirect3D9 *m_pD3D;

  bool m_bObeyDxCommandlineOverride : 1;
  bool m_bAdapterInfoIntialized : 1;
};

extern CShaderDeviceMgrDx8 *g_pShaderDeviceMgrDx8;

// IDirect3D accessor
inline IDirect3D9 *D3D() { return g_pShaderDeviceMgrDx8->D3D(); }

// The Dx8 implementation of the shader device
class CShaderDeviceDx8 : public CShaderDeviceBase {
 public:
  CShaderDeviceDx8();
  virtual ~CShaderDeviceDx8();

  // Methods of IShaderDevice

  void ReleaseResources() override;
  void ReacquireResources() override;

  ImageFormat GetBackBufferFormat() const override;
  void GetBackBufferDimensions(int &width, int &height) const override;

  int GetCurrentAdapter() const override;

  bool IsUsingGraphics() const override;

  void SpewDriverInfo() const override;

  void Present() override;

  IShaderBuffer *CompileShader(const char *pProgram, size_t nBufLen,
                               const char *pShaderVersion) override;

  VertexShaderHandle_t CreateVertexShader(IShaderBuffer *pBuffer) override;
  void DestroyVertexShader(VertexShaderHandle_t hShader) override;

  GeometryShaderHandle_t CreateGeometryShader(
      IShaderBuffer *pShaderBuffer) override;
  void DestroyGeometryShader(GeometryShaderHandle_t hShader) override;

  PixelShaderHandle_t CreatePixelShader(IShaderBuffer *pShaderBuffer) override;
  void DestroyPixelShader(PixelShaderHandle_t hShader) override;

  IMesh *CreateStaticMesh(VertexFormat_t format, const char *pBudgetGroup,
                          IMaterial *pMaterial = nullptr) override;
  void DestroyStaticMesh(IMesh *mesh) override;

  IVertexBuffer *CreateVertexBuffer(ShaderBufferType_t type, VertexFormat_t fmt,
                                    int nVertexCount,
                                    const char *pBudgetGroup) override;
  void DestroyVertexBuffer(IVertexBuffer *pVertexBuffer) override;

  IIndexBuffer *CreateIndexBuffer(ShaderBufferType_t bufferType,
                                  MaterialIndexFormat_t fmt, int nIndexCount,
                                  const char *pBudgetGroup) override;
  void DestroyIndexBuffer(IIndexBuffer *pIndexBuffer) override;

  IVertexBuffer *GetDynamicVertexBuffer(int nStreamID,
                                        VertexFormat_t vertexFormat,
                                        bool bBuffered = true) override;
  IIndexBuffer *GetDynamicIndexBuffer(MaterialIndexFormat_t fmt,
                                      bool bBuffered = true) override;

  void SetHardwareGammaRamp(float fGamma, float fGammaTVRangeMin,
                            float fGammaTVRangeMax, float fGammaTVExponent,
                            bool bTVEnabled) override;

  void EnableNonInteractiveMode(
      MaterialNonInteractiveMode_t mode,
      ShaderNonInteractiveInfo_t *pInfo = nullptr) override;
  void RefreshFrontBufferNonInteractive() override;

  // Methods of CShaderDeviceBase

  bool InitDevice(HWND hWnd, int nAdapter,
                  const ShaderDeviceInfo_t &info) override;
  void ShutdownDevice() override;

  // Used to determine if we're deactivated
  SOURCE_FORCEINLINE bool IsDeactivated() const override {
    return m_DeviceState != DEVICE_STATE_OK || m_bQueuedDeviceLost ||
           m_numReleaseResourcesRefCount;
  }

  // Call this when another app is initializing or finished initializing
  void OtherAppInitializing(bool initializing) override;

  // Other public methods
 public:
  // TODO(d.rattman): Make private
  // Which device are we using?
  u32 m_DisplayAdapter;
  D3DDEVTYPE m_DeviceType;

 protected:
  enum DeviceState_t {
    DEVICE_STATE_OK = 0,
    DEVICE_STATE_OTHER_APP_INIT,
    DEVICE_STATE_LOST_DEVICE,
    DEVICE_STATE_NEEDS_RESET,
  };

  struct NonInteractiveRefreshState_t {
    IDirect3DVertexShader9 *m_pVertexShader;
    IDirect3DPixelShader9 *m_pPixelShader;
    IDirect3DPixelShader9 *m_pPixelShaderStartup;
    IDirect3DPixelShader9 *m_pPixelShaderStartupPass2;
    IDirect3DVertexDeclaration9 *m_pVertexDecl;
    ShaderNonInteractiveInfo_t m_Info;
    MaterialNonInteractiveMode_t m_Mode;
    float m_flLastPacifierTime;
    int m_nPacifierFrame;

    float m_flStartTime;
    float m_flLastPresentTime;
    float m_flPeakDt;
    float m_flTotalDt;
    int m_nSamples;
    int m_nCountAbove66;
  };

 protected:
  // Creates the D3D Device
  bool CreateD3DDevice(HWND pHWnd, int nAdapter,
                       const ShaderDeviceInfo_t &info);

  // Actually creates the D3D Device once the present parameters are set up
  IDirect3DDevice9 *InvokeCreateDevice(HWND hWnd, int nAdapter,
                                       DWORD deviceCreationFlags);

  // Checks for CreateQuery support
  void DetectQuerySupport(IDirect3DDevice9 *pD3DDevice);

  // Computes the presentation parameters
  void SetPresentParameters(HWND hWnd, int nAdapter,
                            const ShaderDeviceInfo_t &info);

  // Computes the supersample flags
  D3DMULTISAMPLE_TYPE ComputeMultisampleType(int nSampleCount);

  // Is the device active?
  bool IsActive() const;

  // Try to reset the device, returned true if it succeeded
  bool TryDeviceReset();

  // Queue up the fact that the device was lost
  void MarkDeviceLost();

  // Deals with lost devices
  void CheckDeviceLost(bool bOtherAppInitializing);

  // Changes the window size
  bool ResizeWindow(const ShaderDeviceInfo_t &info);

  // Deals with the frame synching object
  void AllocFrameSyncObjects();
  void FreeFrameSyncObjects();

  // Alloc and free objects that are necessary for frame syncing
  void AllocFrameSyncTextureObject();
  void FreeFrameSyncTextureObject();

  // Alloc and free objects necessary for noninteractive frame refresh on the
  // x360
  bool AllocNonInteractiveRefreshObjects();
  void FreeNonInteractiveRefreshObjects();

  // TODO(d.rattman): This is for backward compat; I still haven't solved a way
  // of decoupling this
  virtual bool OnAdapterSet() = 0;
  virtual void ResetRenderState(bool bFullReset = true) = 0;

  // For measuring if we meed TCR 022 on the XBox (refreshing often enough)
  void UpdatePresentStats();

  bool InNonInteractiveMode() const;

  void ReacquireResourcesInternal(bool bResetState = false,
                                  bool bForceReacquire = false,
                                  char const *pszForceReason = NULL);

  D3DPRESENT_PARAMETERS m_PresentParameters;
  ImageFormat m_AdapterFormat;

  // Mode info
  int m_DeviceSupportsCreateQuery;

  ShaderDeviceInfo_t m_PendingVideoModeChangeConfig;
  DeviceState_t m_DeviceState;

  bool m_bOtherAppInitializing : 1;
  bool m_bQueuedDeviceLost : 1;
  bool m_IsResizing : 1;
  bool m_bPendingVideoModeChange : 1;
  bool m_bUsingStencil : 1;

  // amount of stencil variation we have available
  int m_iStencilBufferBits;

  // Frame synch objects
  IDirect3DQuery9 *m_pFrameSyncQueryObject;
  IDirect3DTexture9 *m_pFrameSyncTexture;

  // Used for x360 only
  NonInteractiveRefreshState_t m_NonInteractiveRefresh;
  CThreadFastMutex m_nonInteractiveModeMutex;
  friend class CShaderDeviceMgrDx8;

  int m_numReleaseResourcesRefCount;  // This is holding the number of
                                      // ReleaseResources calls queued up, for
                                      // every ReleaseResources call there
                                      // should be a matching call to
                                      // ReacquireResources, only the last
                                      // top-level ReacquireResources will have
                                      // effect. Nested ReleaseResources calls
                                      // are bugs.
};

// Globals
class Direct3DDevice9Wrapper;
Direct3DDevice9Wrapper *Dx9Device();

extern CShaderDeviceDx8 *g_pShaderDeviceDx8;

#endif  // MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEDX8_H_
