// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "shaderdevicedx8.h"

#include "colorformatdx8.h"
#include "filesystem.h"
#include "imeshdx8.h"
#include "locald3dtypes.h"
#include "materialsystem/ishader.h"
#include "materialsystem/materialsystem_config.h"
#include "recording.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx8.h"
#include "shaderapidx8_global.h"
#include "shadershadowdx8.h"
#include "tier0/include/icommandline.h"
#include "tier2/tier2.h"
#include "vertexshaderdx8.h"
#include "wmi.h"

static CShaderDeviceMgrDx8 g_ShaderDeviceMgrDx8;
CShaderDeviceMgrDx8 *g_pShaderDeviceMgrDx8 = &g_ShaderDeviceMgrDx8;

#ifndef SHADERAPIDX10
// In the shaderapidx10.dll, we use its version of IShaderDeviceMgr.
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShaderDeviceMgrDx8, IShaderDeviceMgr,
                                  SHADER_DEVICE_MGR_INTERFACE_VERSION,
                                  g_ShaderDeviceMgrDx8)
#endif

// Hook into mat_forcedynamic from the engine.
static ConVar mat_forcedynamic("mat_forcedynamic", "0", FCVAR_CHEAT);

// This is hooked into the engines convar.
ConVar mat_debugalttab("mat_debugalttab", "0", FCVAR_CHEAT);

CShaderDeviceMgrDx8::CShaderDeviceMgrDx8()
    : m_pD3D{nullptr},
      m_bObeyDxCommandlineOverride{true},
      m_bAdapterInfoIntialized{false} {}

CShaderDeviceMgrDx8::~CShaderDeviceMgrDx8() {}

bool CShaderDeviceMgrDx8::Connect(CreateInterfaceFn factory) {
  LOCK_SHADERAPI();

  if (!BaseClass::Connect(factory)) return false;

  m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  if (!m_pD3D) {
    Warning("Failed to create D3D9!\n");
    return false;
  }

  // TODO(d.rattman): Want this to be here, but we can't because Steam
  // hasn't had it's application ID set up yet.

  //	InitAdapterInfo();
  return true;
}

void CShaderDeviceMgrDx8::Disconnect() {
  LOCK_SHADERAPI();

  if (m_pD3D) {
    m_pD3D->Release();
    m_pD3D = nullptr;
  }

  BaseClass::Disconnect();
}

InitReturnVal_t CShaderDeviceMgrDx8::Init() {
  // TODO(d.rattman): Remove call to InitAdapterInfo once Steam startup issues
  // are resolved. Do it in Connect instead.
  InitAdapterInfo();

  return INIT_OK;
}

void CShaderDeviceMgrDx8::Shutdown() {
  LOCK_SHADERAPI();

  if (g_pShaderAPI) {
    g_pShaderAPI->OnDeviceShutdown();
  }

  if (g_pShaderDevice) {
    g_pShaderDevice->ShutdownDevice();
    g_pMaterialSystemHardwareConfig = nullptr;
  }
}

bool CShaderDeviceDx8::IsActive() const { return Dx9Device()->IsActive(); }

// Initialize adapter information
void CShaderDeviceMgrDx8::InitAdapterInfo() {
  if (m_bAdapterInfoIntialized) return;

  m_bAdapterInfoIntialized = true;
  adapters_.RemoveAll();

  int nCount = m_pD3D->GetAdapterCount();
  for (int i = 0; i < nCount; ++i) {
    int j = adapters_.AddToTail();
    AdapterInfo_t &info = adapters_[j];

#ifdef _DEBUG
    memset(&info.m_ActualCaps, 0xDD, sizeof(info.m_ActualCaps));
#endif

    info.m_ActualCaps.m_bDeviceOk = ComputeCapsFromD3D(&info.m_ActualCaps, i);
    if (!info.m_ActualCaps.m_bDeviceOk) continue;

    ReadDXSupportLevels(info.m_ActualCaps);

    // Read dxsupport.cfg which has config overrides for particular cards.
    ReadHardwareCaps(info.m_ActualCaps, info.m_ActualCaps.m_nMaxDXSupportLevel);

    // What's in "-shader" overrides dxsupport.cfg
    const char *pShaderParam = CommandLine()->ParmValue("-shader");
    if (pShaderParam) {
      Q_strncpy(info.m_ActualCaps.m_pShaderDLL, pShaderParam,
                sizeof(info.m_ActualCaps.m_pShaderDLL));
    }
  }
}

//--------------------------------------------------------------------------------
// Code to detect support for texture border color (widely supported but the
// caps bit is messed up in drivers due to a stupid WHQL test that requires this
// to work with float textures which we don't generally care about wrt this
// address mode)
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckBorderColorSupport(HardwareCaps_t *pCaps,
                                                  int nAdapter) {
  // Most PC parts do this, but let's not deal with that yet (JasonM)
  pCaps->m_bSupportsBorderColor = false;
}

//--------------------------------------------------------------------------------
// Code to detect support for ATI2N and ATI1N formats for normal map compression
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckNormalCompressionSupport(HardwareCaps_t *pCaps,
                                                        int nAdapter) {
  pCaps->m_bSupportsNormalMapCompression = false;

#if defined(COMPRESSED_NORMAL_FORMATS)
//	Check for normal map compression support on PC when we decide to ship
// it...
//  Remove relevant IsX360() calls in CTexture when we plan to ship on PC
//	360 Requires more work in the download logic
//
//	// Test ATI2N support
//	if ( m_pD3D->CheckDeviceFormat( nAdapter, SOURCE_DX9_DEVICE_TYPE,
// D3DFMT_X8R8G8B8,
// 0, D3DRTYPE_TEXTURE, ATIFMT_ATI2N ) == S_OK )
//	{
//		// Test ATI1N support
//		if ( m_pD3D->CheckDeviceFormat( nAdapter,
// SOURCE_DX9_DEVICE_TYPE,
// D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, ATIFMT_ATI1N ) == S_OK )
//		{
//			pCaps->m_bSupportsNormalMapCompression = true;
//		}
//	}
#endif
}

//--------------------------------------------------------------------------------
// Vendor-dependent code to detect support for various flavors of shadow mapping
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentShadowMappingSupport(
    HardwareCaps_t *pCaps, int nAdapter) {
  // Set a default 0 texture format...may be overridden below by IHV-specific
  // surface type
  pCaps->m_NullTextureFormat = IMAGE_FORMAT_ARGB8888;
  if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET,
                                D3DRTYPE_TEXTURE, D3DFMT_R5G6B5) == S_OK) {
    pCaps->m_NullTextureFormat = IMAGE_FORMAT_RGB565;
  }

  bool bToolsMode = (CommandLine()->CheckParm("-tools") != nullptr);
  bool bFound16Bit = false;

  if ((pCaps->m_VendorID == VENDORID_NVIDIA) &&
      (pCaps->m_SupportsShaderModel_3_0))  // ps_3_0 parts from nVidia
  {
    // First, test for 0 texture support
    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET,
                                  D3DRTYPE_TEXTURE, NVFMT_NULL) == S_OK) {
      pCaps->m_NullTextureFormat = IMAGE_FORMAT_NV_NULL;
    }

    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_TEXTURE, D3DFMT_D16) == S_OK) {
      pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST16;
      pCaps->m_bSupportsFetch4 = false;
      pCaps->m_bSupportsShadowDepthTextures = true;
      bFound16Bit = true;

      if (!bToolsMode)  // Tools will continue on and try for 24 bit...
        return;
    }

    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_TEXTURE, D3DFMT_D24S8) == S_OK) {
      pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST24;
      pCaps->m_bSupportsFetch4 = false;
      pCaps->m_bSupportsShadowDepthTextures = true;
      return;
    }

    if (bFound16Bit)  // Found 16 bit but not 24
      return;
  } else if ((pCaps->m_VendorID == VENDORID_ATI) &&
             pCaps->m_SupportsPixelShaders_2_b)  // ps_2_b parts from ATI
  {
    // Initially, check for Fetch4 (tied to ATIFMT_D24S8 support)
    pCaps->m_bSupportsFetch4 = false;
    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_TEXTURE, ATIFMT_D24S8) == S_OK) {
      pCaps->m_bSupportsFetch4 = true;
    }

    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_TEXTURE,
                                  ATIFMT_D16) == S_OK)  // Prefer 16-bit
    {
      pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST16;
      pCaps->m_bSupportsShadowDepthTextures = true;
      bFound16Bit = true;

      if (!bToolsMode)  // Tools will continue on and try for 24 bit...
        return;
    }

    if (m_pD3D->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                  D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
                                  D3DRTYPE_TEXTURE, ATIFMT_D24S8) == S_OK) {
      pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST24;
      pCaps->m_bSupportsShadowDepthTextures = true;
      return;
    }

    if (bFound16Bit)  // Found 16 bit but not 24
      return;
  }

  // Other vendor or old hardware
  pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_UNKNOWN;
  pCaps->m_bSupportsShadowDepthTextures = false;
  pCaps->m_bSupportsFetch4 = false;
}

//-----------------------------------------------------------------------------
// Vendor-dependent code to detect Alpha To Coverage Backdoors
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentAlphaToCoverage(
    HardwareCaps_t *pCaps, int nAdapter) {
  pCaps->m_bSupportsAlphaToCoverage = false;

  if (pCaps->m_nDXSupportLevel < 90) return;

  if (pCaps->m_VendorID == VENDORID_NVIDIA) {
    // nVidia has two modes...assume SSAA is superior to MSAA and hence more
    // desirable (though it's probably not)
    //
    // Currently, they only seem to expose any of this on 7800 and up though
    // older parts certainly support at least the MSAA mode since they support
    // it on OpenGL via the arb_multisample extension
    bool bNVIDIA_MSAA = false;
    bool bNVIDIA_SSAA = false;

    if (m_pD3D->CheckDeviceFormat(
            nAdapter, SOURCE_DX9_DEVICE_TYPE,  // Check MSAA version
            D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE,
            (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C')) == S_OK) {
      bNVIDIA_MSAA = true;
    }

    if (m_pD3D->CheckDeviceFormat(
            nAdapter, SOURCE_DX9_DEVICE_TYPE,  // Check SSAA version
            D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE,
            (D3DFORMAT)MAKEFOURCC('S', 'S', 'A', 'A')) == S_OK) {
      bNVIDIA_SSAA = true;
    }

    // nVidia pitches SSAA but we prefer ATOC
    if (bNVIDIA_MSAA)  //  || bNVIDIA_SSAA )
    {
      //			if ( bNVIDIA_SSAA )
      //				m_AlphaToCoverageEnableValue  =
      // MAKEFOURCC('S', 'S', 'A', 'A'); 			else
      pCaps->m_AlphaToCoverageEnableValue = MAKEFOURCC('A', 'T', 'O', 'C');

      pCaps->m_AlphaToCoverageState = D3DRS_ADAPTIVETESS_Y;
      pCaps->m_AlphaToCoverageDisableValue = (DWORD)D3DFMT_UNKNOWN;
      pCaps->m_bSupportsAlphaToCoverage = true;
      return;
    }
  } else if (pCaps->m_VendorID == VENDORID_ATI) {
    // Supported on all ATI parts...just go ahead and set the state when
    // appropriate
    pCaps->m_AlphaToCoverageState = D3DRS_POINTSIZE;
    pCaps->m_AlphaToCoverageEnableValue = MAKEFOURCC('A', '2', 'M', '1');
    pCaps->m_AlphaToCoverageDisableValue = MAKEFOURCC('A', '2', 'M', '0');
    pCaps->m_bSupportsAlphaToCoverage = true;
    return;
  }
}

ConVar mat_hdr_level("mat_hdr_level", "2");
ConVar mat_slopescaledepthbias_shadowmap("mat_slopescaledepthbias_shadowmap",
                                         "16", FCVAR_CHEAT);
ConVar mat_depthbias_shadowmap("mat_depthbias_shadowmap", "0.0005",
                               FCVAR_CHEAT);

//-----------------------------------------------------------------------------
// Determine capabilities
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ComputeCapsFromD3D(HardwareCaps_t *pCaps,
                                             int nAdapter) {
  D3DCAPS caps;
  // NOTE: When getting the caps, we want to be limited by the hardware
  // even if we're running with software T&L...
  HRESULT hr = m_pD3D->GetDeviceCaps(nAdapter, SOURCE_DX9_DEVICE_TYPE, &caps);
  if (FAILED(hr)) return false;

  D3DADAPTER_IDENTIFIER9 ident;
  hr = m_pD3D->GetAdapterIdentifier(nAdapter, D3DENUM_WHQL_LEVEL, &ident);
  if (FAILED(hr)) return false;

  Q_strncpy(pCaps->m_pDriverName, ident.Description,
            std::min(std::size(ident.Description),
                     (usize)MATERIAL_ADAPTER_NAME_LENGTH));
  pCaps->m_VendorID = ident.VendorId;
  pCaps->m_DeviceID = ident.DeviceId;
  pCaps->m_SubSysID = ident.SubSysId;
  pCaps->m_Revision = ident.Revision;

  pCaps->m_nDriverVersionHigh = ident.DriverVersion.HighPart;
  pCaps->m_nDriverVersionLow = ident.DriverVersion.LowPart;

  pCaps->m_pShaderDLL[0] = '\0';
  pCaps->m_nMaxViewports = 1;

  pCaps->m_PreferDynamicTextures =
      (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) ? 1 : 0;

  pCaps->m_HasProjectedBumpEnv =
      (caps.TextureCaps & D3DPTEXTURECAPS_NOPROJECTEDBUMPENV) == 0;

  pCaps->m_HasSetDeviceGammaRamp =
      (caps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA) != 0;
  pCaps->m_SupportsVertexShaders =
      ((caps.VertexShaderVersion >> 8) & 0xFF) >= 1;
  pCaps->m_SupportsPixelShaders = ((caps.PixelShaderVersion >> 8) & 0xFF) >= 1;

  pCaps->m_bScissorSupported =
      (caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST) != 0;

  pCaps->m_SupportsPixelShaders_1_4 =
      (caps.PixelShaderVersion & 0xffff) >= 0x0104;
  pCaps->m_SupportsPixelShaders_2_0 =
      (caps.PixelShaderVersion & 0xffff) >= 0x0200;
  pCaps->m_SupportsPixelShaders_2_b =
      ((caps.PixelShaderVersion & 0xffff) >= 0x0200) &&
      (caps.PS20Caps.NumInstructionSlots >=
       512);  // More caps to this, but this will do
  pCaps->m_SupportsVertexShaders_2_0 =
      (caps.VertexShaderVersion & 0xffff) >= 0x0200;
  pCaps->m_SupportsShaderModel_3_0 =
      (caps.PixelShaderVersion & 0xffff) >= 0x0300;
  pCaps->m_SupportsMipmappedCubemaps =
      (caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) ? true : false;

  pCaps->m_MaxVertexShader30InstructionSlots = 0;
  pCaps->m_MaxPixelShader30InstructionSlots = 0;

  if (pCaps->m_SupportsShaderModel_3_0) {
    pCaps->m_MaxVertexShader30InstructionSlots =
        caps.MaxVertexShader30InstructionSlots;
    pCaps->m_MaxPixelShader30InstructionSlots =
        caps.MaxPixelShader30InstructionSlots;
  }

  if (CommandLine()->CheckParm("-nops2b")) {
    pCaps->m_SupportsPixelShaders_2_b = false;
  }

  pCaps->m_bSoftwareVertexProcessing = false;
  if (CommandLine()->CheckParm("-mat_softwaretl")) {
    pCaps->m_bSoftwareVertexProcessing = true;
  }

  if (!(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)) {
    // no hardware t&l. . use software
    pCaps->m_bSoftwareVertexProcessing = true;
  }

  // Set mat_forcedynamic if software vertex processing since the software vp
  // pipe has problems with sparse vertex buffres (it transforms the whole
  // thing.)
  if (pCaps->m_bSoftwareVertexProcessing) {
    mat_forcedynamic.SetValue(1);
  }

  if (pCaps->m_bSoftwareVertexProcessing) {
    pCaps->m_SupportsVertexShaders = true;
    pCaps->m_SupportsVertexShaders_2_0 = true;
  }

  // NOTE: Texture stages is a fixed-function concept which also isu
  // NOTE: Normally, the number of texture units == the number of texture
  // stages except for NVidia hardware, which reports more stages than units.
  // The reason for this is because they expose the inner hardware pixel
  // pipeline through the extra stages. The only thing we use stages for
  // in the hardware is for configuring the color + alpha args + ops.
  pCaps->m_NumSamplers = caps.MaxSimultaneousTextures;
  pCaps->m_NumTextureStages = caps.MaxTextureBlendStages;
  if (pCaps->m_SupportsPixelShaders_2_0) {
    pCaps->m_NumSamplers = 16;
  } else {
    Assert(pCaps->m_NumSamplers <= pCaps->m_NumTextureStages);
  }

  // Clamp
  pCaps->m_NumSamplers = std::min(pCaps->m_NumSamplers, (int)MAX_SAMPLERS);
  pCaps->m_NumTextureStages =
      std::min(pCaps->m_NumTextureStages, (int)MAX_TEXTURE_STAGES);

  if (D3DSupportsCompressedTextures()) {
    pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
  } else {
    pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
  }

  pCaps->m_bSupportsAnisotropicFiltering =
      (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
  pCaps->m_bSupportsMagAnisotropicFiltering =
      (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0;
  pCaps->m_nMaxAnisotropy =
      pCaps->m_bSupportsAnisotropicFiltering ? caps.MaxAnisotropy : 1;

  pCaps->m_SupportsCubeMaps =
      (caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) ? true : false;
  pCaps->m_SupportsNonPow2Textures =
      (!(caps.TextureCaps & D3DPTEXTURECAPS_POW2) ||
       (caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL));

  Assert(caps.TextureCaps & D3DPTEXTURECAPS_PROJECTED);

  if (pCaps->m_bSoftwareVertexProcessing) {
    // This should be pushed down based on pixel shaders.
    pCaps->m_NumVertexShaderConstants = 256;
    pCaps->m_NumBooleanVertexShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool vs registers
    pCaps->m_NumBooleanPixelShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool ps registers
    pCaps->m_NumIntegerVertexShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool vs registers
    pCaps->m_NumIntegerPixelShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool ps registers
  } else {
    pCaps->m_NumVertexShaderConstants = caps.MaxVertexShaderConst;
    if (CommandLine()->FindParm("-limitvsconst")) {
      pCaps->m_NumVertexShaderConstants =
          std::min(256, pCaps->m_NumVertexShaderConstants);
    }
    pCaps->m_NumBooleanVertexShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool vs registers
    pCaps->m_NumBooleanPixelShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool ps registers

    // This is a little misleading...this is really 16 int4 registers
    pCaps->m_NumIntegerVertexShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool vs registers
    pCaps->m_NumIntegerPixelShaderConstants =
        pCaps->m_SupportsPixelShaders_2_0
            ? 16
            : 0;  // 2.0 parts have 16 bool ps registers
  }

  if (pCaps->m_SupportsPixelShaders) {
    if (pCaps->m_SupportsPixelShaders_2_0) {
      pCaps->m_NumPixelShaderConstants = 32;
    } else {
      pCaps->m_NumPixelShaderConstants = 8;
    }
  } else {
    pCaps->m_NumPixelShaderConstants = 0;
  }

  pCaps->m_SupportsHardwareLighting =
      (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

  pCaps->m_MaxNumLights = caps.MaxActiveLights;
  if (pCaps->m_MaxNumLights > MAX_NUM_LIGHTS) {
    pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
  }

  if (pCaps->m_bSoftwareVertexProcessing) {
    pCaps->m_SupportsHardwareLighting = true;
    pCaps->m_MaxNumLights = 2;
  }
  pCaps->m_MaxTextureWidth = caps.MaxTextureWidth;
  pCaps->m_MaxTextureHeight = caps.MaxTextureHeight;
  pCaps->m_MaxTextureDepth = caps.MaxVolumeExtent ? caps.MaxVolumeExtent : 1;
  pCaps->m_MaxTextureAspectRatio = caps.MaxTextureAspectRatio;
  if (pCaps->m_MaxTextureAspectRatio == 0) {
    pCaps->m_MaxTextureAspectRatio =
        std::max(pCaps->m_MaxTextureWidth, pCaps->m_MaxTextureHeight);
  }
  pCaps->m_MaxPrimitiveCount = caps.MaxPrimitiveCount;
  pCaps->m_MaxBlendMatrices = caps.MaxVertexBlendMatrices;
  pCaps->m_MaxBlendMatrixIndices = caps.MaxVertexBlendMatrixIndex;

  bool addSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_ADD) != 0;
  bool modSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X) != 0;

  pCaps->m_bNeedsATICentroidHack = false;
  pCaps->m_bDisableShaderOptimizations = false;

  pCaps->m_SupportsMipmapping = true;
  pCaps->m_SupportsOverbright = true;

  // Thank you to all you driver writers who actually correctly return caps
  if (!modSupported || !addSupported) {
    Assert(0);
    pCaps->m_SupportsOverbright = false;
  }

  // Check if ZBias and SlopeScaleDepthBias are supported. .if not, tweak the
  // projection matrix instead for polyoffset.
  pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported =
      ((caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) != 0) &&
      ((caps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS) != 0);

  // Spheremapping supported?
  pCaps->m_bSupportsSpheremapping =
      (caps.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP) != 0;

  // How many user clip planes?
  pCaps->m_MaxUserClipPlanes = caps.MaxUserClipPlanes;
  if (CommandLine()->CheckParm("-nouserclip")) {
    pCaps->m_MaxUserClipPlanes = 0;
  }

  if (pCaps->m_MaxUserClipPlanes > MAXUSERCLIPPLANES) {
    pCaps->m_MaxUserClipPlanes = MAXUSERCLIPPLANES;
  }

  pCaps->m_UseFastClipping = false;

  // query for SRGB support as needed for our DX 9 stuff
  pCaps->m_SupportsSRGB =
      (D3D()->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_SRGBREAD,
                                D3DRTYPE_TEXTURE, D3DFMT_DXT1) == S_OK);

  if (pCaps->m_SupportsSRGB) {
    pCaps->m_SupportsSRGB =
        (D3D()->CheckDeviceFormat(
             nAdapter, SOURCE_DX9_DEVICE_TYPE, D3DFMT_X8R8G8B8,
             D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE,
             D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == S_OK);
  }

  if (CommandLine()->CheckParm("-nosrgb")) {
    pCaps->m_SupportsSRGB = false;
  }

  pCaps->m_bSupportsVertexTextures =
      (D3D()->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_VERTEXTEXTURE,
                                D3DRTYPE_TEXTURE, D3DFMT_R32F) == S_OK);

  // TODO(d.rattman): vs30 has a fixed setting here at 4.
  // Future hardware will need some other way of computing this.
  pCaps->m_nVertexTextureCount = pCaps->m_bSupportsVertexTextures ? 4 : 0;

  // TODO(d.rattman): How do I actually compute this?
  pCaps->m_nMaxVertexTextureDimension =
      pCaps->m_bSupportsVertexTextures ? 4096 : 0;

  // Does the device support filterable int16_t textures?
  bool bSupportsInteger16Textures =
      (D3D()->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER,
                                D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16) == S_OK);

  // Does the device support filterable fp16 textures?
  bool bSupportsFloat16Textures =
      (D3D()->CheckDeviceFormat(nAdapter, SOURCE_DX9_DEVICE_TYPE,
                                D3DFMT_X8R8G8B8, D3DUSAGE_QUERY_FILTER,
                                D3DRTYPE_TEXTURE,
                                D3DFMT_A16B16G16R16F) == S_OK);

  // Does the device support blendable fp16 render targets?
  bool bSupportsFloat16RenderTargets =
      (D3D()->CheckDeviceFormat(
           nAdapter, SOURCE_DX9_DEVICE_TYPE, D3DFMT_X8R8G8B8,
           D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
           D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F) == S_OK);

  // Essentially a proxy for a DX10 device running DX9 code path
  pCaps->m_bSupportsFloat32RenderTargets =
      (D3D()->CheckDeviceFormat(
           nAdapter, SOURCE_DX9_DEVICE_TYPE, D3DFMT_X8R8G8B8,
           D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
           D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F) == S_OK);

  pCaps->m_bFogColorSpecifiedInLinearSpace = false;
  pCaps->m_bFogColorAlwaysLinearSpace = false;

  // Assume not DX10.  Check below.
  pCaps->m_bDX10Card = false;

  // NVidia wants fog color to be specified in linear space
  if (pCaps->m_SupportsSRGB) {
    if (pCaps->m_VendorID == VENDORID_NVIDIA) {
      pCaps->m_bFogColorSpecifiedInLinearSpace = true;

      // On G80, always specify in linear space
      if (pCaps->m_bSupportsFloat32RenderTargets) {
        pCaps->m_bFogColorAlwaysLinearSpace = true;
        pCaps->m_bDX10Card = true;
      }
    } else if (pCaps->m_VendorID == VENDORID_ATI) {
      // Check for DX10 part
      pCaps->m_bDX10Card =
          pCaps->m_SupportsShaderModel_3_0 &&
          (pCaps->m_MaxVertexShader30InstructionSlots > 1024) &&
          (pCaps->m_MaxPixelShader30InstructionSlots > 512);

      if (pCaps->m_bDX10Card) {
        pCaps->m_bFogColorSpecifiedInLinearSpace = true;
        pCaps->m_bFogColorAlwaysLinearSpace = true;
        pCaps->m_bDX10Card = true;
      }
    }
  }

  // Do we have everything necessary to run with integer HDR?  Note that
  // even if we don't support integer 16-bit/component textures, we
  // can still run in this mode if fp16 textures are supported.
  bool bSupportsIntegerHDR =
      pCaps->m_SupportsPixelShaders_2_0 && pCaps->m_SupportsVertexShaders_2_0 &&
      //		(caps.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD)
      //&&
      //		(caps.PrimitiveMiscCaps &
      // D3DPMISCCAPS_SEPARATEALPHABLEND) &&
      (bSupportsInteger16Textures || bSupportsFloat16Textures) &&
      pCaps->m_SupportsSRGB;

  // Do we have everything necessary to run with float HDR?
  bool bSupportsFloatHDR = pCaps->m_SupportsShaderModel_3_0 &&
                           //		(caps.Caps3 &
                           // D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) &&
                           //		(caps.PrimitiveMiscCaps &
                           // D3DPMISCCAPS_SEPARATEALPHABLEND) &&
                           bSupportsFloat16Textures &&
                           bSupportsFloat16RenderTargets &&
                           pCaps->m_SupportsSRGB && !IsX360();

  pCaps->m_MaxHDRType = HDR_TYPE_NONE;
  if (bSupportsFloatHDR)
    pCaps->m_MaxHDRType = HDR_TYPE_FLOAT;
  else if (bSupportsIntegerHDR)
    pCaps->m_MaxHDRType = HDR_TYPE_INTEGER;

  if (bSupportsFloatHDR && (mat_hdr_level.GetInt() == 3)) {
    pCaps->m_HDRType = HDR_TYPE_FLOAT;
  } else if (bSupportsIntegerHDR) {
    pCaps->m_HDRType = HDR_TYPE_INTEGER;
  } else {
    pCaps->m_HDRType = HDR_TYPE_NONE;
  }

  pCaps->m_bColorOnSecondStream = caps.MaxStreams > 1;

  pCaps->m_bSupportsStreamOffset =
      ((caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET) &&  // Tie these caps together
                                                      // since we want to filter
                                                      // out
       pCaps->m_SupportsPixelShaders_2_0);  // any DX8 parts which export
                                            // D3DDEVCAPS2_STREAMOFFSET

  pCaps->m_flMinGammaControlPoint = 0.0f;
  pCaps->m_flMaxGammaControlPoint = 65535.0f;
  pCaps->m_nGammaControlPointCount = 256;

  // Compute the effective DX support level based on all the other caps
  ComputeDXSupportLevel(*pCaps);
  int nCmdlineMaxDXLevel = CommandLine()->ParmValue("-maxdxlevel", 0);
  if (nCmdlineMaxDXLevel > 0) {
    pCaps->m_nMaxDXSupportLevel =
        std::min(pCaps->m_nMaxDXSupportLevel, nCmdlineMaxDXLevel);
  }
  pCaps->m_nDXSupportLevel = pCaps->m_nMaxDXSupportLevel;

  // TODO(d.rattman): m_nDXSupportLevel is uninitialised at this point!! Need to
  // relocate this test:
  int nModelIndex = pCaps->m_nDXSupportLevel < 90 ? VERTEX_SHADER_MODEL - 10
                                                  : VERTEX_SHADER_MODEL;
  pCaps->m_MaxVertexShaderBlendMatrices =
      (pCaps->m_NumVertexShaderConstants - nModelIndex) / 3;

  if (pCaps->m_MaxVertexShaderBlendMatrices > NUM_MODEL_TRANSFORMS) {
    pCaps->m_MaxVertexShaderBlendMatrices = NUM_MODEL_TRANSFORMS;
  }

  CheckNormalCompressionSupport(pCaps, nAdapter);

  CheckBorderColorSupport(pCaps, nAdapter);

  // This may get more complex if we start using multiple flavours of compressed
  // vertex - for now it's "on or off"
  pCaps->m_SupportsCompressedVertices = pCaps->m_nDXSupportLevel >= 90
                                            ? VERTEX_COMPRESSION_ON
                                            : VERTEX_COMPRESSION_NONE;
  if (CommandLine()->CheckParm("-no_compressed_verts")) {
    pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
  }

  // Various vendor-dependent checks...
  CheckVendorDependentAlphaToCoverage(pCaps, nAdapter);
  CheckVendorDependentShadowMappingSupport(pCaps, nAdapter);

  // If we're not on a 3.0 part, these values are more appropriate (X800 & X850
  // parts from ATI do shadow mapping but not 3.0 )
  if (!pCaps->m_SupportsShaderModel_3_0) {
    mat_slopescaledepthbias_shadowmap.SetValue(5.9f);
    mat_depthbias_shadowmap.SetValue(0.003f);
  }

  if (pCaps->m_MaxUserClipPlanes == 0) {
    pCaps->m_UseFastClipping = true;
  }

  pCaps->m_MaxSimultaneousRenderTargets = caps.NumSimultaneousRTs;

  return true;
}

//-----------------------------------------------------------------------------
// Compute the effective DX support level based on all the other caps
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::ComputeDXSupportLevel(HardwareCaps_t &caps) {
  // NOTE: Support level is actually DX level * 10 + subversion
  // So, 70 = DX7, 80 = DX8, 81 = DX8 w/ 1.4 pixel shaders
  // 90 = DX9 w/ 2.0 pixel shaders
  // 95 = DX9 w/ 3.0 pixel shaders and vertex textures
  // 98 = DX9 XBox360
  // NOTE: 82 = NVidia nv3x cards, which can't run dx9 fast

  // TODO(d.rattman): Improve this!! There should be a whole list of features
  // we require in order to be considered a DX7 board, DX8 board, etc.

  if (caps.m_SupportsShaderModel_3_0)  // Note that we don't tie vertex textures
                                       // to 30 shaders anymore
  {
    caps.m_nMaxDXSupportLevel = 95;
    return;
  }

  // NOTE: sRGB is currently required for dx90 because it isn't doing
  // gamma correctly if that feature doesn't exist
  if (caps.m_SupportsVertexShaders_2_0 && caps.m_SupportsPixelShaders_2_0 &&
      caps.m_SupportsSRGB) {
    caps.m_nMaxDXSupportLevel = 90;
    return;
  }

  if (caps.m_SupportsPixelShaders &&
      caps.m_SupportsVertexShaders)  // && caps.m_bColorOnSecondStream)
  {
    if (caps.m_SupportsPixelShaders_1_4) {
      caps.m_nMaxDXSupportLevel = 81;
      return;
    }
    caps.m_nMaxDXSupportLevel = 80;
    return;
  }

  if (caps.m_SupportsCubeMaps && (caps.m_MaxBlendMatrices >= 2)) {
    caps.m_nMaxDXSupportLevel = 70;
    return;
  }

  if ((caps.m_NumSamplers >= 2) && caps.m_SupportsMipmapping) {
    caps.m_nMaxDXSupportLevel = 60;
    return;
  }

  Assert(0);
  // we don't support this!
  caps.m_nMaxDXSupportLevel = 50;
}

//-----------------------------------------------------------------------------
// Gets the number of adapters...
//-----------------------------------------------------------------------------
int CShaderDeviceMgrDx8::GetAdapterCount() const {
  // TODO(d.rattman): Remove call to InitAdapterInfo once Steam startup issues
  // are resolved.
  const_cast<CShaderDeviceMgrDx8 *>(this)->InitAdapterInfo();

  return adapters_.Count();
}

//-----------------------------------------------------------------------------
// Returns info about each adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetAdapterInfo(int nAdapter,
                                         MaterialAdapterInfo_t &info) const {
  // TODO(d.rattman): Remove call to InitAdapterInfo once Steam startup issues
  // are resolved.
  const_cast<CShaderDeviceMgrDx8 *>(this)->InitAdapterInfo();

  Assert((nAdapter >= 0) && (nAdapter < adapters_.Count()));
  const HardwareCaps_t &caps = adapters_[nAdapter].m_ActualCaps;
  memcpy(&info, &caps, sizeof(MaterialAdapterInfo_t));  //-V512
}

//-----------------------------------------------------------------------------
// Sets the adapter
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::SetAdapter(int nAdapter, int nAdapterFlags) {
  LOCK_SHADERAPI();

  // TODO(d.rattman):
  //	g_pShaderDeviceDx8->m_bReadPixelsEnabled = (nAdapterFlags &
  // MATERIAL_INIT_READ_PIXELS_ENABLED) != 0;

  // Set up hardware information for this adapter...
  g_pShaderDeviceDx8->m_DeviceType =
      (nAdapterFlags & MATERIAL_INIT_REFERENCE_RASTERIZER) ? D3DDEVTYPE_REF
                                                           : D3DDEVTYPE_HAL;

  g_pShaderDeviceDx8->m_DisplayAdapter = nAdapter;
  if (g_pShaderDeviceDx8->m_DisplayAdapter >= (UINT)GetAdapterCount()) {
    g_pShaderDeviceDx8->m_DisplayAdapter = 0;
  }

#ifdef NVPERFHUD
  // hack for nvperfhud
  g_pShaderDeviceDx8->m_DisplayAdapter = m_pD3D->GetAdapterCount() - 1;
  g_pShaderDeviceDx8->m_DeviceType = D3DDEVTYPE_REF;
#endif

  // backward compat
  if (!g_pShaderDeviceDx8->OnAdapterSet()) return false;

  //	if ( !g_pShaderDeviceDx8->Init() )
  //	{
  //		Warning( "Unable to initialize dx8 device!\n" );
  //		return false;
  //	}

  g_pShaderDevice = g_pShaderDeviceDx8;

  return true;
}

//-----------------------------------------------------------------------------
// Returns the number of modes
//-----------------------------------------------------------------------------
int CShaderDeviceMgrDx8::GetModeCount(int nAdapter) const {
  LOCK_SHADERAPI();
  Assert(m_pD3D && (nAdapter < GetAdapterCount()));

  // fixme - what format should I use here?
  return m_pD3D->GetAdapterModeCount(nAdapter, D3DFMT_X8R8G8B8);
}

//-----------------------------------------------------------------------------
// Returns mode information..
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetModeInfo(ShaderDisplayMode_t *pInfo, int nAdapter,
                                      int nMode) const {
  Assert(pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION);

  LOCK_SHADERAPI();
  Assert(m_pD3D && (nAdapter < GetAdapterCount()));
  Assert(nMode < GetModeCount(nAdapter));

  HRESULT hr;
  D3DDISPLAYMODE d3dInfo;

  // fixme - what format should I use here?
  hr = D3D()->EnumAdapterModes(nAdapter, D3DFMT_X8R8G8B8, nMode, &d3dInfo);
  Assert(!FAILED(hr));

  pInfo->m_nWidth = d3dInfo.Width;
  pInfo->m_nHeight = d3dInfo.Height;
  pInfo->m_Format = ImageLoader::D3DFormatToImageFormat(d3dInfo.Format);
  pInfo->m_nRefreshRateNumerator = d3dInfo.RefreshRate;
  pInfo->m_nRefreshRateDenominator = 1;
}

//-----------------------------------------------------------------------------
// Returns the current mode information for an adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetCurrentModeInfo(ShaderDisplayMode_t *pInfo,
                                             int nAdapter) const {
  Assert(pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION);

  LOCK_SHADERAPI();
  Assert(D3D());

  D3DDISPLAYMODE mode;
  HRESULT hr = D3D()->GetAdapterDisplayMode(nAdapter, &mode);
  Assert(!FAILED(hr));

  pInfo->m_nWidth = mode.Width;
  pInfo->m_nHeight = mode.Height;
  pInfo->m_Format = ImageLoader::D3DFormatToImageFormat(mode.Format);
  pInfo->m_nRefreshRateNumerator = mode.RefreshRate;
  pInfo->m_nRefreshRateDenominator = 1;
}

//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
CreateInterfaceFn CShaderDeviceMgrDx8::SetMode(void *hWnd, int nAdapter,
                                               const ShaderDeviceInfo_t &mode) {
  LOCK_SHADERAPI();

  Assert(nAdapter < GetAdapterCount());
  int nDXLevel = mode.m_nDXLevel != 0
                     ? mode.m_nDXLevel
                     : adapters_[nAdapter].m_ActualCaps.m_nDXSupportLevel;
  if (m_bObeyDxCommandlineOverride) {
    nDXLevel = CommandLine()->ParmValue("-dxlevel", nDXLevel);
    m_bObeyDxCommandlineOverride = false;
  }

  const int maxAdapterDxSupportLevel{
      adapters_[nAdapter].m_ActualCaps.m_nMaxDXSupportLevel};
  if (nDXLevel > maxAdapterDxSupportLevel) {
    nDXLevel = maxAdapterDxSupportLevel;
  }
  nDXLevel = GetClosestActualDXLevel(nDXLevel);

  if (nDXLevel >= 100) return nullptr;

  bool bReacquireResourcesNeeded = false;
  if (g_pShaderDevice) {
    bReacquireResourcesNeeded = IsPC();
    g_pShaderDevice->ReleaseResources();
  }

  if (g_pShaderAPI) {
    g_pShaderAPI->OnDeviceShutdown();
    g_pShaderAPI = nullptr;
  }

  if (g_pShaderDevice) {
    g_pShaderDevice->ShutdownDevice();
    g_pShaderDevice = nullptr;
  }

  g_pShaderShadow = nullptr;

  ShaderDeviceInfo_t adjustedMode = mode;
  adjustedMode.m_nDXLevel = nDXLevel;
  if (!g_pShaderDeviceDx8->InitDevice(hWnd, nAdapter, adjustedMode))
    return nullptr;

  if (!g_pShaderAPIDX8->OnDeviceInit()) return nullptr;

  g_pShaderDevice = g_pShaderDeviceDx8;
  g_pShaderAPI = g_pShaderAPIDX8;
  g_pShaderShadow = g_pShaderShadowDx8;

  if (bReacquireResourcesNeeded) {
    g_pShaderDevice->ReacquireResources();
  }

  return ShaderInterfaceFactory;
}

//-----------------------------------------------------------------------------
// Validates the mode...
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ValidateMode(int nAdapter,
                                       const ShaderDeviceInfo_t &info) const {
  if (nAdapter >= (int)D3D()->GetAdapterCount()) return false;

  ShaderDisplayMode_t displayMode;
  GetCurrentModeInfo(&displayMode, nAdapter);

  if (info.m_bWindowed) {
    // make sure the window fits within the current video mode
    if ((info.m_DisplayMode.m_nWidth > displayMode.m_nWidth) ||
        (info.m_DisplayMode.m_nHeight > displayMode.m_nHeight))
      return false;
  }

  // Make sure the image format requested is valid
  ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat(
      nAdapter, SOURCE_DX9_DEVICE_TYPE, displayMode.m_Format,
      info.m_DisplayMode.m_Format, info.m_bWindowed);
  return (backBufferFormat != IMAGE_FORMAT_UNKNOWN);
}

//-----------------------------------------------------------------------------
// Returns the amount of video memory in bytes for a particular adapter
//-----------------------------------------------------------------------------
u64 CShaderDeviceMgrDx8::GetVidMemBytes(u32 adapter_idx) const {
  u64 bytes;
  std::tie(bytes, std::ignore) = ::GetVidMemBytes(adapter_idx);
  return bytes;
}

  //-----------------------------------------------------------------------------
  //
  // Shader device
  //
  //-----------------------------------------------------------------------------

#if 0
// TODO(d.rattman): Enable after I've separated it out from shaderapidx8 a little better
static CShaderDeviceDx8 s_ShaderDeviceDX8;
CShaderDeviceDx8* g_pShaderDeviceDx8 = &s_ShaderDeviceDX8;
#endif

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceDx8::CShaderDeviceDx8() {
  m_pD3DDevice = nullptr;
  m_pFrameSyncQueryObject = nullptr;
  m_pFrameSyncTexture = nullptr;
  m_bQueuedDeviceLost = false;
  m_DeviceState = DEVICE_STATE_OK;
  m_bOtherAppInitializing = false;
  m_IsResizing = false;
  m_bPendingVideoModeChange = false;
  m_DeviceSupportsCreateQuery = -1;
  m_bUsingStencil = false;
  m_iStencilBufferBits = 0;
  m_NonInteractiveRefresh.m_Mode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
  m_NonInteractiveRefresh.m_pVertexShader = nullptr;
  m_NonInteractiveRefresh.m_pPixelShader = nullptr;
  m_NonInteractiveRefresh.m_pPixelShaderStartup = nullptr;
  m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 = nullptr;
  m_NonInteractiveRefresh.m_pVertexDecl = nullptr;
  m_NonInteractiveRefresh.m_nPacifierFrame = 0;
  m_numReleaseResourcesRefCount = 0;
}

CShaderDeviceDx8::~CShaderDeviceDx8() {}

//-----------------------------------------------------------------------------
// Computes device creation paramters
//-----------------------------------------------------------------------------
static DWORD ComputeDeviceCreationFlags(D3DCAPS &caps,
                                        bool bSoftwareVertexProcessing) {
  // Find out what type of device to make
  bool bPureDeviceSupported = (caps.DevCaps & D3DDEVCAPS_PUREDEVICE) != 0;

  DWORD nDeviceCreationFlags;
  if (!bSoftwareVertexProcessing) {
    nDeviceCreationFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    if (bPureDeviceSupported) {
      nDeviceCreationFlags |= D3DCREATE_PUREDEVICE;
    }
  } else {
    nDeviceCreationFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
  }
  nDeviceCreationFlags |= D3DCREATE_FPU_PRESERVE;

  return nDeviceCreationFlags;
}

//-----------------------------------------------------------------------------
// Computes the supersample flags
//-----------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE CShaderDeviceDx8::ComputeMultisampleType(int nSampleCount) {
  switch (nSampleCount) {
    case 2:
      return D3DMULTISAMPLE_2_SAMPLES;
    case 3:
      return D3DMULTISAMPLE_3_SAMPLES;
    case 4:
      return D3DMULTISAMPLE_4_SAMPLES;
    case 5:
      return D3DMULTISAMPLE_5_SAMPLES;
    case 6:
      return D3DMULTISAMPLE_6_SAMPLES;
    case 7:
      return D3DMULTISAMPLE_7_SAMPLES;
    case 8:
      return D3DMULTISAMPLE_8_SAMPLES;
    case 9:
      return D3DMULTISAMPLE_9_SAMPLES;
    case 10:
      return D3DMULTISAMPLE_10_SAMPLES;
    case 11:
      return D3DMULTISAMPLE_11_SAMPLES;
    case 12:
      return D3DMULTISAMPLE_12_SAMPLES;
    case 13:
      return D3DMULTISAMPLE_13_SAMPLES;
    case 14:
      return D3DMULTISAMPLE_14_SAMPLES;
    case 15:
      return D3DMULTISAMPLE_15_SAMPLES;
    case 16:
      return D3DMULTISAMPLE_16_SAMPLES;
    default:
    case 0:
    case 1:
      return D3DMULTISAMPLE_NONE;
  }
}

//-----------------------------------------------------------------------------
// Sets the present parameters
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SetPresentParameters(void *hWnd, int nAdapter,
                                            const ShaderDeviceInfo_t &info) {
  ShaderDisplayMode_t mode;
  g_pShaderDeviceMgr->GetCurrentModeInfo(&mode, nAdapter);

  HRESULT hr;
  ZeroMemory(&m_PresentParameters, sizeof(m_PresentParameters));

  m_PresentParameters.Windowed = info.m_bWindowed;
  m_PresentParameters.SwapEffect =
      info.m_bUsingMultipleWindows ? D3DSWAPEFFECT_COPY : D3DSWAPEFFECT_DISCARD;

  // for 360, we want to create it ourselves for hierarchical z support
  m_PresentParameters.EnableAutoDepthStencil = IsX360() ? FALSE : TRUE;

  // What back-buffer format should we use?
  ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat(
      nAdapter, SOURCE_DX9_DEVICE_TYPE, m_AdapterFormat,
      info.m_DisplayMode.m_Format, info.m_bWindowed);

  // What depth format should we use?
  m_bUsingStencil = info.m_bUseStencil;
  if (info.m_nDXLevel >= 80) {
    // always stencil for dx9/hdr
    m_bUsingStencil = true;
  }
  D3DFORMAT nDepthFormat = m_bUsingStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
  m_PresentParameters.AutoDepthStencilFormat = FindNearestSupportedDepthFormat(
      nAdapter, m_AdapterFormat, backBufferFormat, nDepthFormat);
  m_PresentParameters.hDeviceWindow = (HWND)hWnd;

  // store how many stencil buffer bits we have available with the depth/stencil
  // buffer
  switch (m_PresentParameters.AutoDepthStencilFormat) {
    case D3DFMT_D24S8:
      m_iStencilBufferBits = 8;
      break;
    case D3DFMT_D24X4S4:
      m_iStencilBufferBits = 4;
      break;
    case D3DFMT_D15S1:
      m_iStencilBufferBits = 1;
      break;
    default:
      m_iStencilBufferBits = 0;
      m_bUsingStencil = false;  // couldn't acquire a stencil buffer
  };

  if (IsX360() || !info.m_bWindowed) {
    bool useDefault = (info.m_DisplayMode.m_nWidth == 0) ||
                      (info.m_DisplayMode.m_nHeight == 0);
    m_PresentParameters.BackBufferCount = 1;
    m_PresentParameters.BackBufferWidth =
        useDefault ? mode.m_nWidth : info.m_DisplayMode.m_nWidth;
    m_PresentParameters.BackBufferHeight =
        useDefault ? mode.m_nHeight : info.m_DisplayMode.m_nHeight;
    m_PresentParameters.BackBufferFormat =
        ImageLoader::ImageFormatToD3DFormat(backBufferFormat);
    if (!info.m_bWaitForVSync || CommandLine()->FindParm("-forcenovsync")) {
      m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    } else {
      m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }

    m_PresentParameters.FullScreen_RefreshRateInHz =
        info.m_DisplayMode.m_nRefreshRateDenominator
            ? info.m_DisplayMode.m_nRefreshRateNumerator /
                  info.m_DisplayMode.m_nRefreshRateDenominator
            : D3DPRESENT_RATE_DEFAULT;
  } else {
    // NJS: We are seeing a lot of time spent in present in some cases when this
    // isn't set.
    m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (info.m_bResizing) {
      if (info.m_bLimitWindowedSize &&
          (info.m_nWindowedSizeLimitWidth < mode.m_nWidth ||
           info.m_nWindowedSizeLimitHeight < mode.m_nHeight)) {
        // When using material system in windowed resizing apps, it's
        // sometimes not a good idea to allocate stuff as big as the screen
        // video cards can soo run out of resources
        m_PresentParameters.BackBufferWidth = info.m_nWindowedSizeLimitWidth;
        m_PresentParameters.BackBufferHeight = info.m_nWindowedSizeLimitHeight;
      } else {
        // When in resizing windowed mode,
        // we want to allocate enough memory to deal with any resizing...
        m_PresentParameters.BackBufferWidth = mode.m_nWidth;
        m_PresentParameters.BackBufferHeight = mode.m_nHeight;
      }
    } else {
      m_PresentParameters.BackBufferWidth = info.m_DisplayMode.m_nWidth;
      m_PresentParameters.BackBufferHeight = info.m_DisplayMode.m_nHeight;
    }
    m_PresentParameters.BackBufferFormat =
        ImageLoader::ImageFormatToD3DFormat(backBufferFormat);
    m_PresentParameters.BackBufferCount = 1;
  }

  if (info.m_nAASamples > 0 &&
      (m_PresentParameters.SwapEffect == D3DSWAPEFFECT_DISCARD)) {
    D3DMULTISAMPLE_TYPE multiSampleType =
        ComputeMultisampleType(info.m_nAASamples);
    DWORD nQualityLevel;

    // TODO(d.rattman): Should we add the quality level to the
    // ShaderAdapterMode_t struct? 16x on nVidia refers to CSAA or "Coverage
    // Sampled Antialiasing"
    const HardwareCaps_t &adapterCaps =
        g_ShaderDeviceMgrDx8.GetHardwareCaps(nAdapter);
    if ((info.m_nAASamples == 16) &&
        (adapterCaps.m_VendorID == VENDORID_NVIDIA)) {
      multiSampleType = ComputeMultisampleType(4);
      hr = D3D()->CheckDeviceMultiSampleType(
          nAdapter, SOURCE_DX9_DEVICE_TYPE,
          m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed,
          multiSampleType,
          &nQualityLevel);  // 4x at highest quality level

      if (!FAILED(hr) && (nQualityLevel == 16)) {
        nQualityLevel =
            nQualityLevel - 1;  // Highest quality level triggers 16x CSAA
      } else {
        nQualityLevel = 0;  // No CSAA
      }
    } else  // Regular MSAA on any old vendor
    {
      hr = D3D()->CheckDeviceMultiSampleType(
          nAdapter, SOURCE_DX9_DEVICE_TYPE,
          m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed,
          multiSampleType, &nQualityLevel);

      nQualityLevel = 0;
    }

    if (!FAILED(hr)) {
      m_PresentParameters.MultiSampleType = multiSampleType;
      m_PresentParameters.MultiSampleQuality = nQualityLevel;
    }
  } else {
    m_PresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    m_PresentParameters.MultiSampleQuality = 0;
  }
}

//-----------------------------------------------------------------------------
// Initializes, shuts down the D3D device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::InitDevice(void *hwnd, int nAdapter,
                                  const ShaderDeviceInfo_t &info) {
  // windowed
  if (!CreateD3DDevice((HWND)hwnd, nAdapter, info)) return false;

  // Hook up our own windows proc to get at messages to tell us when
  // other instances of the material system are trying to set the mode
  InstallWindowHook((HWND)m_hWnd);
  return true;
}

void CShaderDeviceDx8::ShutdownDevice() {
  if (IsActive()) {
    Dx9Device()->Release();

#ifdef STUBD3D
    delete (CStubD3DDevice *)Dx9Device();
#endif
    Dx9Device()->ShutDownDevice();

    RemoveWindowHook((HWND)m_hWnd);
    m_hWnd = 0;
  }
}

//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::IsUsingGraphics() const {
  //*****LOCK_SHADERAPI();
  return IsActive();
}

//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
int CShaderDeviceDx8::GetCurrentAdapter() const {
  LOCK_SHADERAPI();
  return m_DisplayAdapter;
}

//-----------------------------------------------------------------------------
// Use this to spew information about the 3D layer
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SpewDriverInfo() const {
  LOCK_SHADERAPI();
  HRESULT hr;
  D3DCAPS caps;
  D3DADAPTER_IDENTIFIER9 ident;

  RECORD_COMMAND(DX8_GET_DEVICE_CAPS, 0);

  RECORD_COMMAND(DX8_GET_ADAPTER_IDENTIFIER, 2);
  RECORD_INT(m_nAdapter);
  RECORD_INT(0);

  Dx9Device()->GetDeviceCaps(&caps);
  hr = D3D()->GetAdapterIdentifier(m_nAdapter, D3DENUM_WHQL_LEVEL, &ident);

  Warning("Shader API Driver Info:\n\nDriver : %s Version : %d\n", ident.Driver,
          ident.DriverVersion);
  Warning("Driver Description :  %s\n", ident.Description);
  Warning("Chipset version %d %d %d %d\n\n", ident.VendorId, ident.DeviceId,
          ident.SubSysId, ident.Revision);

  ShaderDisplayMode_t mode;
  g_pShaderDeviceMgr->GetCurrentModeInfo(&mode, m_nAdapter);
  Warning("Display mode : %d x %d (%s)\n", mode.m_nWidth, mode.m_nHeight,
          ImageLoader::GetName(mode.m_Format));
  Warning(
      "Vertex Shader Version : %d.%d Pixel Shader Version : %d.%d\n",
      (caps.VertexShaderVersion >> 8) & 0xFF, caps.VertexShaderVersion & 0xFF,
      (caps.PixelShaderVersion >> 8) & 0xFF, caps.PixelShaderVersion & 0xFF);
  Warning("\nDevice Caps :\n");
  Warning("CANBLTSYSTONONLOCAL %s CANRENDERAFTERFLIP %s HWRASTERIZATION %s\n",
          (caps.DevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL) ? " Y " : " N ",
          (caps.DevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP) ? " Y " : " N ",
          (caps.DevCaps & D3DDEVCAPS_HWRASTERIZATION) ? " Y " : "*N*");
  Warning("HWTRANSFORMANDLIGHT %s NPATCHES %s PUREDEVICE %s\n",
          (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ? " Y " : " N ",
          (caps.DevCaps & D3DDEVCAPS_NPATCHES) ? " Y " : " N ",
          (caps.DevCaps & D3DDEVCAPS_PUREDEVICE) ? " Y " : " N ");
  Warning(
      "SEPARATETEXTUREMEMORIES %s TEXTURENONLOCALVIDMEM %s TEXTURESYSTEMMEMORY "
      "%s\n",
      (caps.DevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES) ? "*Y*" : " N ",
      (caps.DevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM) ? " Y " : " N ",
      (caps.DevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY) ? " Y " : " N ");
  Warning(
      "TEXTUREVIDEOMEMORY %s TLVERTEXSYSTEMMEMORY %s TLVERTEXVIDEOMEMORY %s\n",
      (caps.DevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY) ? " Y " : "*N*",
      (caps.DevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY) ? " Y " : "*N*",
      (caps.DevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY) ? " Y " : " N ");

  Warning("\nPrimitive Caps :\n");
  Warning("BLENDOP %s CLIPPLANESCALEDPOINTS %s CLIPTLVERTS %s\n",
          (caps.PrimitiveMiscCaps & D3DPMISCCAPS_BLENDOP) ? " Y " : " N ",
          (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPPLANESCALEDPOINTS) ? " Y "
                                                                        : " N ",
          (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS) ? " Y " : " N ");
  Warning(
      "COLORWRITEENABLE %s MASKZ %s TSSARGTEMP %s\n",
      (caps.PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) ? " Y " : " N ",
      (caps.PrimitiveMiscCaps & D3DPMISCCAPS_MASKZ) ? " Y " : "*N*",
      (caps.PrimitiveMiscCaps & D3DPMISCCAPS_TSSARGTEMP) ? " Y " : " N ");

  Warning("\nRaster Caps :\n");
  Warning("FOGRANGE %s FOGTABLE %s FOGVERTEX %s ZFOG %s WFOG %s\n",
          (caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_FOGVERTEX) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_ZFOG) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_WFOG) ? " Y " : " N ");
  Warning("MIPMAPLODBIAS %s WBUFFER %s ZBIAS %s ZTEST %s\n",
          (caps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_WBUFFER) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) ? " Y " : " N ",
          (caps.RasterCaps & D3DPRASTERCAPS_ZTEST) ? " Y " : "*N*");

  Warning("Size of Texture Memory : %llu kb\n",
          g_pHardwareConfig->Caps().m_TextureMemorySize / 1024);
  Warning("Max Texture Dimensions : %d x %d\n", caps.MaxTextureWidth,
          caps.MaxTextureHeight);
  if (caps.MaxTextureAspectRatio != 0)
    Warning("Max Texture Aspect Ratio : *%d*\n", caps.MaxTextureAspectRatio);
  Warning("Max Textures : %d Max Stages : %d\n", caps.MaxSimultaneousTextures,
          caps.MaxTextureBlendStages);

  Warning("\nTexture Caps :\n");
  Warning("ALPHA %s CUBEMAP %s MIPCUBEMAP %s SQUAREONLY %s\n",
          (caps.TextureCaps & D3DPTEXTURECAPS_ALPHA) ? " Y " : " N ",
          (caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) ? " Y " : " N ",
          (caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) ? " Y " : " N ",
          (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) ? "*Y*" : " N ");

  Warning("vendor id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_VendorID);
  Warning("device id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_DeviceID);

  Warning("SHADERAPI CAPS:\n");
  Warning("m_NumSamplers: %d\n", g_pHardwareConfig->Caps().m_NumSamplers);
  Warning("m_NumTextureStages: %d\n",
          g_pHardwareConfig->Caps().m_NumTextureStages);
  Warning("m_HasSetDeviceGammaRamp: %s\n",
          g_pHardwareConfig->Caps().m_HasSetDeviceGammaRamp ? "yes" : "no");
  Warning("m_SupportsVertexShaders (1.1): %s\n",
          g_pHardwareConfig->Caps().m_SupportsVertexShaders ? "yes" : "no");
  Warning("m_SupportsVertexShaders_2_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 ? "yes" : "no");
  Warning("m_SupportsPixelShaders (1.1): %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders ? "yes" : "no");
  Warning("m_SupportsPixelShaders_1_4: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_1_4 ? "yes" : "no");
  Warning("m_SupportsPixelShaders_2_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_0 ? "yes" : "no");
  Warning("m_SupportsPixelShaders_2_b: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_b ? "yes" : "no");
  Warning("m_SupportsShaderModel_3_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_3_0 ? "yes" : "no");

  switch (g_pHardwareConfig->Caps().m_SupportsCompressedTextures) {
    case COMPRESSED_TEXTURES_ON:
      Warning("m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n");
      break;
    case COMPRESSED_TEXTURES_OFF:
      Warning("m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n");
      break;
    case COMPRESSED_TEXTURES_NOT_INITIALIZED:
      Warning(
          "m_SupportsCompressedTextures: "
          "COMPRESSED_TEXTURES_NOT_INITIALIZED\n");
      break;
    default:
      Assert(0);
      break;
  }
  Warning("m_SupportsCompressedVertices: %d\n",
          g_pHardwareConfig->Caps().m_SupportsCompressedVertices);
  Warning(
      "m_bSupportsAnisotropicFiltering: %s\n",
      g_pHardwareConfig->Caps().m_bSupportsAnisotropicFiltering ? "yes" : "no");
  Warning("m_nMaxAnisotropy: %d\n", g_pHardwareConfig->Caps().m_nMaxAnisotropy);
  Warning("m_MaxTextureWidth: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureWidth);
  Warning("m_MaxTextureHeight: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureHeight);
  Warning("m_MaxTextureAspectRatio: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureAspectRatio);
  Warning("m_MaxPrimitiveCount: %d\n",
          g_pHardwareConfig->Caps().m_MaxPrimitiveCount);
  Warning("m_ZBiasAndSlopeScaledDepthBiasSupported: %s\n",
          g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported
              ? "yes"
              : "no");
  Warning("m_SupportsMipmapping: %s\n",
          g_pHardwareConfig->Caps().m_SupportsMipmapping ? "yes" : "no");
  Warning("m_SupportsOverbright: %s\n",
          g_pHardwareConfig->Caps().m_SupportsOverbright ? "yes" : "no");
  Warning("m_SupportsCubeMaps: %s\n",
          g_pHardwareConfig->Caps().m_SupportsCubeMaps ? "yes" : "no");
  Warning("m_NumPixelShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumPixelShaderConstants);
  Warning("m_NumVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumVertexShaderConstants);
  Warning("m_NumBooleanVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants);
  Warning("m_NumIntegerVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants);
  Warning("m_TextureMemorySize: %llu\n",
          g_pHardwareConfig->Caps().m_TextureMemorySize);
  Warning("m_MaxNumLights: %d\n", g_pHardwareConfig->Caps().m_MaxNumLights);
  Warning("m_SupportsHardwareLighting: %s\n",
          g_pHardwareConfig->Caps().m_SupportsHardwareLighting ? "yes" : "no");
  Warning("m_MaxBlendMatrices: %d\n",
          g_pHardwareConfig->Caps().m_MaxBlendMatrices);
  Warning("m_MaxBlendMatrixIndices: %d\n",
          g_pHardwareConfig->Caps().m_MaxBlendMatrixIndices);
  Warning("m_MaxVertexShaderBlendMatrices: %d\n",
          g_pHardwareConfig->Caps().m_MaxVertexShaderBlendMatrices);
  Warning("m_SupportsMipmappedCubemaps: %s\n",
          g_pHardwareConfig->Caps().m_SupportsMipmappedCubemaps ? "yes" : "no");
  Warning("m_SupportsNonPow2Textures: %s\n",
          g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ? "yes" : "no");
  Warning("m_nDXSupportLevel: %d\n",
          g_pHardwareConfig->Caps().m_nDXSupportLevel);
  Warning("m_PreferDynamicTextures: %s\n",
          g_pHardwareConfig->Caps().m_PreferDynamicTextures ? "yes" : "no");
  Warning("m_HasProjectedBumpEnv: %s\n",
          g_pHardwareConfig->Caps().m_HasProjectedBumpEnv ? "yes" : "no");
  Warning("m_MaxUserClipPlanes: %d\n",
          g_pHardwareConfig->Caps().m_MaxUserClipPlanes);
  Warning("m_SupportsSRGB: %s\n",
          g_pHardwareConfig->Caps().m_SupportsSRGB ? "yes" : "no");
  switch (g_pHardwareConfig->Caps().m_HDRType) {
    case HDR_TYPE_NONE:
      Warning("m_HDRType: HDR_TYPE_NONE\n");
      break;
    case HDR_TYPE_INTEGER:
      Warning("m_HDRType: HDR_TYPE_INTEGER\n");
      break;
    case HDR_TYPE_FLOAT:
      Warning("m_HDRType: HDR_TYPE_FLOAT\n");
      break;
    default:
      Assert(0);
      break;
  }
  Warning("m_bSupportsSpheremapping: %s\n",
          g_pHardwareConfig->Caps().m_bSupportsSpheremapping ? "yes" : "no");
  Warning("m_UseFastClipping: %s\n",
          g_pHardwareConfig->Caps().m_UseFastClipping ? "yes" : "no");
  Warning("m_pShaderDLL: %s\n", g_pHardwareConfig->Caps().m_pShaderDLL);
  Warning("m_bNeedsATICentroidHack: %s\n",
          g_pHardwareConfig->Caps().m_bNeedsATICentroidHack ? "yes" : "no");
  Warning(
      "m_bDisableShaderOptimizations: %s\n",
      g_pHardwareConfig->Caps().m_bDisableShaderOptimizations ? "yes" : "no");
  Warning("m_bColorOnSecondStream: %s\n",
          g_pHardwareConfig->Caps().m_bColorOnSecondStream ? "yes" : "no");
  Warning("m_MaxSimultaneousRenderTargets: %d\n",
          g_pHardwareConfig->Caps().m_MaxSimultaneousRenderTargets);
}

//-----------------------------------------------------------------------------
// Back buffer information
//-----------------------------------------------------------------------------
ImageFormat CShaderDeviceDx8::GetBackBufferFormat() const {
  return ImageLoader::D3DFormatToImageFormat(
      m_PresentParameters.BackBufferFormat);
}

void CShaderDeviceDx8::GetBackBufferDimensions(int &width, int &height) const {
  width = m_PresentParameters.BackBufferWidth;
  height = m_PresentParameters.BackBufferHeight;
}

//-----------------------------------------------------------------------------
// Detects support for CreateQuery
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::DetectQuerySupport(IDirect3DDevice9 *pD3DDevice) {
  // Do I need to detect whether this device supports CreateQuery before
  // creating it?
  if (m_DeviceSupportsCreateQuery != -1) return;

  IDirect3DQuery9 *pQueryObject = nullptr;

  // Detect whether query is supported by creating and releasing:
  HRESULT hr = pD3DDevice->CreateQuery(D3DQUERYTYPE_EVENT, &pQueryObject);
  if (!FAILED(hr) && pQueryObject) {
    pQueryObject->Release();
    m_DeviceSupportsCreateQuery = 1;
  } else {
    m_DeviceSupportsCreateQuery = 0;
  }
}

//-----------------------------------------------------------------------------
// Actually creates the D3D Device once the present parameters are set up
//-----------------------------------------------------------------------------
IDirect3DDevice9 *CShaderDeviceDx8::InvokeCreateDevice(
    void *hWnd, int nAdapter, DWORD deviceCreationFlags) {
  IDirect3DDevice9 *pD3DDevice = nullptr;
  D3DDEVTYPE devType = SOURCE_DX9_DEVICE_TYPE;

#if NVPERFHUD
  nAdapter = D3D()->GetAdapterCount() - 1;
  devType = D3DDEVTYPE_REF;
  deviceCreationFlags =
      D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING;
#endif

  HRESULT hr =
      D3D()->CreateDevice(nAdapter, devType, (HWND)hWnd, deviceCreationFlags,
                          &m_PresentParameters, &pD3DDevice);

  if (!FAILED(hr) && pD3DDevice) return pD3DDevice;

  if (!IsPC()) return nullptr;

  // try again, other applications may be taking their time
  Sleep(1000);
  hr = D3D()->CreateDevice(nAdapter, devType, (HWND)hWnd, deviceCreationFlags,
                           &m_PresentParameters, &pD3DDevice);
  if (!FAILED(hr) && pD3DDevice) return pD3DDevice;

  // in this case, we actually are allocating too much memory....
  // This will cause us to use less buffers...
  if (m_PresentParameters.Windowed) {
    m_PresentParameters.SwapEffect = D3DSWAPEFFECT_COPY;
    m_PresentParameters.BackBufferCount = 0;
    hr = D3D()->CreateDevice(nAdapter, devType, (HWND)hWnd, deviceCreationFlags,
                             &m_PresentParameters, &pD3DDevice);
  }
  if (!FAILED(hr) && pD3DDevice) return pD3DDevice;

  // Otherwise we failed, show a message and shutdown
  DWarning(
      "init", 0,
      "Failed to create D3D device! Please see the following for more info.\n"
      "https://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/"
      "std_adp.php?p_faqid=772\n");

  return nullptr;
}

//-----------------------------------------------------------------------------
// Creates the D3D Device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::CreateD3DDevice(void *pHWnd, int nAdapter,
                                       const ShaderDeviceInfo_t &info) {
  Assert(info.m_nVersion == SHADER_DEVICE_INFO_VERSION);

  MEM_ALLOC_CREDIT_(__FILE__ ": D3D Device");

  HWND hWnd = (HWND)pHWnd;

#if !defined(PIX_INSTRUMENTATION)
  D3DPERF_SetOptions(
      1);  // Explicitly disallow PIX instrumented profiling in external builds
#endif

  // Get some caps....
  D3DCAPS caps;
  HRESULT hr = D3D()->GetDeviceCaps(nAdapter, SOURCE_DX9_DEVICE_TYPE, &caps);
  if (FAILED(hr)) return false;

  // Determine the adapter format
  ShaderDisplayMode_t mode;
  g_pShaderDeviceMgrDx8->GetCurrentModeInfo(&mode, nAdapter);
  m_AdapterFormat = mode.m_Format;

  // TODO(d.rattman): Need to do this prior to SetPresentParameters. Fix.
  // Make it part of HardwareCaps_t
  InitializeColorInformation(nAdapter, SOURCE_DX9_DEVICE_TYPE, m_AdapterFormat);

  const HardwareCaps_t &adapterCaps =
      g_ShaderDeviceMgrDx8.GetHardwareCaps(nAdapter);
  DWORD deviceCreationFlags =
      ComputeDeviceCreationFlags(caps, adapterCaps.m_bSoftwareVertexProcessing);
  SetPresentParameters(hWnd, nAdapter, info);

  // Tell all other instances of the material system to let go of memory
  SendIPCMessage(RELEASE_MESSAGE);

  // Creates the device
  IDirect3DDevice9 *pD3DDevice =
      InvokeCreateDevice(pHWnd, nAdapter, deviceCreationFlags);

  if (!pD3DDevice) return false;

  // Check to see if query is supported
  DetectQuerySupport(pD3DDevice);

#ifdef STUBD3D
  Dx9Device() = new CStubD3DDevice(pD3DDevice, g_pFullFileSystem);
#else
  Dx9Device()->SetDevicePtr(pD3DDevice);
#endif

  // CheckDeviceLost();

  // Tell all other instances of the material system it's ok to grab memory
  SendIPCMessage(REACQUIRE_MESSAGE);

  m_hWnd = pHWnd;
  m_nAdapter = m_DisplayAdapter = nAdapter;
  m_DeviceState = DEVICE_STATE_OK;
  m_bIsMinimized = false;
  m_bQueuedDeviceLost = false;

  m_IsResizing = info.m_bWindowed && info.m_bResizing;

  // This is our current view.
  m_ViewHWnd = hWnd;
  GetWindowSize(m_nWindowWidth, m_nWindowHeight);

  g_pHardwareConfig->SetupHardwareCaps(
      info, g_ShaderDeviceMgrDx8.GetHardwareCaps(nAdapter));

  // TODO(d.rattman): Bake this into hardware config
  // What texture formats do we support?
  if (D3DSupportsCompressedTextures()) {
    g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures =
        COMPRESSED_TEXTURES_ON;
  } else {
    g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures =
        COMPRESSED_TEXTURES_OFF;
  }

  return (!FAILED(hr));
}

//-----------------------------------------------------------------------------
// Frame sync
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::AllocFrameSyncTextureObject() {
  FreeFrameSyncTextureObject();

  // Create a tiny managed texture.
  HRESULT hr = Dx9Device()->CreateTexture(1, 1,              // width, height
                                          0,                 // levels
                                          D3DUSAGE_DYNAMIC,  // usage
                                          D3DFMT_A8R8G8B8,   // format
                                          D3DPOOL_DEFAULT, &m_pFrameSyncTexture,
                                          nullptr);
  if (FAILED(hr)) {
    m_pFrameSyncTexture = nullptr;
  }
}

void CShaderDeviceDx8::FreeFrameSyncTextureObject() {
  if (m_pFrameSyncTexture) {
    m_pFrameSyncTexture->Release();
    m_pFrameSyncTexture = nullptr;
  }
}

void CShaderDeviceDx8::AllocFrameSyncObjects() {
  if (mat_debugalttab.GetBool()) {
    Warning("mat_debugalttab: CShaderAPIDX8::AllocFrameSyncObjects\n");
  }

  // Allocate the texture for frame syncing in case we force that to be on.
  AllocFrameSyncTextureObject();

  if (m_DeviceSupportsCreateQuery == 0) {
    m_pFrameSyncQueryObject = nullptr;
    return;
  }

  // TODO(d.rattman): Need to record this.
  HRESULT hr =
      Dx9Device()->CreateQuery(D3DQUERYTYPE_EVENT, &m_pFrameSyncQueryObject);
  if (hr == D3DERR_NOTAVAILABLE) {
    Warning("D3DQUERYTYPE_EVENT not available on this driver\n");
    Assert(m_pFrameSyncQueryObject == nullptr);
  } else {
    Assert(hr == D3D_OK);
    Assert(m_pFrameSyncQueryObject);
    m_pFrameSyncQueryObject->Issue(D3DISSUE_END);
  }
}

void CShaderDeviceDx8::FreeFrameSyncObjects() {
  if (mat_debugalttab.GetBool()) {
    Warning("mat_debugalttab: CShaderAPIDX8::FreeFrameSyncObjects\n");
  }

  FreeFrameSyncTextureObject();

  // TODO(d.rattman): Need to record this.
  if (m_pFrameSyncQueryObject) {
#ifdef _DEBUG
    int nRetVal =
#endif
        m_pFrameSyncQueryObject->Release();
    Assert(nRetVal == 0);
    m_pFrameSyncQueryObject = nullptr;
  }
}

//-----------------------------------------------------------------------------
// Occurs when another application is initializing
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::OtherAppInitializing(bool initializing) {
  Assert(m_bOtherAppInitializing != initializing);

  if (!IsDeactivated()) {
    Dx9Device()->EndScene();
  }

  // NOTE: OtherApp is set in this way because we need to know we're
  // active as we release and restore everything
  CheckDeviceLost(initializing);

  if (!IsDeactivated()) {
    Dx9Device()->BeginScene();
  }
}

//-----------------------------------------------------------------------------
// We lost the device, but we have a chance to recover
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::TryDeviceReset() {
  // TODO(d.rattman): Make this rebuild the Dx9Device from scratch!
  // Helps with compatibility
  HRESULT hr = Dx9Device()->Reset(&m_PresentParameters);
  return !FAILED(hr);
}

//-----------------------------------------------------------------------------
// Release, reacquire resources
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::ReleaseResources() {
  // Only the initial "ReleaseResources" actually has effect
  if (m_numReleaseResourcesRefCount++ != 0) {
    Warning("ReleaseResources has no effect, now at level %d.\n",
            m_numReleaseResourcesRefCount);
    DevWarning(
        "ReleaseResources called twice is a bug: use IsDeactivated to check "
        "for a valid device.\n");
    Assert(0);
    return;
  }

  LOCK_SHADERAPI();
  CPixEvent(PIX_VALVE_ORANGE, "ReleaseResources");

  FreeFrameSyncObjects();
  FreeNonInteractiveRefreshObjects();
  ShaderUtil()->ReleaseShaderObjects();
  MeshMgr()->ReleaseBuffers();
  g_pShaderAPI->ReleaseShaderObjects();

#ifdef _DEBUG
  if (MeshMgr()->BufferCount() != 0) {
    for (int i = 0; i < MeshMgr()->BufferCount(); i++) {
    }
  }
#endif

  // All meshes cleaned up?
  Assert(MeshMgr()->BufferCount() == 0);
}

void CShaderDeviceDx8::ReacquireResources() { ReacquireResourcesInternal(); }

void CShaderDeviceDx8::ReacquireResourcesInternal(bool bResetState,
                                                  bool bForceReacquire,
                                                  char const *pszForceReason) {
  if (bForceReacquire) {
    // If we are forcing reacquire then warn if release calls are remaining
    // unpaired
    if (m_numReleaseResourcesRefCount > 1) {
      Warning(
          "Forcefully resetting device (%s), resources release level was %d.\n",
          pszForceReason ? pszForceReason : "unspecified",
          m_numReleaseResourcesRefCount);
      Assert(0);
    }
    m_numReleaseResourcesRefCount = 0;
  } else {
    // Only the final "ReacquireResources" actually has effect
    if (--m_numReleaseResourcesRefCount != 0) {
      Warning("ReacquireResources has no effect, now at level %d.\n",
              m_numReleaseResourcesRefCount);
      DevWarning(
          "ReacquireResources being discarded is a bug: use IsDeactivated to "
          "check for a valid device.\n");
      Assert(0);

      if (m_numReleaseResourcesRefCount < 0) {
        m_numReleaseResourcesRefCount = 0;
      }

      return;
    }
  }

  if (bResetState) {
    ResetRenderState();
  }

  LOCK_SHADERAPI();
  CPixEvent event(PIX_VALVE_ORANGE, "ReacquireResources");

  g_pShaderAPI->RestoreShaderObjects();
  AllocFrameSyncObjects();
  AllocNonInteractiveRefreshObjects();
  MeshMgr()->RestoreBuffers();
  ShaderUtil()->RestoreShaderObjects(
      CShaderDeviceMgrBase::ShaderInterfaceFactory);
}

//-----------------------------------------------------------------------------
// Changes the window size
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::ResizeWindow(const ShaderDeviceInfo_t &info) {
  if (IsX360()) return false;

  m_bPendingVideoModeChange = false;

  // We don't need to do crap if the window was set up to set up
  // to be resizing...
  if (info.m_bResizing) return false;

  g_pShaderDeviceMgr->InvokeModeChangeCallbacks();

  ReleaseResources();

  SetPresentParameters((HWND)m_hWnd, m_DisplayAdapter, info);
  HRESULT hr = Dx9Device()->Reset(&m_PresentParameters);
  if (FAILED(hr)) {
    Warning("ResizeWindow: Reset failed, hr = 0x%08X.\n", hr);
    return false;
  } else {
    ReacquireResourcesInternal(true, true, "ResizeWindow");
  }

  return true;
}

//-----------------------------------------------------------------------------
// Queue up the fact that the device was lost
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::MarkDeviceLost() {
  if (IsX360()) return;

  m_bQueuedDeviceLost = true;
}

//-----------------------------------------------------------------------------
// Checks if the device was lost
//-----------------------------------------------------------------------------
#if defined(_DEBUG)
ConVar mat_forcelostdevice("mat_forcelostdevice", "0");
#endif

void CShaderDeviceDx8::CheckDeviceLost(bool bOtherAppInitializing) {
  // TODO(d.rattman): We could also queue up if WM_SIZE changes and look at that
  // but that seems to only make sense if we have resizable windows where
  // we do *not* allocate buffers as large as the entire current video mode
  // which we're not doing
  m_bIsMinimized = (IsIconic((HWND)m_hWnd) != FALSE);
  m_bOtherAppInitializing = bOtherAppInitializing;

  RECORD_COMMAND(DX8_TEST_COOPERATIVE_LEVEL, 0);
  HRESULT hr = Dx9Device()->TestCooperativeLevel();

#ifdef _DEBUG
  if (mat_forcelostdevice.GetBool()) {
    mat_forcelostdevice.SetValue(0);
    MarkDeviceLost();
  }
#endif

  // If some other call returned device lost previously in the frame, spoof the
  // return value from TCL
  if (m_bQueuedDeviceLost) {
    hr = (hr != D3D_OK) ? hr : D3DERR_DEVICENOTRESET;
    m_bQueuedDeviceLost = false;
  }

  if (m_DeviceState == DEVICE_STATE_OK) {
    // We can transition out of ok if bOtherAppInitializing is set
    // or if we become minimized, or if TCL returns anything other than D3D_OK.
    if ((hr != D3D_OK) || m_bIsMinimized) {
      // We were ok, now we're not. Release resources
      ReleaseResources();
      m_DeviceState = DEVICE_STATE_LOST_DEVICE;
    } else if (bOtherAppInitializing) {
      // We were ok, now we're not. Release resources
      ReleaseResources();
      m_DeviceState = DEVICE_STATE_OTHER_APP_INIT;
    }
  }

  // Immediately checking devicelost after ok helps in the case where we got
  // D3DERR_DEVICENOTRESET in which case we want to immdiately try to switch out
  // of DEVICE_STATE_LOST and into DEVICE_STATE_NEEDS_RESET
  if (m_DeviceState == DEVICE_STATE_LOST_DEVICE) {
    // We can only try to reset if we're not minimized and not lost
    if (!m_bIsMinimized && (hr != D3DERR_DEVICELOST)) {
      m_DeviceState = DEVICE_STATE_NEEDS_RESET;
    }
  }

  // Immediately checking needs reset also helps for the case where we got
  // D3DERR_DEVICENOTRESET
  if (m_DeviceState == DEVICE_STATE_NEEDS_RESET) {
    if ((hr == D3DERR_DEVICELOST) || m_bIsMinimized) {
      m_DeviceState = DEVICE_STATE_LOST_DEVICE;
    } else {
      bool bResetSucceeded = TryDeviceReset();
      if (bResetSucceeded) {
        if (!bOtherAppInitializing) {
          m_DeviceState = DEVICE_STATE_OK;

          // We were bad, now we're ok. Restore resources and reset render
          // state.
          ReacquireResourcesInternal(true, true, "NeedsReset");
        } else {
          m_DeviceState = DEVICE_STATE_OTHER_APP_INIT;
        }
      }
    }
  }

  if (m_DeviceState == DEVICE_STATE_OTHER_APP_INIT) {
    if ((hr != D3D_OK) || m_bIsMinimized) {
      m_DeviceState = DEVICE_STATE_LOST_DEVICE;
    } else if (!bOtherAppInitializing) {
      m_DeviceState = DEVICE_STATE_OK;

      // We were bad, now we're ok. Restore resources and reset render state.
      ReacquireResourcesInternal(true, true, "OtherAppInit");
    }
  }

  // Do mode change if we have a video mode change.
  if (m_bPendingVideoModeChange && !IsDeactivated()) {
#ifdef _DEBUG
    Warning("mode change!\n");
#endif
    ResizeWindow(m_PendingVideoModeChangeConfig);
  }
}

//-----------------------------------------------------------------------------
// Special method to refresh the screen on the XBox360
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::AllocNonInteractiveRefreshObjects() { return true; }

void CShaderDeviceDx8::FreeNonInteractiveRefreshObjects() {
  if (m_NonInteractiveRefresh.m_pVertexShader) {
    m_NonInteractiveRefresh.m_pVertexShader->Release();
    m_NonInteractiveRefresh.m_pVertexShader = nullptr;
  }

  if (m_NonInteractiveRefresh.m_pPixelShader) {
    m_NonInteractiveRefresh.m_pPixelShader->Release();
    m_NonInteractiveRefresh.m_pPixelShader = nullptr;
  }

  if (m_NonInteractiveRefresh.m_pPixelShaderStartup) {
    m_NonInteractiveRefresh.m_pPixelShaderStartup->Release();
    m_NonInteractiveRefresh.m_pPixelShaderStartup = nullptr;
  }

  if (m_NonInteractiveRefresh.m_pPixelShaderStartupPass2) {
    m_NonInteractiveRefresh.m_pPixelShaderStartupPass2->Release();
    m_NonInteractiveRefresh.m_pPixelShaderStartupPass2 = nullptr;
  }

  if (m_NonInteractiveRefresh.m_pVertexDecl) {
    m_NonInteractiveRefresh.m_pVertexDecl->Release();
    m_NonInteractiveRefresh.m_pVertexDecl = nullptr;
  }
}

bool CShaderDeviceDx8::InNonInteractiveMode() const {
  return m_NonInteractiveRefresh.m_Mode != MATERIAL_NON_INTERACTIVE_MODE_NONE;
}

void CShaderDeviceDx8::EnableNonInteractiveMode(
    MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo) {
  if (!IsX360()) return;
  if (pInfo &&
      (pInfo->m_hTempFullscreenTexture == INVALID_SHADERAPI_TEXTURE_HANDLE)) {
    mode = MATERIAL_NON_INTERACTIVE_MODE_NONE;
  }
  m_NonInteractiveRefresh.m_Mode = mode;
  if (pInfo) {
    m_NonInteractiveRefresh.m_Info = *pInfo;
  }
  m_NonInteractiveRefresh.m_nPacifierFrame = 0;

  if (mode != MATERIAL_NON_INTERACTIVE_MODE_NONE) {
    ConVarRef mat_monitorgamma("mat_monitorgamma");
    ConVarRef mat_monitorgamma_tv_range_min("mat_monitorgamma_tv_range_min");
    ConVarRef mat_monitorgamma_tv_range_max("mat_monitorgamma_tv_range_max");
    ConVarRef mat_monitorgamma_tv_exp("mat_monitorgamma_tv_exp");
    ConVarRef mat_monitorgamma_tv_enabled("mat_monitorgamma_tv_enabled");
    SetHardwareGammaRamp(mat_monitorgamma.GetFloat(),
                         mat_monitorgamma_tv_range_min.GetFloat(),
                         mat_monitorgamma_tv_range_max.GetFloat(),
                         mat_monitorgamma_tv_exp.GetFloat(),
                         mat_monitorgamma_tv_enabled.GetBool());
  }

  m_NonInteractiveRefresh.m_flStartTime =
      m_NonInteractiveRefresh.m_flLastPresentTime =
          m_NonInteractiveRefresh.m_flLastPacifierTime = Plat_FloatTime();
  m_NonInteractiveRefresh.m_flPeakDt = 0.0f;
  m_NonInteractiveRefresh.m_flTotalDt = 0.0f;
  m_NonInteractiveRefresh.m_nSamples = 0;
  m_NonInteractiveRefresh.m_nCountAbove66 = 0;
}

void CShaderDeviceDx8::UpdatePresentStats() {
  float t = Plat_FloatTime();
  float flActualDt = t - m_NonInteractiveRefresh.m_flLastPresentTime;
  if (flActualDt > m_NonInteractiveRefresh.m_flPeakDt) {
    m_NonInteractiveRefresh.m_flPeakDt = flActualDt;
  }
  if (flActualDt > 0.066) {
    ++m_NonInteractiveRefresh.m_nCountAbove66;
  }

  m_NonInteractiveRefresh.m_flTotalDt += flActualDt;
  ++m_NonInteractiveRefresh.m_nSamples;

  t = Plat_FloatTime();
  m_NonInteractiveRefresh.m_flLastPresentTime = t;
}

void CShaderDeviceDx8::RefreshFrontBufferNonInteractive() {}

//-----------------------------------------------------------------------------
// Page flip
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::Present() {
  LOCK_SHADERAPI();

  // need to flush the dynamic buffer
  g_pShaderAPI->FlushBufferedPrimitives();

  if (!IsDeactivated()) {
    Dx9Device()->EndScene();
  }

  HRESULT hr = S_OK;

  // Copy the back buffer into the non-interactive temp buffer
  if (m_NonInteractiveRefresh.m_Mode ==
      MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD) {
    g_pShaderAPI->CopyRenderTargetToTextureEx(
        m_NonInteractiveRefresh.m_Info.m_hTempFullscreenTexture, 0, nullptr,
        nullptr);
  }

  // If we're not iconified, try to present (without this check, we can flicker
  // when Alt-Tabbed away)
  if (IsIconic((HWND)m_hWnd) == 0) {
    if ((m_IsResizing || (m_ViewHWnd != (HWND)m_hWnd))) {
      RECT destRect;
      GetClientRect((HWND)m_ViewHWnd, &destRect);

      ShaderViewport_t viewport;
      g_pShaderAPI->GetViewports(&viewport, 1);

      RECT srcRect;
      srcRect.left = viewport.m_nTopLeftX;
      srcRect.right = viewport.m_nTopLeftX + viewport.m_nWidth;
      srcRect.top = viewport.m_nTopLeftY;
      srcRect.bottom = viewport.m_nTopLeftY + viewport.m_nHeight;

      hr = Dx9Device()->Present(&srcRect, &destRect, (HWND)m_ViewHWnd, 0);
    } else {
      g_pShaderAPI->OwnGPUResources(false);
      hr = Dx9Device()->Present(0, 0, 0, 0);
    }
  }

  UpdatePresentStats();

  if (hr == D3DERR_DRIVERINTERNALERROR) {
    /*	Usually this bug means that the driver has run out of internal
       video memory, due to leaking it slowly over several application
       restarts. As of summer 2007, IE in particular seemed to leak a lot of
       driver memory for every image context it created in the browser window.
       A reboot clears out the leaked memory and will generally allow the game
            to be run again; occasionally (but not frequently) it's necessary
       to reduce video settings in the game as well to run. But, this is too
            fine a distinction to explain in a dialog, so place the guilt on
       the user and ask them to reduce video settings regardless.
    */
    Error(
        "Internal driver error at Present.\n"
        "You're likely out of OS Paged Pool Memory! For more info, see\n"
        "https://support.steampowered.com/cgi-bin/steampowered.cfg/php/"
        "enduser/std_adp.php?p_faqid=150\n");
  } else if (hr == D3DERR_DEVICELOST) {
    MarkDeviceLost();
  }

  MeshMgr()->DiscardVertexBuffers();

  CheckDeviceLost(m_bOtherAppInitializing);

#ifdef RECORD_KEYFRAMES
  static int frame = 0;
  ++frame;
  if (frame == KEYFRAME_INTERVAL) {
    RECORD_COMMAND(DX8_KEYFRAME, 0);

    g_pShaderAPI->ResetRenderState();
    frame = 0;
  }
#endif

  g_pShaderAPI->AdvancePIXFrame();

  if (!IsDeactivated()) {
    if (ShaderUtil()->GetConfig().bMeasureFillRate ||
        ShaderUtil()->GetConfig().bVisualizeFillRate) {
      g_pShaderAPI->ClearBuffers(true, true, true, -1, -1);
    }

    Dx9Device()->BeginScene();
  }
}

// We need to scale our colors to the range [16, 235] to keep our colors within
// TV standards.  Some colors might
//    still be out of gamut if any of the R, G, or B channels are more than 191
//    units apart from each other in the 0-255 scale, but it looks like the 360
//    deals with this for us by lowering the bright saturated color components.
// NOTE: I'm leaving the max at 255 to retain whiter than whites.  On most TV's,
// we seems a little dark in the bright colors
//    compared to TV and movies when played in the same conditions.  This keeps
//    out brights on par with what customers are used to seeing.
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer
// into a 2.5 frame buffer for display on a TV

void CShaderDeviceDx8::SetHardwareGammaRamp(float fGamma,
                                            float fGammaTVRangeMin,
                                            float fGammaTVRangeMax,
                                            float fGammaTVExponent,
                                            bool bTVEnabled) {
  DevMsg(2, "SetHardwareGammaRamp( %f )\n", fGamma);

  Assert(Dx9Device());
  if (!Dx9Device()) return;

  D3DGAMMARAMP gammaRamp;
  for (int i = 0; i < 256; i++) {
    float flInputValue = float(i) / 255.0f;

    // Since the 360's sRGB read/write is a piecewise linear approximation, we
    // need to correct for the difference in gamma space here
    float flSrgbGammaValue;
    if (IsX360())  // Should we also do this for the PS3?
    {
      // First undo the 360 broken sRGB curve by bringing the value back into
      // linear space
      float flLinearValue = X360GammaToLinear(flInputValue);
      flLinearValue = std::clamp(flLinearValue, 0.0f, 1.0f);

      // Now apply a true sRGB curve to mimic PC hardware
      flSrgbGammaValue =
          SrgbLinearToGamma(flLinearValue);  // ( flLinearValue <= 0.0031308f )
                                             // ? ( flLinearValue * 12.92f ) :
                                             // ( 1.055f * powf( flLinearValue,
                                             // ( 1.0f / 2.4f ) ) ) - 0.055f;
      flSrgbGammaValue = std::clamp(flSrgbGammaValue, 0.0f, 1.0f);
    } else {
      flSrgbGammaValue = flInputValue;
    }

    // Apply the user controlled exponent curve
    float flCorrection = pow(flSrgbGammaValue, (fGamma / 2.2f));
    flCorrection = std::clamp(flCorrection, 0.0f, 1.0f);

    // TV adjustment - Apply an exp and a scale and bias
    if (bTVEnabled) {
      // Adjust for TV gamma of 2.5 by applying an exponent of 2.2 / 2.5 = 0.88
      flCorrection = pow(flCorrection, 2.2f / fGammaTVExponent);
      flCorrection = std::clamp(flCorrection, 0.0f, 1.0f);

      // Scale and bias to fit into the 16-235 range for TV's
      flCorrection =
          (flCorrection * (fGammaTVRangeMax - fGammaTVRangeMin) / 255.0f) +
          (fGammaTVRangeMin / 255.0f);
      flCorrection = std::clamp(flCorrection, 0.0f, 1.0f);
    }

    // Generate final int value
    unsigned int val = (int)(flCorrection * 65535.0f);
    gammaRamp.red[i] = val;
    gammaRamp.green[i] = val;
    gammaRamp.blue[i] = val;
  }

  Dx9Device()->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &gammaRamp);
}

//-----------------------------------------------------------------------------
// Shader compilation
//-----------------------------------------------------------------------------
IShaderBuffer *CShaderDeviceDx8::CompileShader(const char *pProgram,
                                               size_t nBufLen,
                                               const char *pShaderVersion) {
  return ShaderManager()->CompileShader(pProgram, nBufLen, pShaderVersion);
}

VertexShaderHandle_t CShaderDeviceDx8::CreateVertexShader(
    IShaderBuffer *pBuffer) {
  return ShaderManager()->CreateVertexShader(pBuffer);
}

void CShaderDeviceDx8::DestroyVertexShader(VertexShaderHandle_t hShader) {
  ShaderManager()->DestroyVertexShader(hShader);
}

GeometryShaderHandle_t CShaderDeviceDx8::CreateGeometryShader(
    IShaderBuffer *pShaderBuffer) {
  Assert(0);
  return GEOMETRY_SHADER_HANDLE_INVALID;
}

void CShaderDeviceDx8::DestroyGeometryShader(GeometryShaderHandle_t hShader) {
  Assert(hShader == GEOMETRY_SHADER_HANDLE_INVALID);
}

PixelShaderHandle_t CShaderDeviceDx8::CreatePixelShader(
    IShaderBuffer *pBuffer) {
  return ShaderManager()->CreatePixelShader(pBuffer);
}

void CShaderDeviceDx8::DestroyPixelShader(PixelShaderHandle_t hShader) {
  ShaderManager()->DestroyPixelShader(hShader);
}

//-----------------------------------------------------------------------------
// Creates/destroys Mesh
// NOTE: Will be deprecated soon!
//-----------------------------------------------------------------------------
IMesh *CShaderDeviceDx8::CreateStaticMesh(VertexFormat_t vertexFormat,
                                          const char *pTextureBudgetGroup,
                                          IMaterial *pMaterial) {
  LOCK_SHADERAPI();
  return MeshMgr()->CreateStaticMesh(vertexFormat, pTextureBudgetGroup,
                                     pMaterial);
}

void CShaderDeviceDx8::DestroyStaticMesh(IMesh *pMesh) {
  LOCK_SHADERAPI();
  MeshMgr()->DestroyStaticMesh(pMesh);
}

//-----------------------------------------------------------------------------
// Creates/destroys vertex buffers + index buffers
//-----------------------------------------------------------------------------
IVertexBuffer *CShaderDeviceDx8::CreateVertexBuffer(ShaderBufferType_t type,
                                                    VertexFormat_t fmt,
                                                    int nVertexCount,
                                                    const char *pBudgetGroup) {
  LOCK_SHADERAPI();
  return MeshMgr()->CreateVertexBuffer(type, fmt, nVertexCount, pBudgetGroup);
}

void CShaderDeviceDx8::DestroyVertexBuffer(IVertexBuffer *pVertexBuffer) {
  LOCK_SHADERAPI();
  MeshMgr()->DestroyVertexBuffer(pVertexBuffer);
}

IIndexBuffer *CShaderDeviceDx8::CreateIndexBuffer(ShaderBufferType_t bufferType,
                                                  MaterialIndexFormat_t fmt,
                                                  int nIndexCount,
                                                  const char *pBudgetGroup) {
  LOCK_SHADERAPI();
  return MeshMgr()->CreateIndexBuffer(bufferType, fmt, nIndexCount,
                                      pBudgetGroup);
}

void CShaderDeviceDx8::DestroyIndexBuffer(IIndexBuffer *pIndexBuffer) {
  LOCK_SHADERAPI();
  MeshMgr()->DestroyIndexBuffer(pIndexBuffer);
}

IVertexBuffer *CShaderDeviceDx8::GetDynamicVertexBuffer(
    int streamID, VertexFormat_t vertexFormat, bool bBuffered) {
  LOCK_SHADERAPI();
  return MeshMgr()->GetDynamicVertexBuffer(streamID, vertexFormat, bBuffered);
}

IIndexBuffer *CShaderDeviceDx8::GetDynamicIndexBuffer(MaterialIndexFormat_t fmt,
                                                      bool bBuffered) {
  LOCK_SHADERAPI();
  return MeshMgr()->GetDynamicIndexBuffer(fmt, bBuffered);
}
