// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Implementation of a material

#include <ctype.h>
#include <malloc.h>
#include <cstring>
#include "IHardwareConfigInternal.h"
#include "bitmap/tgaloader.h"
#include "cmaterial_queuefriendly.h"
#include "colorspace.h"
#include "filesystem.h"
#include "ifilelist.h"
#include "imaterialinternal.h"
#include "itextureinternal.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem_global.h"
#include "mathlib/vmatrix.h"
#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishaderutil.h"
#include "shadersystem.h"
#include "texturemanager.h"
#include "tier1/callqueue.h"
#include "tier1/keyvalues.h"
#include "tier1/mempool.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"
#include "vtf/vtf.h"

// Material implementation
class CMaterial : public IMaterialInternal {
 public:
  // Members of the IMaterial interface
  const char *GetName() const;
  const char *GetTextureGroupName() const;

  PreviewImageRetVal_t GetPreviewImageProperties(int *width, int *height,
                                                 ImageFormat *imageFormat,
                                                 bool *isTranslucent) const;
  PreviewImageRetVal_t GetPreviewImage(unsigned char *data, int width,
                                       int height,
                                       ImageFormat imageFormat) const;

  int GetMappingWidth();
  int GetMappingHeight();
  int GetNumAnimationFrames();

  bool InMaterialPage() { return false; }
  void GetMaterialOffset(float *pOffset);
  void GetMaterialScale(float *pOffset);
  IMaterial *GetMaterialPage() { return nullptr; }

  void IncrementReferenceCount();
  void DecrementReferenceCount();
  int GetEnumerationID() const;
  void GetLowResColorSample(float s, float t, float *color) const;

  IMaterialVar *FindVar(char const *varName, bool *found, bool complain = true);
  IMaterialVar *FindVarFast(char const *pVarName, unsigned int *pToken);

  // Sets new VMT shader parameters for the material
  virtual void SetShaderAndParams(KeyValues *pKeyValues);

  bool UsesEnvCubemap();
  bool NeedsSoftwareSkinning();
  virtual bool NeedsSoftwareLighting();
  bool NeedsTangentSpace();
  bool NeedsPowerOfTwoFrameBufferTexture(bool bCheckSpecificToThisFrame = true);
  bool NeedsFullFrameBufferTexture(bool bCheckSpecificToThisFrame = true);
  virtual bool IsUsingVertexID() const;

  // GR - Is lightmap alpha needed?
  bool NeedsLightmapBlendAlpha();

  virtual void AlphaModulate(float alpha);
  virtual void ColorModulate(float r, float g, float b);
  virtual float GetAlphaModulation();
  virtual void GetColorModulation(float *r, float *g, float *b);

  // Gets the morph format
  virtual MorphFormat_t GetMorphFormat() const;

  void SetMaterialVarFlag(MaterialVarFlags_t flag, bool on);
  bool GetMaterialVarFlag(MaterialVarFlags_t flag) const;

  bool IsTranslucent();
  // need to centralize the logic without
  // relying on the *current* alpha
  // modulation being that which is stored in
  // m_pShaderParams[ALPHA].
  bool IsTranslucentInternal(float fAlphaModulation) const;
  bool IsAlphaTested();
  bool IsVertexLit();
  virtual bool IsSpriteCard();

  void GetReflectivity(Vector &reflect);
  bool GetPropertyFlag(MaterialPropertyTypes_t type);

  // Is the material visible from both sides?
  bool IsTwoSided();

  int GetNumPasses();
  int GetTextureMemoryBytes();

  void SetUseFixedFunctionBakedLighting(bool bEnable);

 public:
  // stuff that is visible only from within the material system

  // constructor, destructor
  CMaterial(char const *materialName, const char *pTextureGroupName,
            KeyValues *pVMTKeyValues);
  virtual ~CMaterial();

  void DrawMesh(VertexCompressionType_t vertexCompression);
  int GetReferenceCount() const;
  void Uncache(bool bPreserveVars = false);
  void Precache();
  void ReloadTextures();
  // If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
  bool PrecacheVars(KeyValues *pKeyValues = nullptr,
                    KeyValues *pPatchKeyValues = nullptr,
                    CUtlVector<FileNameHandle_t> *pIncludes = nullptr);
  void SetMinLightmapPageID(int pageID);
  void SetMaxLightmapPageID(int pageID);
  int GetMinLightmapPageID() const;
  int GetMaxLightmapPageID() const;
  void SetNeedsWhiteLightmap(bool val);
  bool GetNeedsWhiteLightmap() const;
  bool IsPrecached() const;
  bool IsPrecachedVars() const;
  IShader *GetShader() const;
  const char *GetShaderName() const;

  virtual void DeleteIfUnreferenced();

  void SetEnumerationID(int id);
  void CallBindProxy(void *proxyData);
  bool HasProxy(void) const;

  // Sets the shader associated with the material
  void SetShader(const char *pShaderName);

  // Can we override this material in debug?
  bool NoDebugOverride() const;

  // Gets the vertex format
  VertexFormat_t GetVertexFormat() const;

  // diffuse bump lightmap?
  bool IsUsingDiffuseBumpedLighting() const;

  // lightmap?
  bool IsUsingLightmap() const;

  // Gets the vertex usage flags
  VertexFormat_t GetVertexUsage() const;

  // Debugs this material
  bool PerformDebugTrace() const;

  // Are we suppressed?
  bool IsSuppressed() const;

  // Do we use fog?
  bool UseFog(void) const;

  // Should we draw?
  void ToggleSuppression();
  void ToggleDebugTrace();

  // Refresh material based on current var values
  void Refresh();
  void RefreshPreservingMaterialVars();

  // This computes the state snapshots for this material
  void RecomputeStateSnapshots();

  // Gets at the shader parameters
  virtual int ShaderParamCount() const;
  virtual IMaterialVar **GetShaderParams();

  virtual void AddMaterialVar(IMaterialVar *pMaterialVar);

  virtual bool IsErrorMaterial() const;

  // Was this manually created (not read from a file?)
  virtual bool IsManuallyCreated() const;

  virtual bool NeedsFixedFunctionFlashlight() const;

  virtual void MarkAsPreloaded(bool bSet);
  virtual bool IsPreloaded() const;

  virtual void ArtificialAddRef();
  virtual void ArtificialRelease();

  virtual void ReportVarChanged(IMaterialVar *pVar) { m_ChangeID++; }
  virtual void ClearContextData();

  virtual uint32_t GetChangeID() const { return m_ChangeID; }

  virtual bool IsRealTimeVersion(void) const { return true; }
  virtual IMaterialInternal *GetRealTimeVersion() { return this; }
  virtual IMaterialInternal *GetQueueFriendlyVersion() {
    return &m_QueueFriendlyVersion;
  }

  void DecideShouldReloadFromWhitelist(IFileList *pFilesToReload);
  void ReloadFromWhitelistIfMarked();
  bool WasReloadedFromWhitelist();

 private:
  // Initializes, cleans up the shader params
  void CleanUpShaderParams();

  // Sets up an error shader when we run into problems.
  void SetupErrorShader();

  // Does this material have a UNC-file name?
  bool UsesUNCFileName() const;

  // Prints material flags.
  void PrintMaterialFlags(int flags, int flagsDefined);

  // Parses material flags
  bool ParseMaterialFlag(KeyValues *pParseValue, IMaterialVar *pFlagVar,
                         IMaterialVar *pFlagDefinedVar, bool parsingOverrides,
                         int &flagMask, int &overrideMask);

  // Computes the material vars for the shader
  template <usize vars_size>
  int ParseMaterialVars(IShader *pShader, KeyValues &keyValues,
                        KeyValues *pOverride, bool modelDefault,
                        IMaterialVar *(&ppVars)[vars_size]);

  // Figures out the preview image for worldcraft
  char const *GetPreviewImageName();
  char const *GetPreviewImageFileName(void) const;

  // Hooks up the shader, returns keyvalues of fallback that was used
  KeyValues *InitializeShader(KeyValues &keyValues, KeyValues &patchKeyValues);

  // Finds the flag associated with a particular flag name
  int FindMaterialVarFlag(char const *pFlagName) const;

  // Initializes, cleans up the state snapshots
  bool InitializeStateSnapshots();
  void CleanUpStateSnapshots();

  // Initializes, cleans up the material proxy
  void InitializeMaterialProxy(KeyValues *pFallbackKeyValues);
  void CleanUpMaterialProxy();

  // Creates, destroys snapshots
  RenderPassList_t *CreateRenderPassList();
  void DestroyRenderPassList(RenderPassList_t *pPassList);

  // Grabs the texture width and height from the var list for faster access
  void PrecacheMappingDimensions();

  // Gets the renderstate
  virtual ShaderRenderState_t *GetRenderState();

  // Do we have a valid renderstate?
  bool IsValidRenderState() const;

  // Get the material var flags
  int GetMaterialVarFlags() const;
  void SetMaterialVarFlags(int flags, bool on);
  int GetMaterialVarFlags2() const;
  void SetMaterialVarFlags2(int flags, bool on);

  // Returns a dummy material variable
  IMaterialVar *GetDummyVariable();

  IMaterialVar *GetShaderParam(int id);

  void FindRepresentativeTexture();

  bool ShouldSkipVar(KeyValues *pMaterialVar, bool *pWasConditional);

  // Fixed-size allocator
  DECLARE_FIXEDSIZE_ALLOCATOR(CMaterial);

 private:
  enum {
    MATERIAL_NEEDS_WHITE_LIGHTMAP = 0x1,
    MATERIAL_IS_PRECACHED = 0x2,
    MATERIAL_VARS_IS_PRECACHED = 0x4,
    MATERIAL_VALID_RENDERSTATE = 0x8,
    MATERIAL_IS_MANUALLY_CREATED = 0x10,
    MATERIAL_USES_UNC_FILENAME = 0x20,
    MATERIAL_IS_PRELOADED = 0x40,
    MATERIAL_ARTIFICIAL_REFCOUNT = 0x80,
  };

  int m_iEnumerationID;

  int m_minLightmapPageID;
  int m_maxLightmapPageID;

  unsigned short m_MappingWidth;
  unsigned short m_MappingHeight;

  IShader *m_pShader;

  CUtlSymbol m_Name;
  // Any textures created for this material go under this texture group.
  CUtlSymbol m_TextureGroupName;

  CInterlockedInt m_RefCount;
  unsigned short m_Flags;

  unsigned char m_VarCount;
  unsigned char m_ProxyCount;

  IMaterialVar **m_pShaderParams;
  IMaterialProxy **m_ppProxies;
  ShaderRenderState_t m_ShaderRenderState;

  // This remembers filenames of VMTs that we included so we can sv_pure/flush
  // ourselves if any of them need to be reloaded.
  CUtlVector<FileNameHandle_t> m_VMTIncludes;
  bool m_bShouldReloadFromWhitelist;  // Tells us if the material decided it
                                      // should be reloaded due to sv_pure
                                      // whitelist changes.

  ITextureInternal *m_representativeTexture;
  Vector m_Reflectivity;
  uint32_t m_ChangeID;

  // Used only by procedural materials; it essentially is an in-memory .VMT file
  KeyValues *m_pVMTKeyValues;

#ifndef NDEBUG
  // Makes it easier to see what's going on
  char *m_pDebugName;
#endif

 protected:
  CMaterial_QueueFriendly m_QueueFriendlyVersion;
};

// NOTE: This must be the last file included
// Has to exist *after* fixed size allocator declaration
#include "tier0/include/memdbgon.h"

// Parser utilities
static inline bool IsWhitespace(char c) { return c == ' ' || c == '\t'; }

static inline bool IsEndline(char c) { return c == '\n' || c == '\0'; }

static inline bool IsVector(char const *v) {
  while (IsWhitespace(*v)) {
    ++v;
    if (IsEndline(*v)) return false;
  }
  return *v == '[' || *v == '{';
}

// Methods to create state snapshots
#include "tier0/include/memdbgoff.h"

struct EditorRenderStateList_t {
  // Store combo of alpha, color, fixed-function baked lighting, flashlight,
  // editor mode
  RenderPassList_t m_Snapshots[SNAPSHOT_COUNT_EDITOR];

  DECLARE_FIXEDSIZE_ALLOCATOR(EditorRenderStateList_t);
};

struct StandardRenderStateList_t {
  // Store combo of alpha, color, fixed-function baked lighting, flashlight
  RenderPassList_t m_Snapshots[SNAPSHOT_COUNT_NORMAL];

  DECLARE_FIXEDSIZE_ALLOCATOR(StandardRenderStateList_t);
};

#include "tier0/include/memdbgon.h"

DEFINE_FIXEDSIZE_ALLOCATOR(EditorRenderStateList_t, 256, true);
DEFINE_FIXEDSIZE_ALLOCATOR(StandardRenderStateList_t, 256, true);

// class factory methods
DEFINE_FIXEDSIZE_ALLOCATOR(CMaterial, 256, true);

IMaterialInternal *IMaterialInternal::CreateMaterial(
    char const *material_name, const char *pTextureGroupName,
    KeyValues *pVMTKeyValues) {
  MaterialLock_t lock = MaterialSystem()->Lock();
  auto *material =
      new CMaterial(material_name, pTextureGroupName, pVMTKeyValues);
  MaterialSystem()->Unlock(lock);
  return material;
}

void IMaterialInternal::DestroyMaterial(IMaterialInternal *material) {
  MaterialLock_t lock = MaterialSystem()->Lock();
  if (material) {
    Assert(material->IsRealTimeVersion());
    CMaterial *impl = static_cast<CMaterial *>(material);
    delete impl;
  }
  MaterialSystem()->Unlock(lock);
}

CMaterial::CMaterial(char const *material_name, const char *pTextureGroupName,
                     KeyValues *pKeyValues)
    : m_TextureGroupName{pTextureGroupName} {
  m_Reflectivity.Init(0.2f, 0.2f, 0.2f);
  usize len = strlen(material_name);
  char *pTemp = (char *)_alloca(len + 1);

  // Strip off the extension
  Q_StripExtension(material_name, pTemp, len + 1);
  Q_strlower(pTemp, len + 1);

  // Convert it to a symbol
  m_Name = pTemp;

#ifndef NDEBUG
  m_pDebugName = new char[strlen(pTemp) + 1];
  strcpy_s(m_pDebugName, strlen(pTemp) + 1, pTemp);
#endif

  m_bShouldReloadFromWhitelist = false;
  m_Flags = 0;
  m_pShader = nullptr;
  m_pShaderParams = nullptr;
  m_RefCount = 0;
  m_representativeTexture = nullptr;
  m_ppProxies = nullptr;
  m_ProxyCount = 0;
  m_VarCount = 0;
  m_MappingWidth = m_MappingHeight = 0;
  m_iEnumerationID = 0;
  m_minLightmapPageID = m_maxLightmapPageID = 0;
  m_pVMTKeyValues = pKeyValues;
  if (m_pVMTKeyValues) {
    m_Flags |= MATERIAL_IS_MANUALLY_CREATED;
  }

  if (pTemp[0] == '/' && pTemp[1] == '/' && pTemp[2] != '/') {
    m_Flags |= MATERIAL_USES_UNC_FILENAME;
  }

  // Initialize the renderstate to something indicating nothing should be drawn
  m_ShaderRenderState.m_Flags = 0;
  m_ShaderRenderState.m_VertexFormat = m_ShaderRenderState.m_VertexUsage = 0;
  m_ShaderRenderState.m_MorphFormat = 0;
  m_ShaderRenderState.m_pSnapshots = CreateRenderPassList();
  m_ChangeID = 0;

  m_QueueFriendlyVersion.SetRealTimeVersion(this);
}

CMaterial::~CMaterial() {
  MaterialSystem()->UnbindMaterial(this);

  Uncache();

  if (m_RefCount != 0) {
    Warning("Reference Count for Material %s (%d) != 0\n", GetName(),
            (int)m_RefCount);
  }

  if (m_pVMTKeyValues) {
    m_pVMTKeyValues->deleteThis();
    m_pVMTKeyValues = nullptr;
  }

  DestroyRenderPassList(m_ShaderRenderState.m_pSnapshots);

  m_representativeTexture = nullptr;

#ifndef NDEBUG
  delete[] m_pDebugName;
#endif
}

void CMaterial::ClearContextData() {
  int nSnapshotCount = SnapshotTypeCount();

  for (int i = 0; i < nSnapshotCount; i++) {
    for (int j = 0; j < m_ShaderRenderState.m_pSnapshots[i].m_nPassCount; j++) {
      if (m_ShaderRenderState.m_pSnapshots[i].m_pContextData[j]) {
        delete m_ShaderRenderState.m_pSnapshots[i].m_pContextData[j];
        m_ShaderRenderState.m_pSnapshots[i].m_pContextData[j] = nullptr;
      }
    }
  }
}

// Sets new VMT shader parameters for the material
void CMaterial::SetShaderAndParams(KeyValues *pKeyValues) {
  Uncache();

  if (m_pVMTKeyValues) {
    m_pVMTKeyValues->deleteThis();
    m_pVMTKeyValues = nullptr;
  }

  m_pVMTKeyValues = pKeyValues ? pKeyValues->MakeCopy() : nullptr;
  if (m_pVMTKeyValues) {
    m_Flags |= MATERIAL_IS_MANUALLY_CREATED;
  }

  if (g_pShaderDevice->IsUsingGraphics()) {
    Precache();
  }
}

// Creates, destroys snapshots
RenderPassList_t *CMaterial::CreateRenderPassList() {
  RenderPassList_t *pRenderPassList;
  if (!MaterialSystem()->CanUseEditorMaterials()) {
    StandardRenderStateList_t *pList = new StandardRenderStateList_t;
    pRenderPassList = (RenderPassList_t *)pList->m_Snapshots;
  } else {
    EditorRenderStateList_t *pList = new EditorRenderStateList_t;
    pRenderPassList = (RenderPassList_t *)pList->m_Snapshots;
  }

  int nSnapshotCount = SnapshotTypeCount();
  memset(pRenderPassList, 0, nSnapshotCount * sizeof(RenderPassList_t));
  return pRenderPassList;
}

void CMaterial::DestroyRenderPassList(RenderPassList_t *pPassList) {
  if (!pPassList) return;

  int nSnapshotCount = SnapshotTypeCount();
  for (int i = 0; i < nSnapshotCount; i++)
    for (int j = 0; j < pPassList[i].m_nPassCount; j++) {
      if (pPassList[i].m_pContextData[j]) {
        delete pPassList[i].m_pContextData[j];
        pPassList[i].m_pContextData[j] = nullptr;
      }
    }
  if (IsConsole() || !MaterialSystem()->CanUseEditorMaterials()) {
    StandardRenderStateList_t *pList = (StandardRenderStateList_t *)pPassList;
    delete pList;
  } else {
    EditorRenderStateList_t *pList = (EditorRenderStateList_t *)pPassList;
    delete pList;
  }
}

// Gets the renderstate
ShaderRenderState_t *CMaterial::GetRenderState() {
  Precache();
  return &m_ShaderRenderState;
}

// Returns a dummy material variable
IMaterialVar *CMaterial::GetDummyVariable() {
  static IMaterialVar *pDummyVar = 0;
  if (!pDummyVar) pDummyVar = IMaterialVar::Create(0, "$dummyVar", 0);

  return pDummyVar;
}

// Are vars precached?
bool CMaterial::IsPrecachedVars() const {
  return (m_Flags & MATERIAL_VARS_IS_PRECACHED) != 0;
}

// Are we precached?
bool CMaterial::IsPrecached() const {
  return (m_Flags & MATERIAL_IS_PRECACHED) != 0;
}

// Cleans up shader parameters
void CMaterial::CleanUpShaderParams() {
  if (m_pShaderParams) {
    for (u8 i = 0; i < m_VarCount; ++i) {
      IMaterialVar::Destroy(m_pShaderParams[i]);
    }

    free(m_pShaderParams);
    m_pShaderParams = 0;
  }
  m_VarCount = 0;
}

// Initializes the material proxy
void CMaterial::InitializeMaterialProxy(KeyValues *pFallbackKeyValues) {
  IMaterialProxyFactory *proxy_factory =
      MaterialSystem()->GetMaterialProxyFactory();
  if (!proxy_factory) return;

  // See if we've got a proxy section; obey fallbacks
  KeyValues *proxies_values = pFallbackKeyValues->FindKey("Proxies");
  if (!proxies_values) return;

  // Iterate through the section + create all of the proxies
  u8 proxies_count{0};
  IMaterialProxy *proxies[256];

  KeyValues *proxy_values = proxies_values->GetFirstSubKey();
  for (; proxy_values; proxy_values = proxy_values->GetNextKey()) {
    // Each of the proxies should themselves be databases
    IMaterialProxy *proxy = proxy_factory->CreateProxy(proxy_values->GetName());
    if (!proxy) {
      Warning("Error: Material \"%s\" : proxy \"%s\" not found!\n", GetName(),
              proxy_values->GetName());
      continue;
    }

    if (!proxy->Init(GetQueueFriendlyVersion(), proxy_values)) {
      proxy_factory->DeleteProxy(proxy);
      Warning("Error: Material \"%s\" : proxy \"%s\" unable to initialize!\n",
              GetName(), proxy_values->GetName());
    } else {
      proxies[proxies_count] = proxy;
      ++proxies_count;

      if (proxies_count >= std::size(proxies)) {
        Warning("Error: Material \"%s\" has more than %d proxies!\n", GetName(),
                std::size(proxies));
        break;
      }
    }
  }

  // Allocate space for the number of proxies we successfully made...
  m_ProxyCount = proxies_count;
  if (proxies_count) {
    m_ppProxies =
        (IMaterialProxy **)malloc(proxies_count * sizeof(IMaterialProxy *));
    memcpy(m_ppProxies, proxies, proxies_count * sizeof(IMaterialProxy *));
  } else {
    m_ppProxies = nullptr;
  }
}

// Cleans up the material proxy
void CMaterial::CleanUpMaterialProxy() {
  if (!m_ProxyCount) return;

  IMaterialProxyFactory *proxy_factory =
      MaterialSystem()->GetMaterialProxyFactory();
  if (!proxy_factory) return;

  // Clean up material proxies
  for (int i = m_ProxyCount; --i >= 0;) {
    proxy_factory->DeleteProxy(m_ppProxies[i]);
  }
  free(m_ppProxies);

  m_ppProxies = nullptr;
  m_ProxyCount = 0;
}

static char const *GetVarName(KeyValues *pVar) {
  char const *pVarName = pVar->GetName();
  char const *pQuestion = strchr(pVarName, '?');
  return !pQuestion ? pVarName : pQuestion + 1;
}

// Finds the index of the material var associated with a var
static int FindMaterialVar(IShader *the_shader, char const *var_name) {
  // Strip preceeding spaces
  var_name += strspn(var_name, " \t");

  for (int i = the_shader->GetNumParams(); --i >= 0;) {
    // Makes the parser a little more lenient.. strips off bogus spaces in the
    // var name.
    const char *param_name = the_shader->GetParamName(i);
    const char *found_name = Q_stristr(var_name, param_name);

    // The found string had better start with the first non-whitespace character
    if (found_name != var_name) continue;

    // Strip spaces at the end
    usize param_size = strlen(param_name);
    found_name += param_size;

    while (true) {
      if (!found_name[0]) return i;
      if (!IsWhitespace(found_name[0])) break;

      ++found_name;
    }
  }

  return -1;
}

// Creates a vector material var
template <usize values_count>
usize ParseVectorFromKeyValueString(KeyValues *pKeyValue,
                                    const char *pMaterialName,
                                    float (&vecVal)[values_count]) {
  static_assert(values_count == 4);

  char const *pScan = pKeyValue->GetString();
  bool divideBy255 = false;

  // skip whitespace
  while (IsWhitespace(*pScan)) {
    ++pScan;
  }

  if (*pScan == '{') {
    divideBy255 = true;
  } else {
    Assert(*pScan == '[');
  }

  // skip the '['
  ++pScan;
  usize i = 0;
  for (; i < values_count; i++) {
    // skip whitespace
    while (IsWhitespace(*pScan)) {
      ++pScan;
    }

    if (IsEndline(*pScan) || *pScan == ']' || *pScan == '}') {
      if (*pScan != ']' && *pScan != '}') {
        Warning(
            "Warning in .VMT file (%s): no ']' or '}' found in vector key "
            "\"%s\".\n"
            "Did you forget to surround the vector with \"s?\n",
            pMaterialName, pKeyValue->GetName());
      }

      // allow for vec2's, etc.
      vecVal[i] = 0.0f;
      break;
    }

    char *pEnd;

    vecVal[i] = strtod(pScan, &pEnd);
    if (pScan == pEnd) {
      Warning(
          "Error in .VMT file: error parsing vector element \"%s\" in \"%s\"\n",
          pKeyValue->GetName(), pMaterialName);
      return 0;
    }

    pScan = pEnd;
  }

  if (divideBy255) {
    vecVal[0] *= (1.0f / 255.0f);
    vecVal[1] *= (1.0f / 255.0f);
    vecVal[2] *= (1.0f / 255.0f);
    vecVal[3] *= (1.0f / 255.0f);
  }

  return i;
}

static IMaterialVar *CreateVectorMaterialVarFromKeyValue(IMaterial *material,
                                                         KeyValues *pKeyValue) {
  char const *var_name = GetVarName(pKeyValue);
  float vector_values[4];
  usize nDim =
      ParseVectorFromKeyValueString(pKeyValue, var_name, vector_values);
  if (nDim == 0) return nullptr;

  // Create the variable!
  return IMaterialVar::Create(material, var_name, vector_values, nDim);
}

// Creates a vector material var
static IMaterialVar *CreateMatrixMaterialVarFromKeyValue(IMaterial *pMaterial,
                                                         KeyValues *pKeyValue) {
  char const *pScan = pKeyValue->GetString();
  char const *pszName = GetVarName(pKeyValue);

  // Matrices can be specified one of two ways:
  // [ # # # #  # # # #  # # # #  # # # # ]
  // or
  // center # # scale # # rotate # translate # #

  VMatrix mat;
  int count =
      sscanf_s(pScan, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
               &mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
               &mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
               &mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
               &mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3]);
  if (count == 16) return IMaterialVar::Create(pMaterial, pszName, mat);

  Vector2D scale, center;
  float angle;
  Vector2D translation;
  count = sscanf_s(pScan, " center %f %f scale %f %f rotate %f translate %f %f",
                   &center.x, &center.y, &scale.x, &scale.y, &angle,
                   &translation.x, &translation.y);
  if (count != 7) return nullptr;

  VMatrix temp;
  MatrixBuildTranslation(mat, -center.x, -center.y, 0.0f);
  MatrixBuildScale(temp, scale.x, scale.y, 1.0f);
  MatrixMultiply(temp, mat, mat);
  MatrixBuildRotateZ(temp, angle);
  MatrixMultiply(temp, mat, mat);
  MatrixBuildTranslation(temp, center.x + translation.x,
                         center.y + translation.y, 0.0f);
  MatrixMultiply(temp, mat, mat);

  // Create the variable!
  return IMaterialVar::Create(pMaterial, pszName, mat);
}

// Creates a material var from a key value
static IMaterialVar *CreateMaterialVarFromKeyValue(IMaterial *pMaterial,
                                                   KeyValues *pKeyValue) {
  char const *pszName = GetVarName(pKeyValue);
  switch (pKeyValue->GetDataType()) {
    case KeyValues::TYPE_INT:
      return IMaterialVar::Create(pMaterial, pszName, pKeyValue->GetInt());

    case KeyValues::TYPE_FLOAT:
      return IMaterialVar::Create(pMaterial, pszName, pKeyValue->GetFloat());

    case KeyValues::TYPE_STRING: {
      char const *pString = pKeyValue->GetString();
      if (!pString || !pString[0]) return 0;

      // Look for matrices
      IMaterialVar *pMatrixVar =
          CreateMatrixMaterialVarFromKeyValue(pMaterial, pKeyValue);
      if (pMatrixVar) return pMatrixVar;

      // Look for vectors
      if (!IsVector(pString))
        return IMaterialVar::Create(pMaterial, pszName, pString);

      // Parse the string as a vector...
      return CreateVectorMaterialVarFromKeyValue(pMaterial, pKeyValue);
    }
  }

  return 0;
}

// Reads out common flags, prevents them from becoming material vars
int CMaterial::FindMaterialVarFlag(char const *flag_name) const {
  // Strip preceeding spaces
  while (flag_name[0]) {
    if (!IsWhitespace(flag_name[0])) break;
    ++flag_name;
  }

  for (int i = 0; *ShaderSystem()->ShaderStateString(i); ++i) {
    const char *pStateString = ShaderSystem()->ShaderStateString(i);
    const char *pFound = Q_stristr(flag_name, pStateString);

    // The found string had better start with the first non-whitespace character
    if (pFound != flag_name) continue;

    // Strip spaces at the end
    usize nLen = strlen(pStateString);
    pFound += nLen;
    while (true) {
      if (!pFound[0]) return (1 << i);

      if (!IsWhitespace(pFound[0])) break;

      ++pFound;
    }
  }
  return 0;
}

// Print material flags
void CMaterial::PrintMaterialFlags(int flags, int flagsDefined) {
  for (int i = 0; *ShaderSystem()->ShaderStateString(i); i++) {
    if (flags & (1 << i)) {
      Warning("%s|", ShaderSystem()->ShaderStateString(i));
    }
  }
  Warning("\n");
}

// Parses material flags
bool CMaterial::ParseMaterialFlag(KeyValues *pParseValue,
                                  IMaterialVar *pFlagVar,
                                  IMaterialVar *pFlagDefinedVar,
                                  bool parsingOverrides, int &flagMask,
                                  int &overrideMask) {
  // See if the var is a flag...
  int flagbit = FindMaterialVarFlag(GetVarName(pParseValue));
  if (!flagbit) return false;

  // Allow for flag override
  int testMask = parsingOverrides ? overrideMask : flagMask;
  if (testMask & flagbit) {
    Warning("Error! Flag \"%s\" is multiply defined in material \"%s\"!\n",
            pParseValue->GetName(), GetName());
    return true;
  }

  // Make sure overrides win
  if (overrideMask & flagbit) return true;

  if (parsingOverrides)
    overrideMask |= flagbit;
  else
    flagMask |= flagbit;

  // If so, then set the flag bit
  pFlagVar->SetIntValue(pParseValue->GetInt()
                            ? (pFlagVar->GetIntValue() | flagbit)
                            : (pFlagVar->GetIntValue() & (~flagbit)));

  // Mark the flag as being defined
  pFlagDefinedVar->SetIntValue(pFlagDefinedVar->GetIntValue() | flagbit);

  return true;
}

bool CMaterial::ShouldSkipVar(KeyValues *var_values, bool *was_conditional) {
  char const *var_name{var_values->GetName()};
  char const *question{strchr(var_name, '?')};

  if (!question || question == var_name) {
    // Unconditional var.
    *was_conditional = false;
    return false;
  }

  *was_conditional = true;

  // Parse the conditional part.
  char condition_name[256];
  V_strncpy(condition_name, var_name, 1 + question - var_name);

  char const *the_condition{condition_name};
  bool should_toggle{false};
  if (the_condition[0] == '!') {
    ++the_condition;
    should_toggle = true;
  }

  bool should_skip = true;
  if (!_stricmp(the_condition, "lowfill")) {
    should_skip = !HardwareConfig()->PreferReducedFillrate();
  } else if (!_stricmp(the_condition, "hdr")) {
    should_skip = HardwareConfig()->GetHDRType() == HDR_TYPE_NONE;
  } else if (!_stricmp(the_condition, "srgb")) {
    should_skip = !HardwareConfig()->UsesSRGBCorrectBlending();
  } else if (!_stricmp(the_condition, "ldr")) {
    should_skip = HardwareConfig()->GetHDRType() != HDR_TYPE_NONE;
  } else if (!_stricmp(the_condition, "360")) {
    should_skip = true;
  } else {
    Warning("Unrecognized conditional test %s in %s\n", var_name, GetName());
  }

  return should_skip ^ should_toggle;
}

// Computes the material vars for the shader
template <usize vars_size>
int CMaterial::ParseMaterialVars(IShader *the_shader, KeyValues &main_values,
                                 KeyValues *override_values,
                                 bool is_default_model,
                                 IMaterialVar *(&vars)[vars_size]) {
  if (!the_shader) return 0;

  memset(vars, 0, sizeof(vars));

  int overrideMask = 0;
  int flagMask = 0;

  bool has_override[vars_size];
  memset(has_override, 0, sizeof(has_override));

  bool was_conditional[vars_size];
  memset(was_conditional, 0, sizeof(was_conditional));

  // Create the flag var...
  // Set model mode if we fell back from a model mode shader
  int model_flag = is_default_model ? MATERIAL_VAR_MODEL : 0;
  vars[FLAGS] = IMaterialVar::Create(this, "$flags", model_flag);
  vars[FLAGS_DEFINED] =
      IMaterialVar::Create(this, "$flags_defined", model_flag);
  vars[FLAGS2] = IMaterialVar::Create(this, "$flags2", 0);
  vars[FLAGS_DEFINED2] = IMaterialVar::Create(this, "$flags_defined2", 0);

  int numParams = the_shader->GetNumParams();
  int varCount = numParams;

  bool should_parse_overrides = override_values != nullptr;
  KeyValues *values = override_values ? override_values->GetFirstSubKey()
                                      : main_values.GetFirstSubKey();
  while (values) {
    // See if the var is a flag...
    bool is_conditional_var,
        should_process_this = !(
            ShouldSkipVar(values, &is_conditional_var) ||  // should skip?
            ((values->GetName()[0] == '%') &&
             (g_pShaderDevice->IsUsingGraphics()) &&
             (!MaterialSystem()
                   ->CanUseEditorMaterials())) ||  // is an editor var?
            ParseMaterialFlag(values, vars[FLAGS], vars[FLAGS_DEFINED],
                              should_parse_overrides, flagMask,
                              overrideMask) ||  // is a flag?
            ParseMaterialFlag(values, vars[FLAGS2], vars[FLAGS_DEFINED2],
                              should_parse_overrides, flagMask, overrideMask));

    if (should_process_this) {
      // See if the var is one of the shader params
      int var_idx = FindMaterialVar(the_shader, GetVarName(values));

      // Check for multiply defined or overridden
      if (var_idx >= 0) {
        if (vars[var_idx] && (!is_conditional_var)) {
          if (!has_override[var_idx] || should_parse_overrides) {
            Warning(
                "Error! Variable \"%s\" is multiply defined in material "
                "\"%s\"!\n",
                values->GetName(), GetName());
          }
          goto nextVar;
        }
      } else {
        int i;
        for (i = numParams; i < varCount; ++i) {
          Assert(vars[i]);
          if (!_stricmp(vars[i]->GetName(), values->GetName())) break;
        }

        if (i != varCount) {
          if (!has_override[i] || should_parse_overrides) {
            Warning(
                "Error! Variable \"%s\" is multiply defined in material "
                "\"%s\"!\n",
                values->GetName(), GetName());
          }
          goto nextVar;
        }
      }

      // Create a material var for this dudely dude; could be zero...
      IMaterialVar *new_var = CreateMaterialVarFromKeyValue(this, values);
      if (!new_var) goto nextVar;

      if (var_idx < 0) var_idx = varCount++;
      if (vars[var_idx]) IMaterialVar::Destroy(vars[var_idx]);

      vars[var_idx] = new_var;

      if (should_parse_overrides) has_override[var_idx] = true;

      was_conditional[var_idx] = is_conditional_var;
    }

  nextVar:
    values = values->GetNextKey();

    if (!values && should_parse_overrides) {
      values = main_values.GetFirstSubKey();
      should_parse_overrides = false;
    }
  }

  // Create undefined vars for all the actual material vars
  for (int i = 0; i < numParams; ++i) {
    if (!vars[i])
      vars[i] = IMaterialVar::Create(this, the_shader->GetParamName(i));
  }

  return varCount;
}

static KeyValues *CheckConditionalFakeShaderName(char const *shader_name,
                                                 char const *suffix_name,
                                                 KeyValues *values) {
  KeyValues *fallback_values = values->FindKey(suffix_name);
  if (fallback_values) return fallback_values;

  char name[256];
  sprintf_s(name, "%s_%s", shader_name, suffix_name);

  return values->FindKey(name);
}

static KeyValues *FindBuiltinFallbackBlock(char const *pShaderName,
                                           KeyValues *pKeyValues) {
  int dx_support_level{HardwareConfig()->GetDXSupportLevel()};
  // handle "fake" shader fallbacks which are conditional upon mode. like
  // _hdr_dx9, etc
  if (dx_support_level < 90) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "<DX90", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level < 95) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "<DX95", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level < 90 || !HardwareConfig()->SupportsPixelShaders_2_b()) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "<DX90_20b", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level >= 90 && HardwareConfig()->SupportsPixelShaders_2_b()) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, ">=DX90_20b", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level <= 90) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "<=DX90", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level >= 90) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, ">=DX90", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level > 90) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, ">DX90", pKeyValues);
    if (pRet) return pRet;
  }
  if (HardwareConfig()->GetHDRType() != HDR_TYPE_NONE) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "hdr_dx9", pKeyValues);
    if (pRet) return pRet;

    pRet = CheckConditionalFakeShaderName(pShaderName, "hdr", pKeyValues);
    if (pRet) return pRet;
  } else {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "ldr", pKeyValues);
    if (pRet) return pRet;
  }
  if (HardwareConfig()->UsesSRGBCorrectBlending()) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "srgb", pKeyValues);
    if (pRet) return pRet;
  }
  if (dx_support_level >= 90) {
    KeyValues *pRet =
        CheckConditionalFakeShaderName(pShaderName, "dx9", pKeyValues);
    if (pRet) return pRet;
  }
  return nullptr;
}

// Hooks up the shader
KeyValues *CMaterial::InitializeShader(KeyValues &keyValues,
                                       KeyValues &patchKeyValues) {
  MaterialLock_t material_lock = MaterialSystem()->Lock();

  KeyValues *current_fallback_values = &keyValues;
  KeyValues *fallback_values = nullptr;

  char the_shader_name[SOURCE_MAX_PATH];
  char const *shader_name = current_fallback_values->GetName();
  if (!shader_name) {
    // I'm not quite sure how this can happen, but we'll see...
    Warning("Shader not specified in material %s\nUsing wireframe instead...\n",
            GetName());
    Assert(0);
    shader_name = "Wireframe_DX6";
  } else {
    // can't pass a stable reference to the key values name around
    // naive leaf functions can cause KV system to re-alloc
    strcpy_s(the_shader_name, shader_name);
    shader_name = the_shader_name;
  }

  IShader *the_shader;
  IMaterialVar *material_vars[256];
  char fallback_shader_name[256], fallback_material_name[256];
  int varCount = 0;
  bool modelDefault = false;

  // Keep going until there's no more fallbacks...
  while (true) {
    // Find the shader for this material. Note that this may not be
    // the actual shader we use due to fallbacks...
    the_shader = ShaderSystem()->FindShader(shader_name);

    if (!the_shader) {
      if (g_pShaderDevice->IsUsingGraphics()) {
        Warning("Error: Material \"%s\" uses unknown shader \"%s\"\n",
                GetName(), shader_name);
        Assert(0);
      }

      shader_name = "Wireframe_DX6";
      the_shader = ShaderSystem()->FindShader(shader_name);

      Assert(the_shader);
    }

    if (!the_shader) {
      MaterialSystem()->Unlock(material_lock);
      return nullptr;
    }

    bool has_builtin_fallback = false;
    if (!fallback_values) {
      fallback_values = FindBuiltinFallbackBlock(shader_name, &keyValues);

      if (fallback_values) {
        has_builtin_fallback = true;
        fallback_values->ChainKeyValue(&keyValues);
        current_fallback_values = fallback_values;
      }
    }

    // Here we must set up all flags + material vars that the shader needs
    // because it may look at them when choosing shader fallback.
    varCount = ParseMaterialVars(the_shader, keyValues, fallback_values,
                                 modelDefault, material_vars);

    // Make sure we set default values before the fallback is looked for
    ShaderSystem()->InitShaderParameters(the_shader, material_vars, GetName());

    // Now that the material vars are parsed, see if there's a fallback
    // But only if we're not in the tools
    // Check for a fallback; if not, we're done
    shader_name = the_shader->GetFallbackShader(material_vars);
    if (!shader_name) break;

    // Copy off the shader name, as it may be in a materialvar in the shader
    // because we're about to delete all materialvars
    strcpy_s(fallback_shader_name, shader_name);
    shader_name = fallback_shader_name;

    // Remember the model flag if we're on dx7 or higher...
    if (HardwareConfig()->SupportsVertexAndPixelShaders()) {
      modelDefault =
          (material_vars[FLAGS]->GetIntValue() & MATERIAL_VAR_MODEL) != 0;
    }

    // Try to get the section associated with the fallback shader
    // Then chain it to the base data so it can override the
    // values if it wants to
    if (!has_builtin_fallback) {
      fallback_values = keyValues.FindKey(shader_name);
      if (fallback_values) {
        fallback_values->ChainKeyValue(&keyValues);
        current_fallback_values = fallback_values;
      }
    }

    // Now, blow away all of the material vars + try again...
    for (int i = 0; i < varCount; ++i) {
      Assert(material_vars[i]);
      IMaterialVar::Destroy(material_vars[i]);
    }

    // Check the KeyValues for '$fallbackmaterial'
    // Note we have to do this *after* we chain the keyvalues
    // based on the fallback shader	since the names of the fallback material
    // must lie within the shader-specific block usually.
    const char *fallback_material =
        current_fallback_values->GetString("$fallbackmaterial");
    if (fallback_material && fallback_material[0]) {
      // Gotta copy it off; clearing the keyvalues will blow the string away
      strcpy_s(fallback_material_name, fallback_material);
      keyValues.Clear();

      if (!LoadVMTFile(keyValues, patchKeyValues, fallback_material_name,
                       UsesUNCFileName(), nullptr)) {
        Warning("CMaterial::PrecacheVars: error loading vmt file %s for %s\n",
                fallback_material_name, GetName());
        keyValues = *(((CMaterial *)g_pErrorMaterial)->m_pVMTKeyValues);
      }

      current_fallback_values = &keyValues;
      fallback_values = nullptr;

      // I'm not quite sure how this can happen, but we'll see...
      shader_name = current_fallback_values->GetName();
      if (!shader_name) {
        Warning(
            "Shader not specified in material %s (fallback %s)\nUsing "
            "wireframe instead...\n",
            GetName(), fallback_material_name);
        shader_name = "Wireframe_DX6";
      }
    }
  }

  // Store off the shader
  m_pShader = the_shader;

  // Store off the material vars + flags
  m_VarCount = varCount;
  m_pShaderParams = (IMaterialVar **)malloc(varCount * sizeof(IMaterialVar *));
  memcpy(m_pShaderParams, material_vars, varCount * sizeof(IMaterialVar *));

#ifndef NDEBUG
  for (int i = 0; i < varCount; ++i) {
    Assert(ppVars[i]);
  }
#endif

  MaterialSystem()->Unlock(material_lock);
  return current_fallback_values;
}

// Gets the texturemap size
void CMaterial::PrecacheMappingDimensions() {
  // Cache mapping width and mapping height
  if (!m_representativeTexture) {
#ifdef PARANOID
    Warning("No representative texture on material: \"%s\"\n", GetName());
#endif
    m_MappingWidth = 64;
    m_MappingHeight = 64;
  } else {
    m_MappingWidth = m_representativeTexture->GetMappingWidth();
    m_MappingHeight = m_representativeTexture->GetMappingHeight();
  }
}

// Initialize the state snapshot
bool CMaterial::InitializeStateSnapshots() {
  if (IsPrecached()) {
    if (MaterialSystem()->GetCurrentMaterial() == this) {
      g_pShaderAPI->FlushBufferedPrimitives();
    }

    // Default state
    CleanUpStateSnapshots();

    if (m_pShader &&
        !ShaderSystem()->InitRenderState(m_pShader, m_VarCount, m_pShaderParams,
                                         &m_ShaderRenderState, GetName()))
      return false;

    m_Flags |= MATERIAL_VALID_RENDERSTATE;
  }

  return true;
}

void CMaterial::CleanUpStateSnapshots() {
  if (IsValidRenderState()) {
    ShaderSystem()->CleanupRenderState(&m_ShaderRenderState);
    m_Flags &= ~MATERIAL_VALID_RENDERSTATE;
  }
}

//-----------------------------------------------------------------------------
// This sets up a debugging/error shader...
//-----------------------------------------------------------------------------
void CMaterial::SetupErrorShader() {
  // Preserve the model flags
  int flags = 0;
  if (m_pShaderParams && m_pShaderParams[FLAGS]) {
    flags = (m_pShaderParams[FLAGS]->GetIntValue() & MATERIAL_VAR_MODEL);
  }

  CleanUpShaderParams();
  CleanUpMaterialProxy();

  // We had a failure; replace it with a valid shader...

  m_pShader = ShaderSystem()->FindShader("Wireframe_DX6");
  Assert(m_pShader);

  // Create undefined vars for all the actual material vars
  m_VarCount = m_pShader->GetNumParams();
  m_pShaderParams =
      (IMaterialVar **)malloc(m_VarCount * sizeof(IMaterialVar *));

  for (int i = 0; i < m_VarCount; ++i) {
    m_pShaderParams[i] = IMaterialVar::Create(this, m_pShader->GetParamName(i));
  }

  // Store the model flags
  SetMaterialVarFlags(flags, true);

  // Set the default values
  ShaderSystem()->InitShaderParameters(m_pShader, m_pShaderParams, "Error");

  // Invokes the SHADER_INIT block in the various shaders,
  ShaderSystem()->InitShaderInstance(m_pShader, m_pShaderParams, "Error",
                                     GetTextureGroupName());

#ifndef NDEBUG
  bool ok =
#endif
      InitializeStateSnapshots();

#ifndef NDEBUG
  Assert(ok);
#endif

  m_QueueFriendlyVersion.UpdateToRealTime();

  Assert(ok);
}

// This computes the state snapshots for this material
void CMaterial::RecomputeStateSnapshots() {
  CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
  if (pCallQueue) {
    pCallQueue->QueueCall(this, &CMaterial::RecomputeStateSnapshots);
    return;
  }

  bool ok = InitializeStateSnapshots();
  // compute the state snapshots
  if (!ok) SetupErrorShader();
}

// Are we valid
inline bool CMaterial::IsValidRenderState() const {
  return (m_Flags & MATERIAL_VALID_RENDERSTATE) != 0;
}

// Gets/sets material var flags
inline int CMaterial::GetMaterialVarFlags() const {
  if (m_pShaderParams && m_pShaderParams[FLAGS]) {
    return m_pShaderParams[FLAGS]->GetIntValueFast();
  }

  return 0;
}

inline void CMaterial::SetMaterialVarFlags(int flags, bool on) {
  m_pShaderParams[FLAGS]->SetIntValue(on ? (GetMaterialVarFlags() | flags)
                                         : (GetMaterialVarFlags() & (~flags)));

  // Mark it as being defined...
  m_pShaderParams[FLAGS_DEFINED]->SetIntValue(
      m_pShaderParams[FLAGS_DEFINED]->GetIntValueFast() | flags);
}

inline int CMaterial::GetMaterialVarFlags2() const {
  if (m_pShaderParams && m_pShaderParams[FLAGS2]) {
    return m_pShaderParams[FLAGS2]->GetIntValueFast();
  }

  return 0;
}

inline void CMaterial::SetMaterialVarFlags2(int flags, bool on) {
  if (m_pShaderParams && m_pShaderParams[FLAGS2]) {
    m_pShaderParams[FLAGS2]->SetIntValue(
        on ? (GetMaterialVarFlags2() | flags)
           : (GetMaterialVarFlags2() & (~flags)));
  }

  if (m_pShaderParams && m_pShaderParams[FLAGS_DEFINED2]) {
    // Mark it as being defined...
    m_pShaderParams[FLAGS_DEFINED2]->SetIntValue(
        m_pShaderParams[FLAGS_DEFINED2]->GetIntValueFast() | flags);
  }
}

// Gets the morph format
MorphFormat_t CMaterial::GetMorphFormat() const {
  const_cast<CMaterial *>(this)->Precache();
  Assert(IsValidRenderState());
  return m_ShaderRenderState.m_MorphFormat;
}

// Gets the vertex format
VertexFormat_t CMaterial::GetVertexFormat() const {
  Assert(IsValidRenderState());
  return m_ShaderRenderState.m_VertexFormat;
}

VertexFormat_t CMaterial::GetVertexUsage() const {
  Assert(IsValidRenderState());
  return m_ShaderRenderState.m_VertexUsage;
}

bool CMaterial::PerformDebugTrace() const {
  return IsValidRenderState() &&
         ((GetMaterialVarFlags() & MATERIAL_VAR_DEBUG) != 0);
}

// Are we suppressed?
bool CMaterial::IsSuppressed() const {
  if (!IsValidRenderState()) return true;

  return ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) != 0);
}

void CMaterial::ToggleSuppression() {
  if (IsValidRenderState()) {
    if ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DEBUG_OVERRIDE) != 0) return;

    SetMaterialVarFlags(MATERIAL_VAR_NO_DRAW,
                        (GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) == 0);
  }
}

void CMaterial::ToggleDebugTrace() {
  if (IsValidRenderState()) {
    SetMaterialVarFlags(MATERIAL_VAR_DEBUG,
                        (GetMaterialVarFlags() & MATERIAL_VAR_DEBUG) == 0);
  }
}

// Can we override this material in debug?
bool CMaterial::NoDebugOverride() const {
  return IsValidRenderState() &&
         (GetMaterialVarFlags() & MATERIAL_VAR_NO_DEBUG_OVERRIDE) != 0;
}

// Material Var flags
void CMaterial::SetMaterialVarFlag(MaterialVarFlags_t flag, bool on) {
  CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
  if (pCallQueue) {
    pCallQueue->QueueCall(this, &CMaterial::SetMaterialVarFlag, flag, on);
    return;
  }

  bool oldOn = (GetMaterialVarFlags() & flag) != 0;
  if (oldOn != on) {
    SetMaterialVarFlags(flag, on);

    // This is going to be called from client code; recompute snapshots!
    RecomputeStateSnapshots();
  }
}

bool CMaterial::GetMaterialVarFlag(MaterialVarFlags_t flag) const {
  return (GetMaterialVarFlags() & flag) != 0;
}

// Do we use the env_cubemap entity to get cubemaps from the level?
bool CMaterial::UsesEnvCubemap() {
  Precache();
  Assert(m_pShader);
  if (!m_pShader) return false;

  Assert(m_pShaderParams);
  return IsFlag2Set(m_pShaderParams, MATERIAL_VAR2_USES_ENV_CUBEMAP);
}

// Do we need a tangent space at the vertex level?
bool CMaterial::NeedsTangentSpace() {
  Precache();
  Assert(m_pShader);
  if (!m_pShader) return false;

  Assert(m_pShaderParams);
  return IsFlag2Set(m_pShaderParams, MATERIAL_VAR2_NEEDS_TANGENT_SPACES);
}

bool CMaterial::NeedsPowerOfTwoFrameBufferTexture(
    bool bCheckSpecificToThisFrame) {
  Precache();
  Assert(m_pShader);
  if (!m_pShader) return false;

  Assert(m_pShaderParams);
  return m_pShader->NeedsPowerOfTwoFrameBufferTexture(
      m_pShaderParams, bCheckSpecificToThisFrame);
}

bool CMaterial::NeedsFullFrameBufferTexture(bool bCheckSpecificToThisFrame) {
  Precache();
  Assert(m_pShader);
  if (!m_pShader) return false;

  Assert(m_pShaderParams);
  return m_pShader->NeedsFullFrameBufferTexture(m_pShaderParams,
                                                bCheckSpecificToThisFrame);
}

// GR - Is lightmap alpha needed?
bool CMaterial::NeedsLightmapBlendAlpha() {
  Precache();
  return (GetMaterialVarFlags2() & MATERIAL_VAR2_BLEND_WITH_LIGHTMAP_ALPHA) !=
         0;
}

// Do we need software skinning?
bool CMaterial::NeedsSoftwareSkinning() {
  Precache();
  Assert(m_pShader);
  if (!m_pShader) {
    return false;
  }
  Assert(m_pShaderParams);
  return IsFlagSet(m_pShaderParams, MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING);
}

// Do we need software lighting?
bool CMaterial::NeedsSoftwareLighting() {
  Precache();

  Assert(m_pShader);
  if (!m_pShader) return false;

  Assert(m_pShaderParams);
  return IsFlag2Set(m_pShaderParams, MATERIAL_VAR2_NEEDS_SOFTWARE_LIGHTING);
}

// Alpha/color modulation
void CMaterial::AlphaModulate(float alpha) {
  Precache();
  m_pShaderParams[ALPHA]->SetFloatValue(alpha);
}

void CMaterial::ColorModulate(float r, float g, float b) {
  Precache();
  m_pShaderParams[COLOR]->SetVecValue(r, g, b);
}

float CMaterial::GetAlphaModulation() {
  Precache();
  return m_pShaderParams[ALPHA]->GetFloatValue();
}

void CMaterial::GetColorModulation(float *r, float *g, float *b) {
  Precache();

  float pColor[3];
  m_pShaderParams[COLOR]->GetVecValue(pColor, 3);
  *r = pColor[0];
  *g = pColor[1];
  *b = pColor[2];
}

// Do we use fog?
bool CMaterial::UseFog() const {
  Assert(m_VarCount > 0);
  return IsValidRenderState() &&
         ((GetMaterialVarFlags() & MATERIAL_VAR_NOFOG) == 0);
}

// diffuse bump?
bool CMaterial::IsUsingDiffuseBumpedLighting() const {
  return (GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP) != 0;
}

// lightmap?
bool CMaterial::IsUsingLightmap() const {
  return (GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_LIGHTMAP) != 0;
}

bool CMaterial::IsManuallyCreated() const {
  return (m_Flags & MATERIAL_IS_MANUALLY_CREATED) != 0;
}

bool CMaterial::UsesUNCFileName() const {
  return (m_Flags & MATERIAL_USES_UNC_FILENAME) != 0;
}

void CMaterial::DecideShouldReloadFromWhitelist(IFileList *pFilesToReload) {
  m_bShouldReloadFromWhitelist = false;
  if (IsManuallyCreated() || !IsPrecached()) return;

  // Materials loaded with an absolute pathname are usually debug materials.
  if (V_IsAbsolutePath(GetName())) return;

  char vmtFilename[SOURCE_MAX_PATH];
  V_ComposeFileName("materials", GetName(), vmtFilename, sizeof(vmtFilename));
  V_strncat(vmtFilename, ".vmt", sizeof(vmtFilename));

  // Check if either this file or any of the files it included need to be
  // reloaded.
  bool bShouldReload = pFilesToReload->IsFileInList(vmtFilename);
  if (!bShouldReload) {
    for (int i = 0; i < m_VMTIncludes.Count(); i++) {
      g_pFullFileSystem->String(m_VMTIncludes[i], vmtFilename,
                                sizeof(vmtFilename));
      bShouldReload = pFilesToReload->IsFileInList(vmtFilename);
      if (bShouldReload) break;
    }
  }

  m_bShouldReloadFromWhitelist = bShouldReload;
}

void CMaterial::ReloadFromWhitelistIfMarked() {
  if (!m_bShouldReloadFromWhitelist) return;

  Uncache();
  Precache();

  if (!GetShader()) {
    // We can get in here if we previously loaded this material off disk and now
    // the whitelist says to get it out of Steam but it's not in Steam. So just
    // setup a wireframe thingy to draw the material with.
    m_Flags |= MATERIAL_IS_PRECACHED | MATERIAL_VARS_IS_PRECACHED;
    SetupErrorShader();
  }
}

bool CMaterial::WasReloadedFromWhitelist() {
  return m_bShouldReloadFromWhitelist;
}

// Loads the material vars
bool CMaterial::PrecacheVars(KeyValues *pVMTKeyValues,
                             KeyValues *pPatchKeyValues,
                             CUtlVector<FileNameHandle_t> *pIncludes) {
  // We should get both parameters or neither
  Assert((pVMTKeyValues == nullptr) ? (pPatchKeyValues == nullptr)
                                    : (pPatchKeyValues != nullptr));

  // Don't bother if we're already precached
  if (IsPrecachedVars()) return true;

  if (pIncludes)
    m_VMTIncludes = *pIncludes;
  else
    m_VMTIncludes.Purge();

  MaterialLock_t lock = MaterialSystem()->Lock();

  bool is_ok = false, has_error = false;
  KeyValues *vmtKeyValues = nullptr, *patchKeyValues = nullptr;

  if (m_pVMTKeyValues) {
    // Use the procedural KeyValues
    vmtKeyValues = m_pVMTKeyValues;
    patchKeyValues = new KeyValues("vmt_patches");

    // The caller should not be passing in KeyValues if we have procedural ones
    Assert((pVMTKeyValues == nullptr) && (pPatchKeyValues == nullptr));
  } else if (pVMTKeyValues) {
    // Use the passed-in (already-loaded) KeyValues
    vmtKeyValues = pVMTKeyValues;
    patchKeyValues = pPatchKeyValues;
  } else {
    m_VMTIncludes.Purge();

    // load data from the vmt file
    vmtKeyValues = new KeyValues("vmt");
    patchKeyValues = new KeyValues("vmt_patches");
    if (!LoadVMTFile(*vmtKeyValues, *patchKeyValues, GetName(),
                     UsesUNCFileName(), &m_VMTIncludes)) {
      Warning("CMaterial::PrecacheVars: error loading vmt file for %s\n",
              GetName());
      has_error = true;
    }
  }

  if (!has_error) {
    // Needed to prevent re-entrancy
    m_Flags |= MATERIAL_VARS_IS_PRECACHED;

    // Create shader and the material vars...
    KeyValues *pFallbackKeyValues =
        InitializeShader(*vmtKeyValues, *patchKeyValues);
    if (pFallbackKeyValues) {
      // Gotta initialize the proxies too, using the fallback proxies
      InitializeMaterialProxy(pFallbackKeyValues);
      is_ok = true;
    }
  }

  // Clean up
  if ((vmtKeyValues != m_pVMTKeyValues) && (vmtKeyValues != pVMTKeyValues)) {
    vmtKeyValues->deleteThis();
  }
  if (patchKeyValues != pPatchKeyValues) {
    patchKeyValues->deleteThis();
  }

  MaterialSystem()->Unlock(lock);

  return is_ok;
}

// Loads the material info from the VMT file
void CMaterial::Precache() {
  // Don't bother if we're already precached
  if (IsPrecached()) return;

  // load data from the vmt file
  if (!PrecacheVars()) return;

  MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

  m_Flags |= MATERIAL_IS_PRECACHED;

  // Invokes the SHADER_INIT block in the various shaders,
  if (m_pShader) {
    ShaderSystem()->InitShaderInstance(m_pShader, m_pShaderParams, GetName(),
                                       GetTextureGroupName());
  }

  // compute the state snapshots
  RecomputeStateSnapshots();

  FindRepresentativeTexture();

  // Reads in the texture width and height from the material var
  PrecacheMappingDimensions();

  Assert(IsValidRenderState());

  if (m_pShaderParams) m_QueueFriendlyVersion.UpdateToRealTime();

  MaterialSystem()->Unlock(hMaterialLock);
}

// Unloads the material data from memory
void CMaterial::Uncache(bool bPreserveVars) {
  MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

  // Don't bother if we're not cached
  if (IsPrecached()) {
    // Clean up the state snapshots
    CleanUpStateSnapshots();

    m_Flags &= ~MATERIAL_IS_PRECACHED;
  }

  if (!bPreserveVars) {
    if (IsPrecachedVars()) {
      // Clean up the shader + params
      CleanUpShaderParams();
      m_pShader = 0;

      // Clean up the material proxy
      CleanUpMaterialProxy();

      m_Flags &= ~MATERIAL_VARS_IS_PRECACHED;
    }
  }

  MaterialSystem()->Unlock(hMaterialLock);
}

// reload all textures used by this materals
void CMaterial::ReloadTextures() {
  Precache();

  int nParams = ShaderParamCount();
  IMaterialVar **ppVars = GetShaderParams();

  for (int i = 0; i < nParams; i++) {
    if (ppVars[i] && ppVars[i]->IsTexture()) {
      auto *texture = (ITextureInternal *)ppVars[i]->GetTextureValue();

      if (!IsTextureInternalEnvCubemap(texture)) texture->Download();
    }
  }
}

// Meant to be used with materials created using CreateMaterial It updates the
// materials to reflect the current values stored in the material vars
void CMaterial::Refresh() {
  if (g_pShaderDevice->IsUsingGraphics()) {
    Uncache();
    Precache();
  }
}

void CMaterial::RefreshPreservingMaterialVars() {
  if (g_pShaderDevice->IsUsingGraphics()) {
    Uncache(true);
    Precache();
  }
}

// Gets the material name
char const *CMaterial::GetName() const { return m_Name.String(); }

char const *CMaterial::GetTextureGroupName() const {
  return m_TextureGroupName.String();
}

// Material dimensions
int CMaterial::GetMappingWidth() {
  Precache();
  return m_MappingWidth;
}

int CMaterial::GetMappingHeight() {
  Precache();
  return m_MappingHeight;
}

// Animated material info
int CMaterial::GetNumAnimationFrames() {
  Precache();
  if (m_representativeTexture) {
    return m_representativeTexture->GetNumAnimationFrames();
  }

#ifndef OS_POSIX
  Warning(
      "CMaterial::GetNumAnimationFrames:\nno representative texture for "
      "material %s\n",
      GetName());
#endif
  return 1;
}

void CMaterial::GetMaterialOffset(float *pOffset) {
  // Identity.
  pOffset[0] = 0.0f;
  pOffset[1] = 0.0f;
}

void CMaterial::GetMaterialScale(float *pScale) {
  // Identity.
  pScale[0] = 1.0f;
  pScale[1] = 1.0f;
}

// Reference count
void CMaterial::IncrementReferenceCount() { ++m_RefCount; }

void CMaterial::DecrementReferenceCount() { --m_RefCount; }

int CMaterial::GetReferenceCount() const { return m_RefCount; }

// Sets the shader associated with the material
void CMaterial::SetShader(const char *pShaderName) {
  Assert(pShaderName);

  int i;
  IShader *pShader;
  IMaterialVar *ppVars[256];
  int iVarCount = 0;

  // Clean up existing state
  Uncache();

  // Keep going until there's no more fallbacks...
  while (true) {
    // Find the shader for this material. Note that this may not be
    // the actual shader we use due to fallbacks...
    pShader = ShaderSystem()->FindShader(pShaderName);
    if (!pShader) {
      // Couldn't find the shader we wanted to use; it's not defined...
      Warning("SetShader: Couldn't find shader %s for material %s!\n",
              pShaderName, GetName());
      pShaderName = IsPC() ? "Wireframe_DX6" : "Wireframe_DX9";
      pShader = ShaderSystem()->FindShader(pShaderName);
      Assert(pShader);
    }

    // Create undefined vars for all the actual material vars
    iVarCount = pShader->GetNumParams();
    for (i = 0; i < iVarCount; ++i) {
      ppVars[i] = IMaterialVar::Create(this, pShader->GetParamName(i));
    }

    // Make sure we set default values before the fallback is looked for
    ShaderSystem()->InitShaderParameters(pShader, ppVars, pShaderName);

    // Now that the material vars are parsed, see if there's a fallback
    // But only if we're not in the tools
    if (!g_pShaderDevice->IsUsingGraphics()) break;

    // Check for a fallback; if not, we're done
    pShaderName = pShader->GetFallbackShader(ppVars);
    if (!pShaderName) break;

    // Now, blow away all of the material vars + try again...
    for (i = 0; i < iVarCount; ++i) {
      Assert(ppVars[i]);
      IMaterialVar::Destroy(ppVars[i]);
    }
  }

  // Store off the shader
  m_pShader = pShader;

  // Store off the material vars + flags
  m_VarCount = iVarCount;
  m_pShaderParams = (IMaterialVar **)malloc(iVarCount * sizeof(IMaterialVar *));
  memcpy(m_pShaderParams, ppVars, iVarCount * sizeof(IMaterialVar *));

  // Invokes the SHADER_INIT block in the various shaders,
  ShaderSystem()->InitShaderInstance(m_pShader, m_pShaderParams, GetName(),
                                     GetTextureGroupName());

  // Precache our initial state...
  // NOTE: What happens here for textures???

  // Pretend that we precached our material vars; we certainly don't have any!
  m_Flags |= MATERIAL_VARS_IS_PRECACHED;

  // NOTE: The caller has to call 'Refresh' for the shader to be ready...
}

const char *CMaterial::GetShaderName() const { return m_pShader->GetName(); }

// Enumeration ID
int CMaterial::GetEnumerationID() const { return m_iEnumerationID; }

void CMaterial::SetEnumerationID(int id) { m_iEnumerationID = id; }

// Preview image
char const *CMaterial::GetPreviewImageName() {
  PrecacheVars();

  bool is_found;
  FindVar("%noToolTexture", &is_found, false);
  if (is_found) return nullptr;

  IMaterialVar *var = FindVar("%toolTexture", &is_found, false);
  if (is_found) {
    if (var->GetType() == MATERIAL_VAR_TYPE_STRING)
      return var->GetStringValue();

    if (var->GetType() == MATERIAL_VAR_TYPE_TEXTURE)
      return var->GetTextureValue()->GetName();
  }

  var = FindVar("$baseTexture", &is_found, false);
  if (is_found) {
    if (var->GetType() == MATERIAL_VAR_TYPE_STRING)
      return var->GetStringValue();

    if (var->GetType() == MATERIAL_VAR_TYPE_TEXTURE)
      return var->GetTextureValue()->GetName();
  }

  return GetName();
}

char const *CMaterial::GetPreviewImageFileName() const {
  char const *pName = const_cast<CMaterial *>(this)->GetPreviewImageName();
  if (!pName) return nullptr;

  static char vtf_file_path[MATERIAL_MAX_PATH];
  if (strlen(pName) >= MATERIAL_MAX_PATH - 5) {
    Warning("MATERIAL_MAX_PATH (%d) too short for %s.vtf (%zu)\n",
            MATERIAL_MAX_PATH, pName, strlen(pName) + 5);
    return nullptr;
  }

  sprintf_s(vtf_file_path, !UsesUNCFileName() ? "materials/%s.vtf" : "%s.vtf",
            pName);

  return vtf_file_path;
}

PreviewImageRetVal_t CMaterial::GetPreviewImageProperties(
    int *width, int *height, ImageFormat *imageFormat,
    bool *isTranslucent) const {
  char const *pFileName = GetPreviewImageFileName();
  if (!pFileName) {
    *width = *height = 0;
    *imageFormat = IMAGE_FORMAT_RGBA8888;
    *isTranslucent = false;
    return MATERIAL_NO_PREVIEW_IMAGE;
  }

  int nHeaderSize = VTFFileHeaderSize(VTF_MAJOR_VERSION);
  unsigned char *pMem = (unsigned char *)stackalloc(nHeaderSize);
  CUtlBuffer buf(pMem, nHeaderSize);

  if (!g_pFullFileSystem->ReadFile(pFileName, nullptr, buf, nHeaderSize)) {
    Warning("\"%s\": cached version doesn't exist\n", pFileName);
    return MATERIAL_PREVIEW_IMAGE_BAD;
  }

  IVTFTexture *pVTFTexture = CreateVTFTexture();
  if (!pVTFTexture->Unserialize(buf, true)) {
    Warning("Error reading material \"%s\"\n", pFileName);
    DestroyVTFTexture(pVTFTexture);
    return MATERIAL_PREVIEW_IMAGE_BAD;
  }

  *width = pVTFTexture->Width();
  *height = pVTFTexture->Height();
  *imageFormat = pVTFTexture->Format();
  *isTranslucent = (pVTFTexture->Flags() & (TEXTUREFLAGS_ONEBITALPHA |
                                            TEXTUREFLAGS_EIGHTBITALPHA)) != 0;
  DestroyVTFTexture(pVTFTexture);
  return MATERIAL_PREVIEW_IMAGE_OK;
}

PreviewImageRetVal_t CMaterial::GetPreviewImage(unsigned char *pData, int width,
                                                int height,
                                                ImageFormat imageFormat) const {
  int nHeaderSize, nImageOffset, nImageSize;
  CUtlBuffer buf;

  char const *pFileName = GetPreviewImageFileName();
  if (!pFileName) return MATERIAL_NO_PREVIEW_IMAGE;

  IVTFTexture *pVTFTexture = CreateVTFTexture();
  FileHandle_t fileHandle = g_pFullFileSystem->Open(pFileName, "rb");
  if (!fileHandle) {
    Warning("\"%s\": cached version doesn't exist\n", pFileName);
    goto fail;
  }

  nHeaderSize = VTFFileHeaderSize(VTF_MAJOR_VERSION);
  buf.EnsureCapacity(nHeaderSize);

  // read the header first.. it's faster!!
  // GCC won't let this be initialized right away
  int nBytesRead;
  nBytesRead = g_pFullFileSystem->Read(buf.Base(), nHeaderSize, fileHandle);
  buf.SeekPut(CUtlBuffer::SEEK_HEAD, nBytesRead);

  // Unserialize the header
  if (!pVTFTexture->Unserialize(buf, true)) {
    Warning("Error reading material \"%s\"\n", pFileName);
    goto fail;
  }

  // TODO(d.rattman): Make sure the preview image size requested is the same
  // size as mip level 0 of the texture
  Assert((width == pVTFTexture->Width()) && (height == pVTFTexture->Height()));

  // Determine where in the file to start reading (frame 0, face 0, mip 0)
  pVTFTexture->ImageFileInfo(0, 0, 0, &nImageOffset, &nImageSize);

  // Prep the utlbuffer for reading
  buf.EnsureCapacity(nImageSize);
  buf.SeekPut(CUtlBuffer::SEEK_HEAD, 0);

  // Read in the bits at the specified location
  g_pFullFileSystem->Seek(fileHandle, nImageOffset, FILESYSTEM_SEEK_HEAD);
  g_pFullFileSystem->Read(buf.Base(), nImageSize, fileHandle);
  g_pFullFileSystem->Close(fileHandle);

  // Convert from the format read in to the requested format
  ImageLoader::ConvertImageFormat((unsigned char *)buf.Base(),
                                  pVTFTexture->Format(), pData, imageFormat,
                                  width, height);

  DestroyVTFTexture(pVTFTexture);
  return MATERIAL_PREVIEW_IMAGE_OK;

fail:
  if (fileHandle) g_pFullFileSystem->Close(fileHandle);

  int nSize = ImageLoader::GetMemRequired(width, height, 1, imageFormat, false);
  memset(pData, 0xff, nSize);

  DestroyVTFTexture(pVTFTexture);

  return MATERIAL_PREVIEW_IMAGE_BAD;
}

// Material variables
IMaterialVar *CMaterial::FindVar(char const *pVarName, bool *pFound,
                                 bool complain) {
  PrecacheVars();

  // TODO(d.rattman): Could look for flags here too...
  MaterialVarSym_t sym = IMaterialVar::FindSymbol(pVarName);
  if (sym != UTL_INVAL_SYMBOL) {
    for (int i = m_VarCount; --i >= 0;) {
      if (m_pShaderParams[i]->GetNameAsSymbol() == sym) {
        if (pFound) *pFound = true;
        return m_pShaderParams[i];
      }
    }
  }

  if (pFound) *pFound = false;

  if (complain) {
    constexpr usize kMaxComplainCount{100};
    static usize complain_count{0};
    static bool overflow_complain_count{false};

    if (complain_count < kMaxComplainCount) {
      Warning("No such variable \"%s\" for material \"%s\"\n", pVarName,
              GetName());
      ++complain_count;
    } else {
      if (!overflow_complain_count) {
        Warning(
            "Too much missed material vars (> %zu), skip rest warnings about\n",
            complain_count);
        overflow_complain_count = true;
      }
    }
  }

  return GetDummyVariable();
}

struct tokencache_t {
  unsigned short symbol;
  unsigned char varIndex;
  unsigned char cached;
};

IMaterialVar *CMaterial::FindVarFast(char const *pVarName,
                                     unsigned int *pCacheData) {
  tokencache_t *token_cache = reinterpret_cast<tokencache_t *>(pCacheData);
  PrecacheVars();

  if (token_cache->cached) {
    if (token_cache->varIndex < m_VarCount &&
        m_pShaderParams[token_cache->varIndex]->GetNameAsSymbol() ==
            token_cache->symbol)
      return m_pShaderParams[token_cache->varIndex];

    // TODO(d.rattman): Could look for flags here too...
    if (!IMaterialVar::SymbolMatches(pVarName, token_cache->symbol)) {
      token_cache->symbol = IMaterialVar::FindSymbol(pVarName);
    }
  } else {
    token_cache->cached = 1;
    token_cache->symbol = IMaterialVar::FindSymbol(pVarName);
  }

  if (token_cache->symbol != UTL_INVAL_SYMBOL) {
    for (int i = m_VarCount; --i >= 0;) {
      if (m_pShaderParams[i]->GetNameAsSymbol() == token_cache->symbol) {
        token_cache->varIndex = i;
        return m_pShaderParams[i];
      }
    }
  }

  return nullptr;
}

// Lovely material properties
void CMaterial::GetReflectivity(Vector &reflect) {
  Precache();

  reflect = m_Reflectivity;
}

bool CMaterial::GetPropertyFlag(MaterialPropertyTypes_t type) {
  Precache();

  if (!IsValidRenderState()) return false;

  switch (type) {
    case MATERIAL_PROPERTY_NEEDS_LIGHTMAP:
      return IsUsingLightmap();

    case MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS:
      return IsUsingDiffuseBumpedLighting();
  }

  return false;
}

// Is the material visible from both sides?
bool CMaterial::IsTwoSided() {
  PrecacheVars();
  return GetMaterialVarFlag(MATERIAL_VAR_NOCULL);
}

// Are we translucent?
bool CMaterial::IsTranslucent() {
  Precache();
  return IsTranslucentInternal(
      m_pShaderParams ? m_pShaderParams[ALPHA]->GetFloatValue() : 0.0);
}

bool CMaterial::IsTranslucentInternal(float fAlphaModulation) const {
  // I have to check for alpha modulation here because it isn't factored into
  // the shader's notion of whether or not it's transparent.
  return m_pShader && IsValidRenderState() &&
             ::IsTranslucent(&m_ShaderRenderState) ||
         (fAlphaModulation < 1.0f) || m_pShader->IsTranslucent(m_pShaderParams);
}

// Are we alphatested?
bool CMaterial::IsAlphaTested() {
  Precache();
  return m_pShader && IsValidRenderState() &&
             ::IsAlphaTested(&m_ShaderRenderState) ||
         GetMaterialVarFlag(MATERIAL_VAR_ALPHATEST);
}

// Are we vertex lit?
bool CMaterial::IsVertexLit() {
  Precache();
  return IsValidRenderState() &&
         (GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_VERTEX_LIT) != 0;
}

// Is the shader a sprite card shader?
bool CMaterial::IsSpriteCard() {
  Precache();
  return IsValidRenderState() &&
         (GetMaterialVarFlags2() & MATERIAL_VAR2_IS_SPRITECARD) != 0;
}

void CMaterial::CallBindProxy(void *proxyData) {
  CMatCallQueue *call_queue = MaterialSystem()->GetRenderCallQueue();
  bool is_threaded = call_queue != nullptr;

  switch (g_config.proxiesTestMode) {
    case 0: {
      // Make sure we call the proxies in the order in which they show up
      // in the .vmt file
      if (m_ProxyCount) {
        if (is_threaded) {
          EnableThreadedMaterialVarAccess(true, m_pShaderParams, m_VarCount);
        }
        for (int i = 0; i < m_ProxyCount; ++i) {
          m_ppProxies[i]->OnBind(proxyData);
        }
        if (is_threaded) {
          EnableThreadedMaterialVarAccess(false, m_pShaderParams, m_VarCount);
        }
      }
    } break;

    case 2:
      // alpha mod all....
      {
        float value =
            (sin(2.0f * M_PI * Plat_FloatTime() / 10.0f) * 0.5f) + 0.5f;
        m_pShaderParams[ALPHA]->SetFloatValue(value);
      }
      break;

    case 3:
      // color mod all...
      {
        float value =
            (sin(2.0f * M_PI * Plat_FloatTime() / 10.0f) * 0.5f) + 0.5f;
        m_pShaderParams[COLOR]->SetVecValue(value, 1.0f, 1.0f);
      }
      break;
  }
}

bool CMaterial::HasProxy() const { return (m_ProxyCount > 0); }

// Main draw method
void CMaterial::DrawMesh(VertexCompressionType_t vertexCompression) {
  if (m_pShader) {
#ifndef NDEBUG
    if (GetMaterialVarFlags() & MATERIAL_VAR_DEBUG) {
      // Putcher breakpoint here to catch the rendering of a material
      // marked for debugging ($debug = 1 in a .vmt file) dynamic state version
      [[maybe_unused]] int x = 0;
    }
#endif
    if ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) == 0) {
      [[maybe_unused]] const char *pName = m_pShader->GetName();
      ShaderSystem()->DrawElements(m_pShader, m_pShaderParams,
                                   &m_ShaderRenderState, vertexCompression,
                                   m_ChangeID ^ g_nDebugVarsSignature);
    }
  } else {
    Warning("CMaterial::DrawElements: No bound shader\n");
  }
}

IShader *CMaterial::GetShader() const { return m_pShader; }

IMaterialVar *CMaterial::GetShaderParam(int id) { return m_pShaderParams[id]; }

// Adds a material variable to the material
void CMaterial::AddMaterialVar(IMaterialVar *pMaterialVar) {
  ++m_VarCount;
  m_pShaderParams = (IMaterialVar **)realloc(
      m_pShaderParams, m_VarCount * sizeof(IMaterialVar *));
  m_pShaderParams[m_VarCount - 1] = pMaterialVar;
}

bool CMaterial::IsErrorMaterial() const {
  extern IMaterialInternal *g_pErrorMaterial;
  const IMaterialInternal *pThis = this;
  return g_pErrorMaterial == pThis;
}

void CMaterial::FindRepresentativeTexture() {
  Precache();

  // First try to find the base texture...
  bool is_found;
  IMaterialVar *texture_var = FindVar("$baseTexture", &is_found, false);
  if (is_found && texture_var->GetType() == MATERIAL_VAR_TYPE_TEXTURE) {
    auto *texture = (ITextureInternal *)texture_var->GetTextureValue();
    if (texture) texture->GetReflectivity(m_Reflectivity);
  }

  if (!is_found || texture_var->GetType() != MATERIAL_VAR_TYPE_TEXTURE) {
    // Try the env map mask if the base texture doesn't work...
    // this is needed for specular decals
    texture_var = FindVar("$envmapmask", &is_found, false);

    if (!is_found || texture_var->GetType() != MATERIAL_VAR_TYPE_TEXTURE) {
      // Try the bumpmap
      texture_var = FindVar("$bumpmap", &is_found, false);

      if (!is_found || texture_var->GetType() != MATERIAL_VAR_TYPE_TEXTURE) {
        texture_var = FindVar("$dudvmap", &is_found, false);

        if (!is_found || texture_var->GetType() != MATERIAL_VAR_TYPE_TEXTURE) {
          texture_var = FindVar("$normalmap", &is_found, false);

          if (!is_found ||
              texture_var->GetType() != MATERIAL_VAR_TYPE_TEXTURE) {
            /*Warning("Can't find representative texture for material \"%s\"\n",
                    GetName());*/
            m_representativeTexture = TextureManager()->ErrorTexture();
            return;
          }
        }
      }
    }
  }

  m_representativeTexture =
      static_cast<ITextureInternal *>(texture_var->GetTextureValue());

  if (m_representativeTexture) {
    m_representativeTexture->Precache();
  } else {
    m_representativeTexture = TextureManager()->ErrorTexture();
    Assert(m_representativeTexture);
  }
}

void CMaterial::GetLowResColorSample(float s, float t, float *color) const {
  if (!m_representativeTexture) return;

  m_representativeTexture->GetLowResColorSample(s, t, color);
}

// Lightmap-related methods

void CMaterial::SetMinLightmapPageID(int pageID) {
  m_minLightmapPageID = pageID;
}

void CMaterial::SetMaxLightmapPageID(int pageID) {
  m_maxLightmapPageID = pageID;
}

int CMaterial::GetMinLightmapPageID() const { return m_minLightmapPageID; }

int CMaterial::GetMaxLightmapPageID() const { return m_maxLightmapPageID; }

void CMaterial::SetNeedsWhiteLightmap(bool val) {
  if (val)
    m_Flags |= MATERIAL_NEEDS_WHITE_LIGHTMAP;
  else
    m_Flags &= ~MATERIAL_NEEDS_WHITE_LIGHTMAP;
}

bool CMaterial::GetNeedsWhiteLightmap() const {
  return (m_Flags & MATERIAL_NEEDS_WHITE_LIGHTMAP) != 0;
}

void CMaterial::MarkAsPreloaded(bool bSet) {
  if (bSet) {
    m_Flags |= MATERIAL_IS_PRELOADED;
  } else {
    m_Flags &= ~MATERIAL_IS_PRELOADED;
  }
}

bool CMaterial::IsPreloaded() const {
  return (m_Flags & MATERIAL_IS_PRELOADED) != 0;
}

void CMaterial::ArtificialAddRef() {
  if (m_Flags & MATERIAL_ARTIFICIAL_REFCOUNT) {
    // already done
    return;
  }

  m_Flags |= MATERIAL_ARTIFICIAL_REFCOUNT;
  ++m_RefCount;
}

void CMaterial::ArtificialRelease() {
  if (!(m_Flags & MATERIAL_ARTIFICIAL_REFCOUNT)) {
    return;
  }

  m_Flags &= ~MATERIAL_ARTIFICIAL_REFCOUNT;
  --m_RefCount;
}

// Return the shader params
IMaterialVar **CMaterial::GetShaderParams() { return m_pShaderParams; }

int CMaterial::ShaderParamCount() const { return m_VarCount; }

// VMT parser
void InsertKeyValues(KeyValues &dst, KeyValues &src, bool bCheckForExistence) {
  KeyValues *pSrcVar = src.GetFirstSubKey();
  while (pSrcVar) {
    if (!bCheckForExistence || dst.FindKey(pSrcVar->GetName())) {
      switch (pSrcVar->GetDataType()) {
        case KeyValues::TYPE_STRING:
          dst.SetString(pSrcVar->GetName(), pSrcVar->GetString());
          break;
        case KeyValues::TYPE_INT:
          dst.SetInt(pSrcVar->GetName(), pSrcVar->GetInt());
          break;
        case KeyValues::TYPE_FLOAT:
          dst.SetFloat(pSrcVar->GetName(), pSrcVar->GetFloat());
          break;
        case KeyValues::TYPE_PTR:
          dst.SetPtr(pSrcVar->GetName(), pSrcVar->GetPtr());
          break;
      }
    }
    pSrcVar = pSrcVar->GetNextKey();
  }

  if (bCheckForExistence) {
    for (KeyValues *pScan = dst.GetFirstTrueSubKey(); pScan;
         pScan = pScan->GetNextTrueSubKey()) {
      KeyValues *pTmp = src.FindKey(pScan->GetName());
      if (!pTmp) continue;
      // make sure that this is a subkey.
      if (pTmp->GetDataType() != KeyValues::TYPE_NONE) continue;
      InsertKeyValues(*pScan, *pTmp, bCheckForExistence);
    }
  }
}

void WriteKeyValuesToFile(const char *pFileName, KeyValues &keyValues) {
  keyValues.SaveToFile(g_pFullFileSystem, pFileName);
}

void ApplyPatchKeyValues(KeyValues &keyValues, KeyValues &patchKeyValues) {
  // DONOTCHECKIN: add comment that again, this is broked (and the assert on
  // insert/replace is invalid too - but whatever!)

  KeyValues *pInsertSection = patchKeyValues.FindKey("insert");
  KeyValues *pReplaceSection = patchKeyValues.FindKey("replace");
  // We expect patch files to do one or the other, not both:
  Assert((pInsertSection == nullptr) || (pReplaceSection == nullptr));

  if (pInsertSection) {
    InsertKeyValues(keyValues, *pInsertSection, false);
  }

  if (pReplaceSection) {
    InsertKeyValues(keyValues, *pReplaceSection, true);
  }

  // Could add other commands here, like "delete", "rename", etc.
}

void ExpandPatchFile(KeyValues &keyValues, KeyValues &patchKeyValues,
                     const char *pPathID,
                     CUtlVector<FileNameHandle_t> *pIncludes) {
  if (pIncludes) pIncludes->Purge();

  // Recurse down through all patch files:
  int count = 0;
  while (count < 10 && _stricmp(keyValues.GetName(), "patch") == 0) {
    //		WriteKeyValuesToFile( "patch.txt", keyValues );

    // Accumulate the new patch keys from this file
    ApplyPatchKeyValues(keyValues, patchKeyValues);
    patchKeyValues = keyValues;

    // Load the included file
    const char *pIncludeFileName = keyValues.GetString("include");
    if (pIncludeFileName) {
      KeyValues *includeKeyValues = new KeyValues("vmt");
      bool success = includeKeyValues->LoadFromFile(g_pFullFileSystem,
                                                    pIncludeFileName, pPathID);
      if (success) {
        // Remember that we included this file for the pure server stuff.
        if (pIncludes)
          pIncludes->AddToTail(
              g_pFullFileSystem->FindOrAddFileName(pIncludeFileName));
      } else {
        includeKeyValues->deleteThis();
        Warning("Failed to load $include VMT file (%s)\n", pIncludeFileName);
        Assert(success);
        return;  //-V773
      }
      keyValues = *includeKeyValues;
      includeKeyValues->deleteThis();
    } else {
      // A patch file without an $include key? Not good...
      Warning("VMT patch file has no $include key - invalid!\n");
      Assert(pIncludeFileName);
      break;
    }

    count++;
  }
  if (count >= 10) {
    Warning("Infinite recursion in patch file?\n");
  }

  // KeyValues is now a real (non-patch) VMT, so apply the patches and return
  ApplyPatchKeyValues(keyValues, patchKeyValues);
}

bool LoadVMTFile(KeyValues &vmtKeyValues, KeyValues &patchKeyValues,
                 const char *pMaterialName, bool bAbsolutePath,
                 CUtlVector<FileNameHandle_t> *pIncludes) {
  char pFileName[SOURCE_MAX_PATH];
  const char *pPathID = "GAME";
  if (!bAbsolutePath) {
    Q_snprintf(pFileName, sizeof(pFileName), "materials/%s.vmt", pMaterialName);
  } else {
    Q_snprintf(pFileName, sizeof(pFileName), "%s.vmt", pMaterialName);
    if (pMaterialName[0] == '/' && pMaterialName[1] == '/' &&
        pMaterialName[2] != '/') {
      // UNC, do full search
      pPathID = nullptr;
    }
  }

  if (!vmtKeyValues.LoadFromFile(g_pFullFileSystem, pFileName, pPathID)) {
    return false;
  }
  ExpandPatchFile(vmtKeyValues, patchKeyValues, pPathID, pIncludes);

  return true;
}

int CMaterial::GetNumPasses() {
  Precache();
  //	int mod = m_ShaderRenderState.m_Modulation;
  int mod = 0;
  return m_ShaderRenderState.m_pSnapshots[mod].m_nPassCount;
}

int CMaterial::GetTextureMemoryBytes() {
  Precache();
  int bytes = 0;
  int i;
  for (i = 0; i < m_VarCount; i++) {
    IMaterialVar *pVar = m_pShaderParams[i];
    if (pVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE) {
      ITexture *pTexture = pVar->GetTextureValue();
      if (pTexture && pTexture != (ITexture *)0xffffffff) {
        bytes += pTexture->GetApproximateVidMemBytes();
      }
    }
  }
  return bytes;
}

void CMaterial::SetUseFixedFunctionBakedLighting(bool bEnable) {
  SetMaterialVarFlags2(MATERIAL_VAR2_USE_FIXED_FUNCTION_BAKED_LIGHTING,
                       bEnable);
}

bool CMaterial::NeedsFixedFunctionFlashlight() const {
  return (GetMaterialVarFlags2() &
          MATERIAL_VAR2_NEEDS_FIXED_FUNCTION_FLASHLIGHT) &&
         MaterialSystem()->InFlashlightMode();
}

bool CMaterial::IsUsingVertexID() const {
  return (GetMaterialVarFlags2() & MATERIAL_VAR2_USES_VERTEXID) != 0;
}

void CMaterial::DeleteIfUnreferenced() {
  if (m_RefCount > 0) return;
  IMaterialVar::DeleteUnreferencedTextures(true);
  MaterialSystem()->RemoveMaterial(this);
  IMaterialVar::DeleteUnreferencedTextures(false);
}
