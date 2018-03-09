// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#define NOMINMAX

#include <d3d10.h>
#undef GetCommandLine

#include "inputlayoutdx10.h"
#include "materialsystem/imesh.h"
#include "shaderdevicedx10.h"

// NOTE: This has to be the last file included!
#include "tier0/include/memdbgon.h"

// Standard input layouts

static const DXGI_FORMAT s_pSizeLookup[] = {
    DXGI_FORMAT_UNKNOWN,             // Should be unused...
    DXGI_FORMAT_R32_FLOAT,           // D3DDECLTYPE_FLOAT1
    DXGI_FORMAT_R32G32_FLOAT,        // D3DDECLTYPE_FLOAT2,
    DXGI_FORMAT_R32G32B32_FLOAT,     // D3DDECLTYPE_FLOAT3,
    DXGI_FORMAT_R32G32B32A32_FLOAT,  // D3DDECLTYPE_FLOAT4
};

struct FieldInfo_t {
  const char *m_pSemanticString;
  unsigned int m_nSemanticIndex;
  uint64_t m_nFormatMask;
  int m_nFieldSize;
};

static FieldInfo_t s_pFieldInfo[] = {
    {"POSITION", 0, VERTEX_POSITION, sizeof(float) * 3},
    {"BLENDWEIGHT", 0, VERTEX_BONE_WEIGHT_MASK, 0},
    {"BLENDINDICES", 0, VERTEX_BONE_INDEX, 4},
    {"NORMAL", 0, VERTEX_NORMAL, sizeof(float) * 3},
    {"COLOR", 0, VERTEX_COLOR, 4},
    {"SPECULAR", 0, VERTEX_SPECULAR, 4},
    {"TEXCOORD", 0, VERTEX_TEXCOORD_MASK(0), 0},
    {"TEXCOORD", 1, VERTEX_TEXCOORD_MASK(1), 0},
    {"TEXCOORD", 2, VERTEX_TEXCOORD_MASK(2), 0},
    {"TEXCOORD", 3, VERTEX_TEXCOORD_MASK(3), 0},
    {"TEXCOORD", 4, VERTEX_TEXCOORD_MASK(4), 0},
    {"TEXCOORD", 5, VERTEX_TEXCOORD_MASK(5), 0},
    {"TEXCOORD", 6, VERTEX_TEXCOORD_MASK(6), 0},
    {"TEXCOORD", 7, VERTEX_TEXCOORD_MASK(7), 0},
    {"TANGENT", 0, VERTEX_TANGENT_S, sizeof(float) * 3},
    {"BINORMAL", 0, VERTEX_TANGENT_T, sizeof(float) * 3},
    {"USERDATA", 0, USER_DATA_SIZE_MASK, 0},
    {NULL, 0, 0},
};

static D3D10_INPUT_ELEMENT_DESC s_pVertexDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA,
     0},
    {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D10_INPUT_PER_VERTEX_DATA,
     0},
    {"SPECULAR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 1, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 2, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 3, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 4, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 5, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 6, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 7, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D10_INPUT_PER_VERTEX_DATA, 0},
    {"USERDATA", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
};

static D3D10_INPUT_ELEMENT_DESC s_pFallbackVertexDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 0,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32_FLOAT, 15, 12,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 20,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 24,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 36,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"SPECULAR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 40,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 15, 44,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 15, 52,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 15, 60,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 15, 68,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 15, 76,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 15, 84,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 15, 92,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 15, 100,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 108,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 120,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"USERDATA", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 15, 132,
     D3D10_INPUT_PER_INSTANCE_DATA, UINT_MAX},
};

#ifndef NDEBUG
// Computes the required input desc based on the vertex format.
static void PrintInputDesc(int nCount, const D3D10_INPUT_ELEMENT_DESC *pDecl) {
  for (int i = 0; i < nCount; i++) {
    Msg("%s (%d): Stream: %d, Offset: %d, Instanced? %c\n",
        pDecl[i].SemanticName, pDecl[i].SemanticIndex, (int)pDecl[i].InputSlot,
        (int)pDecl[i].AlignedByteOffset,
        pDecl[i].InputSlotClass == D3D10_INPUT_PER_VERTEX_DATA ? 'n' : 'y');
  }
}
#endif

// Checks to see if a shader requires a particular field.
static bool CheckShaderSignatureExpectations(
    ID3D10ShaderReflection *pReflection, const char *pSemantic,
    unsigned int nSemanticIndex) {
  Assert(pSemantic);
  Assert(pReflection);

  D3D10_SHADER_DESC shader_desc;
  HRESULT hr = pReflection->GetDesc(&shader_desc);
  Assert(SUCCEEDED(hr));

  D3D10_SIGNATURE_PARAMETER_DESC paramDesc;
  for (unsigned int k = 0; k < shader_desc.InputParameters; k++) {
    hr = pReflection->GetInputParameterDesc(k, &paramDesc);
    Assert(SUCCEEDED(hr));

    if ((nSemanticIndex == paramDesc.SemanticIndex) &&
        !Q_stricmp(pSemantic, paramDesc.SemanticName))
      return true;
  }

  return false;
}

// Computes the required input desc based on the vertex format

static unsigned int ComputeInputDesc(VertexFormat_t fmt,
                                     D3D10_INPUT_ELEMENT_DESC *pDecl,
                                     ID3D10ShaderReflection *pReflection) {
  unsigned int nCount = 0;
  int nOffset = 0;

  // Fix up the global table so we don't need special-case code
  int nBoneCount = NumBoneWeights(fmt);
  s_pFieldInfo[1].m_nFieldSize = sizeof(float) * nBoneCount;
  s_pVertexDesc[1].Format = s_pSizeLookup[nBoneCount];

  int nUserDataSize = UserDataSize(fmt);
  s_pFieldInfo[16].m_nFieldSize = sizeof(float) * nUserDataSize;
  s_pVertexDesc[16].Format = s_pSizeLookup[nUserDataSize];

  // NOTE: Fix s_pFieldInfo, s_pVertexDesc, s_pFallbackVertexDesc if you add
  // more fields As well as the fallback stream (stream #15)
  static_assert(VERTEX_MAX_TEXTURE_COORDINATES == 8);
  for (int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i) {
    int nTexCoordCount = TexCoordSize(i, fmt);
    s_pFieldInfo[6 + i].m_nFieldSize = sizeof(float) * nTexCoordCount;
    s_pVertexDesc[6 + i].Format = s_pSizeLookup[nTexCoordCount];
  }

  // TODO(d.rattman): Change this loop so CheckShaderSignatureExpectations is called once!
  for (int i = 0; s_pFieldInfo[i].m_pSemanticString; ++i) {
    if (fmt & s_pFieldInfo[i].m_nFormatMask) {
      memcpy(&pDecl[nCount], &s_pVertexDesc[i],
             sizeof(D3D10_INPUT_ELEMENT_DESC));
      pDecl[nCount].AlignedByteOffset = nOffset;
      nOffset += s_pFieldInfo[i].m_nFieldSize;
      ++nCount;
    } else if (CheckShaderSignatureExpectations(
                   pReflection, s_pFieldInfo[i].m_pSemanticString,
                   s_pFieldInfo[i].m_nSemanticIndex)) {
      memcpy(&pDecl[nCount], &s_pFallbackVertexDesc[i],
             sizeof(D3D10_INPUT_ELEMENT_DESC));
      ++nCount;
    }
  }

  // For debugging only...
#ifndef NDEBUG
  PrintInputDesc(nCount, pDecl);
#endif

  return nCount;
}

// Gets the input layout associated with a vertex format.
ID3D10InputLayout *CreateInputLayout(VertexFormat_t fmt,
                                     ID3D10ShaderReflection *pReflection,
                                     const void *pByteCode,
                                     size_t nByteCodeLen) {
  D3D10_INPUT_ELEMENT_DESC pDecl[32];
  unsigned int nDeclCount = ComputeInputDesc(fmt, pDecl, pReflection);

  ID3D10InputLayout *pInputLayout;
  HRESULT hr = D3D10Device()->CreateInputLayout(pDecl, nDeclCount, pByteCode,
                                                nByteCodeLen, &pInputLayout);
  if (SUCCEEDED(hr)) return pInputLayout;

  Warning("CreateInputLayout::Unable to create input layout for format %X!\n",
          fmt);
  return nullptr;
}
