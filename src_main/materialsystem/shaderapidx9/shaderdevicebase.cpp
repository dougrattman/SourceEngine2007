// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <cinttypes>
#include "base/include/windows/windows_light.h"

#include "shaderdevicebase.h"

#include "datacache/idatacache.h"
#include "filesystem.h"
#include "shaderapi/ishadershadow.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi_global.h"
#include "shaderapibase.h"
#include "tier0/include/icommandline.h"
#include "tier1/convar.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"

IShaderUtil *g_pShaderUtil;  // The main shader utility interface
CShaderDeviceBase *g_pShaderDevice;
CShaderDeviceMgrBase *g_pShaderDeviceMgr;
CShaderAPIBase *g_pShaderAPI;
IShaderShadow *g_pShaderShadow;

bool g_bUseShaderMutex = false;  // Shader mutex globals
bool g_bShaderAccessDisallowed;
CShaderMutex g_ShaderMutex;

// FIXME: Hack related to setting command-line values for convars. Remove!!!
class CShaderAPIConVarAccessor : public IConCommandBaseAccessor {
 public:
  bool RegisterConCommandBase(ConCommandBase *pCommand) override {
    // Link to engine's list instead
    g_pCVar->RegisterConCommand(pCommand);

    char const *pValue = g_pCVar->GetCommandLineValue(pCommand->GetName());
    if (pValue && !pCommand->IsCommand()) {
      ((ConVar *)pCommand)->SetValue(pValue);
    }

    return true;
  }
};

static void InitShaderAPICVars() {
  static CShaderAPIConVarAccessor g_ConVarAccessor;
  if (g_pCVar) {
    ConVar_Register(0, &g_ConVarAccessor);
  }
}

// Read dx support levels
#define SUPPORT_CFG_FILE "dxsupport.cfg"
#define SUPPORT_CFG_OVERRIDE_FILE "dxsupport_override.cfg"

CShaderDeviceMgrBase::CShaderDeviceMgrBase() : dx_support_config_{nullptr} {}

CShaderDeviceMgrBase::~CShaderDeviceMgrBase() {}

// Factory used to get at internal interfaces (used by shaderapi + shader dlls)
static CreateInterfaceFn s_TempFactory;

void *ShaderDeviceFactory(const char *pName, int *pReturnCode) {
  if (pReturnCode) {
    *pReturnCode = IFACE_OK;
  }

  void *the_interface = s_TempFactory(pName, pReturnCode);
  if (the_interface) return the_interface;

  the_interface = Sys_GetFactoryThis()(pName, pReturnCode);
  if (the_interface) return the_interface;

  if (pReturnCode) {
    *pReturnCode = IFACE_FAILED;
  }

  return nullptr;
}

bool CShaderDeviceMgrBase::Connect(CreateInterfaceFn factory) {
  LOCK_SHADERAPI();

  Assert(!g_pShaderDeviceMgr);

  s_TempFactory = factory;

  // Connection/convar registration
  CreateInterfaceFn actualFactory = ShaderDeviceFactory;
  ConnectTier1Libraries(&actualFactory, 1);
  InitShaderAPICVars();
  ConnectTier2Libraries(&actualFactory, 1);
  g_pShaderUtil = (IShaderUtil *)ShaderDeviceFactory(
      SHADER_UTIL_INTERFACE_VERSION, nullptr);
  g_pShaderDeviceMgr = this;

  s_TempFactory = nullptr;

  if (!g_pShaderUtil || !g_pFullFileSystem || !g_pShaderDeviceMgr) {
    Warning("ShaderDeviceMgr was unable to access the required interfaces!\n");
    return false;
  }

  // NOTE: Overbright is 1.0 so that Hammer will work properly with the white
  // bumped and unbumped lightmaps.
  MathLib_Init(2.2f, 2.2f, 0.0f, 2.0f);

  return true;
}

void CShaderDeviceMgrBase::Disconnect() {
  LOCK_SHADERAPI();

  g_pShaderDeviceMgr = nullptr;
  g_pShaderUtil = nullptr;
  DisconnectTier2Libraries();
  ConVar_Unregister();
  DisconnectTier1Libraries();

  if (dx_support_config_) {
    dx_support_config_->deleteThis();
    dx_support_config_ = nullptr;
  }
}

// Query interface
void *CShaderDeviceMgrBase::QueryInterface(const char *pInterfaceName) {
  if (!Q_stricmp(pInterfaceName, SHADER_DEVICE_MGR_INTERFACE_VERSION))
    return (IShaderDeviceMgr *)this;

  if (!Q_stricmp(pInterfaceName,
                 MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION))
    return (IMaterialSystemHardwareConfig *)g_pHardwareConfig;

  return nullptr;
}

// Returns the hardware caps for a particular adapter
const HardwareCaps_t &CShaderDeviceMgrBase::GetHardwareCaps(
    int nAdapter) const {
  Assert((nAdapter >= 0) && (nAdapter < GetAdapterCount()));
  return adapters_[nAdapter].m_ActualCaps;
}

// Utility methods for reading config scripts
static inline int ReadHexValue(KeyValues *pVal, const char *pName) {
  const char *pString = pVal->GetString(pName, nullptr);
  if (!pString) {
    return -1;
  }

  char *pTemp;
  int nVal = strtol(pString, &pTemp, 16);
  return (pTemp != pString) ? nVal : -1;
}

static bool ReadBool(KeyValues *pGroup, const char *pKeyName, bool bDefault) {
  int nVal = pGroup->GetInt(pKeyName, -1);
  if (nVal != -1) {
    return (nVal != false);
  }
  return bDefault;
}

static void ReadInt(KeyValues *pGroup, const char *pKeyName, int nInvalidValue,
                    int *pResult) {
  int nVal = pGroup->GetInt(pKeyName, nInvalidValue);
  if (nVal != nInvalidValue) {
    *pResult = nVal;
  }
}

// Utility method to copy over a keyvalue
static void AddKey(KeyValues *pDest, KeyValues *pSrc) {
  // Note this will replace already-existing values
  switch (pSrc->GetDataType()) {
    case KeyValues::TYPE_NONE:
      break;
    case KeyValues::TYPE_STRING:
      pDest->SetString(pSrc->GetName(), pSrc->GetString());
      break;
    case KeyValues::TYPE_INT:
      pDest->SetInt(pSrc->GetName(), pSrc->GetInt());
      break;
    case KeyValues::TYPE_FLOAT:
      pDest->SetFloat(pSrc->GetName(), pSrc->GetFloat());
      break;
    case KeyValues::TYPE_PTR:
      pDest->SetPtr(pSrc->GetName(), pSrc->GetPtr());
      break;
    case KeyValues::TYPE_WSTRING:
      pDest->SetWString(pSrc->GetName(), pSrc->GetWString());
      break;
    case KeyValues::TYPE_COLOR:
      pDest->SetColor(pSrc->GetName(), pSrc->GetColor());
      break;
    default:
      Assert(0);
      break;
  }
}

// Finds if we have a dxlevel-specific config in the support keyvalues
KeyValues *CShaderDeviceMgrBase::FindDXLevelSpecificConfig(
    KeyValues *pKeyValues, int nDxLevel) {
  KeyValues *pGroup = pKeyValues->GetFirstSubKey();

  for (pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    int nFoundDxLevel = pGroup->GetInt("name", 0);
    if (nFoundDxLevel == nDxLevel) return pGroup;
  }

  return nullptr;
}

// Finds if we have a dxlevel and vendor-specific config in the support
// keyvalues
KeyValues *CShaderDeviceMgrBase::FindDXLevelAndVendorSpecificConfig(
    KeyValues *pKeyValues, int nDxLevel, int nVendorID) {
  KeyValues *pGroup = pKeyValues->GetFirstSubKey();

  for (pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    int nFoundDxLevel = pGroup->GetInt("name", 0);
    int nFoundVendorID = ReadHexValue(pGroup, "VendorID");
    if (nFoundDxLevel == nDxLevel && nFoundVendorID == nVendorID) return pGroup;
  }

  return nullptr;
}

// Finds if we have a vendor-specific config in the support keyvalues
KeyValues *CShaderDeviceMgrBase::FindCPUSpecificConfig(KeyValues *pKeyValues,
                                                       i32 nCPUMhz,
                                                       bool is_amd) {
  for (KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    const char *pName = pGroup->GetString("name", nullptr);
    if (!pName) continue;

    if ((is_amd && Q_stristr(pName, "AMD")) ||
        (!is_amd && Q_stristr(pName, "Intel"))) {
      int nMinMegahertz = pGroup->GetInt("min megahertz", -1);
      int nMaxMegahertz = pGroup->GetInt("max megahertz", -1);
      if (nMinMegahertz == -1 || nMaxMegahertz == -1) continue;

      if (nMinMegahertz <= nCPUMhz && nCPUMhz < nMaxMegahertz) return pGroup;
    }
  }
  return nullptr;
}

// Finds if we have a vendor-specific config in the support keyvalues
KeyValues *CShaderDeviceMgrBase::FindCardSpecificConfig(KeyValues *pKeyValues,
                                                        int nVendorId,
                                                        int nDeviceId) {
  KeyValues *pGroup = pKeyValues->GetFirstSubKey();
  for (pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    int nFoundVendorId = ReadHexValue(pGroup, "VendorID");
    int nFoundDeviceIdMin = ReadHexValue(pGroup, "MinDeviceID");
    int nFoundDeviceIdMax = ReadHexValue(pGroup, "MaxDeviceID");
    if (nFoundVendorId == nVendorId && nDeviceId >= nFoundDeviceIdMin &&
        nDeviceId <= nFoundDeviceIdMax)
      return pGroup;
  }

  return nullptr;
}

// Finds if we have a vendor-specific config in the support keyvalues
KeyValues *CShaderDeviceMgrBase::FindMemorySpecificConfig(KeyValues *pKeyValues,
                                                          u32 nSystemRamMB) {
  for (KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    const u64 nMinMB =
        pGroup->GetUint64("min megabytes", std::numeric_limits<u64>::max());
    const u64 nMaxMB =
        pGroup->GetUint64("max megabytes", std::numeric_limits<u64>::max());
    if (nMinMB == std::numeric_limits<u64>::max() ||
        nMaxMB == std::numeric_limits<u64>::max())
      continue;

    if (nMinMB <= nSystemRamMB && nSystemRamMB < nMaxMB) return pGroup;
  }

  return nullptr;
}

// Finds if we have a texture mem size specific config
KeyValues *CShaderDeviceMgrBase::FindVidMemSpecificConfig(KeyValues *pKeyValues,
                                                          u32 nVideoRamMB) {
  for (KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    const u64 nMinMB =
        pGroup->GetUint64("min megatexels", std::numeric_limits<u64>::max());
    const u64 nMaxMB =
        pGroup->GetUint64("max megatexels", std::numeric_limits<u64>::max());
    if (nMinMB == std::numeric_limits<u64>::max() ||
        nMaxMB == std::numeric_limits<u64>::max())
      continue;

    if (nMinMB <= nVideoRamMB && nVideoRamMB < nMaxMB) return pGroup;
  }
  return nullptr;
}

// Methods related to reading DX support levels given particular devices

// Reads in the dxsupport.cfg keyvalues
static void OverrideValues_R(KeyValues *pDest, KeyValues *pSrc) {
  // Any same-named values get overridden in pDest.
  for (KeyValues *pSrcValue = pSrc->GetFirstValue(); pSrcValue;
       pSrcValue = pSrcValue->GetNextValue()) {
    // Shouldn't be a container for more keys.
    Assert(pSrcValue->GetDataType() != KeyValues::TYPE_NONE);
    AddKey(pDest, pSrcValue);
  }

  // Recurse.
  for (KeyValues *pSrcDir = pSrc->GetFirstTrueSubKey(); pSrcDir;
       pSrcDir = pSrcDir->GetNextTrueSubKey()) {
    Assert(pSrcDir->GetDataType() == KeyValues::TYPE_NONE);

    KeyValues *pDestDir = pDest->FindKey(pSrcDir->GetName());
    if (pDestDir && pDestDir->GetDataType() == KeyValues::TYPE_NONE) {
      OverrideValues_R(pDestDir, pSrcDir);
    }
  }
}

static KeyValues *FindMatchingGroup(KeyValues *pSrc, KeyValues *pMatch) {
  KeyValues *pMatchSubKey = pMatch->FindKey("name");
  bool bHasSubKey =
      (pMatchSubKey && (pMatchSubKey->GetDataType() != KeyValues::TYPE_NONE));
  const char *name = bHasSubKey ? pMatchSubKey->GetString() : nullptr;
  int nMatchVendorID = ReadHexValue(pMatch, "VendorID");
  int nMatchMinDeviceID = ReadHexValue(pMatch, "MinDeviceID");
  int nMatchMaxDeviceID = ReadHexValue(pMatch, "MaxDeviceID");

  KeyValues *pSrcGroup = nullptr;
  for (pSrcGroup = pSrc->GetFirstTrueSubKey(); pSrcGroup;
       pSrcGroup = pSrcGroup->GetNextTrueSubKey()) {
    if (name) {
      KeyValues *pSrcGroupName = pSrcGroup->FindKey("name");
      Assert(pSrcGroupName);
      Assert(pSrcGroupName->GetDataType() != KeyValues::TYPE_NONE);
      if (Q_stricmp(pSrcGroupName->GetString(), name)) continue;
    }

    if (nMatchVendorID >= 0) {
      int nVendorID = ReadHexValue(pSrcGroup, "VendorID");
      if (nMatchVendorID != nVendorID) continue;
    }

    if (nMatchMinDeviceID >= 0 && nMatchMaxDeviceID >= 0) {
      int nMinDeviceID = ReadHexValue(pSrcGroup, "MinDeviceID");
      int nMaxDeviceID = ReadHexValue(pSrcGroup, "MaxDeviceID");
      if (nMinDeviceID < 0 || nMaxDeviceID < 0) continue;

      if (nMatchMinDeviceID > nMinDeviceID || nMatchMaxDeviceID < nMaxDeviceID)
        continue;
    }

    return pSrcGroup;
  }
  return nullptr;
}

static void OverrideKeyValues(KeyValues *pDst, KeyValues *pSrc) {
  KeyValues *pSrcGroup = nullptr;
  for (pSrcGroup = pSrc->GetFirstTrueSubKey(); pSrcGroup;
       pSrcGroup = pSrcGroup->GetNextTrueSubKey()) {
    // Match each group in pSrc to one in pDst containing the same "name" value:
    KeyValues *pDstGroup = FindMatchingGroup(pDst, pSrcGroup);
    Assert(pDstGroup);
    if (pDstGroup) {
      OverrideValues_R(pDstGroup, pSrcGroup);
    }
  }
}

KeyValues *CShaderDeviceMgrBase::ReadDXSupportKeyValues() {
  if (CommandLine()->CheckParm("-ignoredxsupportcfg")) return nullptr;
  if (dx_support_config_) return dx_support_config_;

  KeyValues *dx_support_config = new KeyValues("dxsupport");
  const char *path_id = "EXECUTABLE_PATH";

  // First try to read a game-specific config, if it exists
  if (!dx_support_config->LoadFromFile(g_pFullFileSystem, SUPPORT_CFG_FILE,
                                       path_id)) {
    dx_support_config->deleteThis();
    return nullptr;  //-V773
  }

  char temp_path[1024];
  if (g_pFullFileSystem->GetSearchPath("GAME", false, temp_path,
                                       ARRAYSIZE(temp_path)) > 1) {
    // Is there a mod-specific override file?
    KeyValues *dx_support_override_config = new KeyValues("dxsupport_override");
    if (dx_support_override_config->LoadFromFile(
            g_pFullFileSystem, SUPPORT_CFG_OVERRIDE_FILE, "GAME")) {
      OverrideKeyValues(dx_support_config, dx_support_override_config);
    }

    dx_support_override_config->deleteThis();
  }

  dx_support_config_ = dx_support_config;

  return dx_support_config;
}

// Returns the max dx support level achievable with this board
void CShaderDeviceMgrBase::ReadDXSupportLevels(HardwareCaps_t &caps) {
  // See if the file tells us otherwise
  KeyValues *dx_support_config = ReadDXSupportKeyValues();
  if (!dx_support_config) return;

  KeyValues *gpu_config = FindCardSpecificConfig(
      dx_support_config, caps.m_VendorID, caps.m_DeviceID);
  if (gpu_config) {
    // First, set the max dx level
    int max_dx_level = 0;
    ReadInt(gpu_config, "MaxDXLevel", 0, &max_dx_level);
    if (max_dx_level != 0) {
      caps.m_nMaxDXSupportLevel = max_dx_level;
    }

    // Next, set the preferred dx level
    int dx_support_level = 0;
    ReadInt(gpu_config, "DXLevel", 0, &dx_support_level);
    caps.m_nDXSupportLevel =
        dx_support_level != 0 ? dx_support_level : caps.m_nMaxDXSupportLevel;
  }
}

// Loads the hardware caps, for cases in which the D3D caps lie or where we need
// to augment the caps
void CShaderDeviceMgrBase::LoadHardwareCaps(KeyValues *pGroup,
                                            HardwareCaps_t &caps) {
  if (!pGroup) return;

  caps.m_UseFastClipping =
      ReadBool(pGroup, "NoUserClipPlanes", caps.m_UseFastClipping);
  caps.m_bNeedsATICentroidHack =
      ReadBool(pGroup, "CentroidHack", caps.m_bNeedsATICentroidHack);
  caps.m_bDisableShaderOptimizations = ReadBool(
      pGroup, "DisableShaderOptimizations", caps.m_bDisableShaderOptimizations);
}

// Reads in the hardware caps from the dxsupport.cfg file
void CShaderDeviceMgrBase::ReadHardwareCaps(HardwareCaps_t &caps,
                                            int nDxLevel) {
  KeyValues *dx_support_config = ReadDXSupportKeyValues();
  if (!dx_support_config) return;

  // Next, read the hardware caps for that dx support level.
  KeyValues *dx_levels_config =
      FindDXLevelSpecificConfig(dx_support_config, nDxLevel);
  // Look for a vendor specific line for a given dxlevel.
  KeyValues *dx_level_and_vendor_config = FindDXLevelAndVendorSpecificConfig(
      dx_support_config, nDxLevel, caps.m_VendorID);
  // Finally, override the hardware caps based on the specific card
  KeyValues *gpu_config = FindCardSpecificConfig(
      dx_support_config, caps.m_VendorID, caps.m_DeviceID);

  // Apply
  if (gpu_config && ReadHexValue(gpu_config, "MinDeviceID") == 0 &&
      ReadHexValue(gpu_config, "MaxDeviceID") == 0xFFFF) {
    // The card specific case is a catch all for device ids, so run it before
    // running the dxlevel and card specific stuff.
    LoadHardwareCaps(dx_levels_config, caps);
    LoadHardwareCaps(gpu_config, caps);
    LoadHardwareCaps(dx_level_and_vendor_config, caps);
  } else {
    // The card specific case is a small range of cards, so run it last to
    // override all other configs.
    LoadHardwareCaps(dx_levels_config, caps);
    // don't run this one since we have a specific config for this card.
    //		LoadHardwareCaps( pDXLevelAndVendorKeyValue, caps );
    LoadHardwareCaps(gpu_config, caps);
  }
}

// Reads in ConVars + config variables
void CShaderDeviceMgrBase::LoadConfig(KeyValues *pKeyValues,
                                      KeyValues *pConfiguration) {
  if (!pKeyValues) return;

  if (CommandLine()->FindParm("-debugdxsupport")) {
    CUtlBuffer tmpBuf;
    pKeyValues->RecursiveSaveToFile(tmpBuf, 0);
    Warning("%s\n", (const char *)tmpBuf.Base());
  }
  for (KeyValues *pGroup = pKeyValues->GetFirstSubKey(); pGroup;
       pGroup = pGroup->GetNextKey()) {
    AddKey(pConfiguration, pGroup);
  }
}

// Computes amount of ram.
static u64 GetRam() {
  MEMORYSTATUSEX memory_status{sizeof(MEMORYSTATUSEX)};
  if (GlobalMemoryStatusEx(&memory_status)) {
    return memory_status.ullTotalPhys / (1024 * 1024);
  }

  DevWarning("Can't get system RAM info. Assume %u MB.\n", 1024);
  return 1024;
}

// Gets the recommended configuration associated with a particular dx level
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo(
    int nAdapter, int nDXLevel, int nVendorID, int nDeviceID,
    KeyValues *common_config) {
  LOCK_SHADERAPI();

  const HardwareCaps_t &caps = GetHardwareCaps(nAdapter);
  if (nDXLevel == 0) {
    nDXLevel = caps.m_nDXSupportLevel;
  }
  nDXLevel = GetClosestActualDXLevel(nDXLevel);
  if (nDXLevel > caps.m_nMaxDXSupportLevel) return false;

  KeyValues *dx_config = ReadDXSupportKeyValues();
  if (!dx_config) return true;

  // Look for a dxlevel specific line
  KeyValues *dx_level_config = FindDXLevelSpecificConfig(dx_config, nDXLevel);
  // Look for a vendor specific line for a given dxlevel.
  KeyValues *dx_level_and_vendor_config =
      FindDXLevelAndVendorSpecificConfig(dx_config, nDXLevel, nVendorID);
  // Next, override with device-specific overrides
  KeyValues *gpu_config =
      FindCardSpecificConfig(dx_config, nVendorID, nDeviceID);

  // Apply
  if (gpu_config && ReadHexValue(gpu_config, "MinDeviceID") == 0 &&
      ReadHexValue(gpu_config, "MaxDeviceID") == 0xffff) {
    // The card specific case is a catch all for device ids, so run it before
    // running the dxlevel and card specific stuff.
    LoadConfig(dx_level_config, common_config);
    LoadConfig(gpu_config, common_config);
    LoadConfig(dx_level_and_vendor_config, common_config);
  } else {
    // The card specific case is a small range of cards, so run it last to
    // override all other configs.
    LoadConfig(dx_level_config, common_config);
    // don't run this one since we have a specific config for this card.
    //		LoadConfig( pDXLevelAndVendorKeyValue, pConfiguration );
    LoadConfig(gpu_config, common_config);
  }

  // Next, override with cpu-speed based overrides
  const CPUInformation &cpu_info = GetCPUInformation();
  const i64 cpu_speed_mhz = cpu_info.m_Speed / 1000000;
  const bool is_amd_cpu = Q_stristr(cpu_info.m_szProcessorID, "amd") != nullptr;

  DevMsg("CPU %s frequency %.2f GHz\n", cpu_info.m_szProcessorID,
         cpu_speed_mhz / 1000.f);

  KeyValues *cpu_config =
      FindCPUSpecificConfig(dx_config, cpu_speed_mhz, is_amd_cpu);
  LoadConfig(cpu_config, common_config);

  // Override with system memory-size based overrides
  const u64 memory_in_mb = GetRam();
  DevMsg("%" PRIu64 " MB of system RAM\n", memory_in_mb);

  KeyValues *memory_config = FindMemorySpecificConfig(dx_config, memory_in_mb);
  LoadConfig(memory_config, common_config);

  // Override with texture memory-size based overrides
  const u64 gpu_memory_in_mb = GetVidMemBytes(nAdapter) / (1024 * 1024);
  KeyValues *gpu_memory_config =
      FindVidMemSpecificConfig(dx_config, gpu_memory_in_mb);
  if (gpu_memory_config) {
    if (CommandLine()->FindParm("-debugdxsupport")) {
      CUtlBuffer tmpBuf;
      gpu_memory_config->RecursiveSaveToFile(tmpBuf, 0);
      Warning("gpu memory config\n%s\n", (const char *)tmpBuf.Base());
    }
    KeyValues *gpu_mat_picmip_config =
        gpu_memory_config->FindKey("ConVar.mat_picmip", false);

    // FIXME: Man, is this brutal. If it wasn't 1 day till orange box ship, I'd
    // do something in dxsupport maybe
    if (gpu_mat_picmip_config &&
        ((nDXLevel == caps.m_nMaxDXSupportLevel) || (gpu_memory_in_mb < 100))) {
      KeyValues *common_mat_picmip_config =
          common_config->FindKey("ConVar.mat_picmip", false);
      const int newPicMip = gpu_mat_picmip_config->GetInt();
      const int oldPicMip =
          common_mat_picmip_config ? common_mat_picmip_config->GetInt() : 0;
      common_config->SetInt("ConVar.mat_picmip",
                            std::max(newPicMip, oldPicMip));
    }
  }

  // Hack to slam the mat_dxlevel ConVar to match the requested dxlevel
  common_config->SetInt("ConVar.mat_dxlevel", nDXLevel);

  if (CommandLine()->FindParm("-debugdxsupport")) {
    CUtlBuffer tmpBuf;
    common_config->RecursiveSaveToFile(tmpBuf, 0);
    Warning("final config:\n%s\n", (const char *)tmpBuf.Base());
  }

  return true;
}

// Gets recommended congifuration for a particular adapter at a particular dx
// level
bool CShaderDeviceMgrBase::GetRecommendedConfigurationInfo(
    int nAdapter, int nDXLevel, KeyValues *pCongifuration) {
  Assert(nAdapter >= 0 && nAdapter <= GetAdapterCount());
  MaterialAdapterInfo_t info;
  GetAdapterInfo(nAdapter, info);
  return GetRecommendedConfigurationInfo(nAdapter, nDXLevel, info.m_VendorID,
                                         info.m_DeviceID, pCongifuration);
}

// Returns only valid dx levels
int CShaderDeviceMgrBase::GetClosestActualDXLevel(int nDxLevel) const {
  if (nDxLevel <= 69) return 60;
  if (nDxLevel <= 79) return 70;
  if (nDxLevel == 80) return 80;
  if (nDxLevel <= 89) return 81;
  if (nDxLevel <= 94) return 90;
  if (nDxLevel <= 99) return 95;
  if (nDxLevel <= 100) return 100;
  if (nDxLevel <= 110) return 110;
  return 120;
}

// Mode change callback
void CShaderDeviceMgrBase::AddModeChangeCallback(
    ShaderModeChangeCallbackFunc_t func) {
  LOCK_SHADERAPI();
  Assert(func && shader_mode_change_callbacks_.Find(func) < 0);
  shader_mode_change_callbacks_.AddToTail(func);
}

void CShaderDeviceMgrBase::RemoveModeChangeCallback(
    ShaderModeChangeCallbackFunc_t func) {
  LOCK_SHADERAPI();
  shader_mode_change_callbacks_.FindAndRemove(func);
}

void CShaderDeviceMgrBase::InvokeModeChangeCallbacks() {
  int nCount = shader_mode_change_callbacks_.Count();
  for (int i = 0; i < nCount; ++i) {
    shader_mode_change_callbacks_[i]();
  }
}

// Factory to return from SetMode
void *CShaderDeviceMgrBase::ShaderInterfaceFactory(const char *pInterfaceName,
                                                   int *pReturnCode) {
  if (pReturnCode) {
    *pReturnCode = IFACE_OK;
  }
  if (!Q_stricmp(pInterfaceName, SHADER_DEVICE_INTERFACE_VERSION))
    return static_cast<IShaderDevice *>(g_pShaderDevice);
  if (!Q_stricmp(pInterfaceName, SHADERAPI_INTERFACE_VERSION))
    return static_cast<IShaderAPI *>(g_pShaderAPI);
  if (!Q_stricmp(pInterfaceName, SHADERSHADOW_INTERFACE_VERSION))
    return static_cast<IShaderShadow *>(g_pShaderShadow);

  if (pReturnCode) {
    *pReturnCode = IFACE_FAILED;
  }
  return nullptr;
}

//
// The Base implementation of the shader device
//

CShaderDeviceBase::CShaderDeviceBase() {
  m_bInitialized = false;
  m_nAdapter = -1;
  m_hWnd = nullptr;
  m_hWndCookie = nullptr;
}

CShaderDeviceBase::~CShaderDeviceBase() {}

// Methods of IShaderDevice
ImageFormat CShaderDeviceBase::GetBackBufferFormat() const {
  return IMAGE_FORMAT_UNKNOWN;
}

int CShaderDeviceBase::StencilBufferBits() const { return 0; }

bool CShaderDeviceBase::IsAAEnabled() const { return false; }

// Methods for interprocess communication to release resources

#define MATERIAL_SYSTEM_WINDOW_ID 0xFEEDDEAD

static HWND GetTopmostParentWindow(HWND hWnd) {
  // Find the parent window...
  HWND hParent = GetParent(hWnd);
  while (hParent) {
    hWnd = hParent;
    hParent = GetParent(hWnd);
  }

  return hWnd;
}

static BOOL CALLBACK EnumChildWindowsProc(HWND hWnd, LPARAM lParam) {
  int windowId = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
  if (windowId == MATERIAL_SYSTEM_WINDOW_ID) {
    COPYDATASTRUCT copyData;
    copyData.dwData = lParam;
    copyData.cbData = 0;
    copyData.lpData = 0;

    SendMessageW(hWnd, WM_COPYDATA, 0, (LPARAM)&copyData);
  }
  return TRUE;
}

static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
  EnumChildWindows(hWnd, EnumChildWindowsProc, lParam);
  return TRUE;
}

static BOOL CALLBACK EnumWindowsProcNotThis(HWND hWnd, LPARAM lParam) {
  if (g_pShaderDevice &&
      (GetTopmostParentWindow((HWND)g_pShaderDevice->GetIPCHWnd()) == hWnd))
    return TRUE;

  EnumChildWindows(hWnd, EnumChildWindowsProc, lParam);
  return TRUE;
}

// Adds a hook to let us know when other instances are setting the mode
static LRESULT CALLBACK ShaderApiDx9WndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                            LPARAM lParam) {
  // FIXME: Should these IPC messages tell when an app has focus or not?
  // If so, we'd want to totally disable the shader api layer when an app
  // doesn't have focus.

  // Look for the special IPC message that tells us we're trying to set
  // the mode....
  switch (msg) {
    case WM_COPYDATA: {
      if (!g_pShaderDevice) break;

      COPYDATASTRUCT *pData = (COPYDATASTRUCT *)lParam;

      // that number is our magic cookie number
      if (pData->dwData == CShaderDeviceBase::RELEASE_MESSAGE) {
        g_pShaderDevice->OtherAppInitializing(true);
      } else if (pData->dwData == CShaderDeviceBase::REACQUIRE_MESSAGE) {
        g_pShaderDevice->OtherAppInitializing(false);
      } else if (pData->dwData == CShaderDeviceBase::EVICT_MESSAGE) {
        g_pShaderDevice->EvictManagedResourcesInternal();
      }
    } break;
  }

  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Install, remove ability to talk to other shaderapi apps
void CShaderDeviceBase::InstallWindowHook(void *hWnd) {
  Assert(m_hWndCookie == nullptr);

  HWND parent_window = GetTopmostParentWindow((HWND)hWnd);

  // Attach a child window to the parent; we're gonna store special info there
  // We can't use the USERDATA, cause other apps may want to use this.
  HINSTANCE instance =
      (HINSTANCE)GetWindowLongPtrW(parent_window, GWLP_HINSTANCE);
  WNDCLASSW wc{0};
  wc.style = CS_NOCLOSE | CS_PARENTDC;
  wc.lpfnWndProc = ShaderApiDx9WndProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"Valve_ShaderApiDx9_WndClass";

  // In case an old one is sitting around still...
  UnregisterClassW(L"Valve_ShaderApiDx9_WndClass", instance);

  RegisterClassW(&wc);

  // Create the window
  m_hWndCookie = CreateWindowW(L"Valve_ShaderApiDx9_WndClass",
                               L"Valve ShaderApiDx9", WS_CHILD, 0, 0, 0, 0,
                               parent_window, nullptr, instance, nullptr);

  // Marks it as a material system window
  SetWindowLongPtrW((HWND)m_hWndCookie, GWLP_USERDATA,
                    MATERIAL_SYSTEM_WINDOW_ID);
}

void CShaderDeviceBase::RemoveWindowHook(void *hWnd) {
  if (m_hWndCookie) {
    DestroyWindow((HWND)m_hWndCookie);
    m_hWndCookie = 0;
  }

  HWND parent_window = GetTopmostParentWindow((HWND)hWnd);
  HINSTANCE instance =
      (HINSTANCE)GetWindowLongPtrW(parent_window, GWLP_HINSTANCE);

  UnregisterClassW(L"Valve_ShaderApiDx9_WndClass", instance);
}

// Sends a message to other shaderapi applications
void CShaderDeviceBase::SendIPCMessage(IPCMessage_t msg) {
  // Gotta send this to all windows, since we don't know which ones
  // are material system apps...
  if (msg != EVICT_MESSAGE) {
    EnumWindows(EnumWindowsProc, implicit_cast<LPARAM>(msg));
  } else {
    EnumWindows(EnumWindowsProcNotThis, implicit_cast<LPARAM>(msg));
  }
}

// Find view
int CShaderDeviceBase::FindView(void *hWnd) const {
  /* FIXME: Is this necessary?
  // Look for the view in the list of views
  for (int i = m_Views.Count(); --i >= 0; )
  {
  if (m_Views[i].m_HWnd == (HWND)hwnd)
  return i;
  }
  */
  return -1;
}

// Creates a child window
bool CShaderDeviceBase::AddView(void *hWnd) {
  LOCK_SHADERAPI();
  /*
  // If we haven't created a device yet
  if (!Dx9Device())
          return false;

  // Make sure no duplicate hwnds...
  if (FindView(hwnd) >= 0)
          return false;

  // In this case, we need to create the device; this is our
  // default swap chain. This here says we're gonna use a part of the
  // existing buffer and just grab that.
  int view = m_Views.AddToTail();
  m_Views[view].m_HWnd = (HWND)hwnd;
  //	memcpy( &m_Views[view].m_PresentParamters, m_PresentParameters,
  sizeof(m_PresentParamters) );

  HRESULT hr;
  hr = Dx9Device()->CreateAdditionalSwapChain( &m_PresentParameters,
  &m_Views[view].m_pSwapChain );
  return !FAILED(hr);
  */

  return true;
}

void CShaderDeviceBase::RemoveView(void *hWnd) {
  LOCK_SHADERAPI();
  /*
  // Look for the view in the list of views
  int i = FindView(hwnd);
  if (i >= 0)
  {
  // FIXME		m_Views[i].m_pSwapChain->Release();
  m_Views.FastRemove(i);
  }
  */
}

// Activates a child window
void CShaderDeviceBase::SetView(void *hWnd) {
  LOCK_SHADERAPI();

  ShaderViewport_t viewport;
  g_pShaderAPI->GetViewports(&viewport, 1);

  // Get the window (*not* client) rect of the view window
  m_ViewHWnd = (HWND)hWnd;
  GetWindowSize(m_nWindowWidth, m_nWindowHeight);

  // Reset the viewport (takes into account the view rect)
  // Don't need to set the viewport if it's not ready
  g_pShaderAPI->SetViewports(1, &viewport);
}

// Gets the window size
void CShaderDeviceBase::GetWindowSize(int &nWidth, int &nHeight) const {
  // If the window was minimized last time swap buffers happened, or if it's
  // iconic now, return 0 size
  if (!m_bIsMinimized && !IsIconic((HWND)m_hWnd)) {
    // NOTE: Use the 'current view' (which may be the same as the main window)
    RECT rect;
    GetClientRect((HWND)m_ViewHWnd, &rect);
    nWidth = rect.right - rect.left;
    nHeight = rect.bottom - rect.top;
  } else {
    nWidth = nHeight = 0;
  }
}
