// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SHADERDEVICEDX10_H
#define SHADERDEVICEDX10_H

#include "shaderdevicebase.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlvector.h"

struct IDXGIFactory;
struct IDXGIAdapter;
struct IDXGIOutput;
struct IDXGISwapChain;
struct ID3D10Device;
struct ID3D10RenderTargetView;
struct ID3D10VertexShader;
struct ID3D10PixelShader;
struct ID3D10GeometryShader;
struct ID3D10InputLayout;
struct ID3D10ShaderReflection;

// The Base implementation of the shader device
class CShaderDeviceMgrDx10 : public CShaderDeviceMgrBase {
  typedef CShaderDeviceMgrBase BaseClass;

 public:
  // constructor, destructor
  CShaderDeviceMgrDx10();
  virtual ~CShaderDeviceMgrDx10();

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

 private:
  // Initialize adapter information
  void InitAdapterInfo();

  // Determines hardware caps from D3D
  bool ComputeCapsFromD3D(HardwareCaps_t *pCaps, IDXGIAdapter *pAdapter,
                          IDXGIOutput *pOutput);

  // Returns the amount of video memory in bytes for a particular adapter
  virtual u64 GetVidMemBytes(u32 nAdapter) const override;

  // Returns the appropriate adapter output to use
  IDXGIOutput *GetAdapterOutput(int nAdapter) const;

  // Returns the adapter interface for a particular adapter
  IDXGIAdapter *GetAdapter(int nAdapter) const;

  // Used to enumerate adapters, attach to windows
  IDXGIFactory *m_pDXGIFactory;

  bool m_bObeyDxCommandlineOverride : 1;

  friend class CShaderDeviceDx10;
};

//-----------------------------------------------------------------------------
// The Dx10 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceDx10 : public CShaderDeviceBase {
 public:
  CShaderDeviceDx10();
  virtual ~CShaderDeviceDx10();

  // Methods of IShaderDevice

  void ReleaseResources() override {}
  void ReacquireResources() override {}

  ImageFormat GetBackBufferFormat() const override;
  void GetBackBufferDimensions(int &width, int &height) const override;

  int GetCurrentAdapter() const override;

  bool IsUsingGraphics() const override;

  void SpewDriverInfo() const override;

  void Present() override;

  void SetHardwareGammaRamp(float fGamma, float fGammaTVRangeMin,
                            float fGammaTVRangeMax, float fGammaTVExponent,
                            bool bTVEnabled) override;

  IShaderBuffer *CompileShader(const char *pProgram, size_t nBufLen,
                               const char *pShaderVersion) override;

  VertexShaderHandle_t CreateVertexShader(IShaderBuffer *pShader) override;
  void DestroyVertexShader(VertexShaderHandle_t hShader) override;

  GeometryShaderHandle_t CreateGeometryShader(
      IShaderBuffer *pShaderBuffer) override;
  void DestroyGeometryShader(GeometryShaderHandle_t hShader) override;

  PixelShaderHandle_t CreatePixelShader(IShaderBuffer *pShaderBuffer) override;
  void DestroyPixelShader(PixelShaderHandle_t hShader);

  IMesh *CreateStaticMesh(VertexFormat_t format,
                          const char *pTextureBudgetGroup,
                          IMaterial *pMaterial) override;
  void DestroyStaticMesh(IMesh *mesh) override;

  IVertexBuffer *CreateVertexBuffer(ShaderBufferType_t type, VertexFormat_t fmt,
                                    int nVertexCount,
                                    const char *pTextureBudgetGroup) override;
  void DestroyVertexBuffer(IVertexBuffer *pVertexBuffer) override;

  IIndexBuffer *CreateIndexBuffer(ShaderBufferType_t type,
                                  MaterialIndexFormat_t fmt, int nIndexCount,
                                  const char *pTextureBudgetGroup) override;
  void DestroyIndexBuffer(IIndexBuffer *pIndexBuffer) override;

  IVertexBuffer *GetDynamicVertexBuffer(int nStreamID,
                                        VertexFormat_t vertexFormat,
                                        bool bBuffered = true) override;
  IIndexBuffer *GetDynamicIndexBuffer(MaterialIndexFormat_t fmt,
                                      bool bBuffered = true) override;

  // A special path used to tick the front buffer while loading on the 360
  void EnableNonInteractiveMode(MaterialNonInteractiveMode_t mode,
                                ShaderNonInteractiveInfo_t *pInfo) override {}
  void RefreshFrontBufferNonInteractive() override {}

  // Methods of CShaderDeviceBase

  bool InitDevice(HWND hWnd, int nAdapter,
                  const ShaderDeviceInfo_t &mode) override;
  void ShutdownDevice() override;
  bool IsDeactivated() const override { return false; }

  // Other public methods

  // Inline methods of CShaderDeviceDx10
  ID3D10VertexShader *GetVertexShader(VertexShaderHandle_t hShader) const {
    if (hShader != VERTEX_SHADER_HANDLE_INVALID)
      return m_VertexShaderDict[(VertexShaderIndex_t)hShader].m_pShader;

    return nullptr;
  }

  ID3D10GeometryShader *GetGeometryShader(
      GeometryShaderHandle_t hShader) const {
    if (hShader != GEOMETRY_SHADER_HANDLE_INVALID)
      return m_GeometryShaderDict[(GeometryShaderIndex_t)hShader].m_pShader;

    return nullptr;
  }

  ID3D10PixelShader *GetPixelShader(PixelShaderHandle_t hShader) const {
    if (hShader != PIXEL_SHADER_HANDLE_INVALID)
      return m_PixelShaderDict[(PixelShaderIndex_t)hShader].m_pShader;

    return nullptr;
  }

  ID3D10InputLayout *GetInputLayout(VertexShaderHandle_t hShader,
                                    VertexFormat_t format);

 private:
  struct InputLayout_t {
    ID3D10InputLayout *m_pInputLayout;
    VertexFormat_t m_VertexFormat;
  };

  using InputLayoutDict_t = CUtlRBTree<InputLayout_t, u16>;

  static bool InputLayoutLessFunc(const InputLayout_t &lhs,
                                  const InputLayout_t &rhs) {
    return lhs.m_VertexFormat < rhs.m_VertexFormat;
  }

  struct VertexShader_t {
    ID3D10VertexShader *m_pShader;
    ID3D10ShaderReflection *m_pInfo;
    void *m_pByteCode;
    size_t m_nByteCodeLen;
    InputLayoutDict_t m_InputLayouts;

    VertexShader_t() : m_InputLayouts{0, 0, InputLayoutLessFunc} {}
  };

  struct GeometryShader_t {
    ID3D10GeometryShader *m_pShader;
    ID3D10ShaderReflection *m_pInfo;
  };

  struct PixelShader_t {
    ID3D10PixelShader *m_pShader;
    ID3D10ShaderReflection *m_pInfo;
  };

  using VertexShaderIndex_t = CUtlFixedLinkedList<VertexShader_t>::IndexType_t;
  using GeometryShaderIndex_t =
      CUtlFixedLinkedList<GeometryShader_t>::IndexType_t;
  using PixelShaderIndex_t = CUtlFixedLinkedList<PixelShader_t>::IndexType_t;

  void SetupHardwareCaps();
  void ReleaseInputLayouts(VertexShaderIndex_t nIndex);

  IDXGIOutput *m_pOutput;
  ID3D10Device *m_pDevice;
  IDXGISwapChain *m_pSwapChain;
  ID3D10RenderTargetView *m_pRenderTargetView;

  CUtlFixedLinkedList<VertexShader_t> m_VertexShaderDict;
  CUtlFixedLinkedList<GeometryShader_t> m_GeometryShaderDict;
  CUtlFixedLinkedList<PixelShader_t> m_PixelShaderDict;

  friend ID3D10Device *D3D10Device();
  friend IDXGISwapChain *D3D10SwapChain();
  friend ID3D10RenderTargetView *D3D10RenderTargetView();
};

// Singleton
extern CShaderDeviceDx10 *g_pShaderDeviceDx10;

// Utility methods
inline ID3D10Device *D3D10Device() { return g_pShaderDeviceDx10->m_pDevice; }

inline IDXGISwapChain *D3D10SwapChain() {
  return g_pShaderDeviceDx10->m_pSwapChain;
}

inline ID3D10RenderTargetView *D3D10RenderTargetView() {
  return g_pShaderDeviceDx10->m_pRenderTargetView;
}

#endif  // SHADERDEVICEDX10_H
