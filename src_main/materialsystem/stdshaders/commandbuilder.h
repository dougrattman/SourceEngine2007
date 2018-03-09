//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
// $NoKeywords: $
// Utility class for building command buffers into memory
//===========================================================================//

#ifndef COMMANDBUILDER_H
#define COMMANDBUILDER_H

#ifndef COMMANDBUFFER_H
#include "shaderapi/commandbuffer.h"
#endif

#include "basevsshader.h"

template <int N>
class CFixedCommandStorageBuffer {
 public:
  uint8_t m_Data[N];

  uint8_t *m_pDataOut;
#ifndef NDEBUG
  size_t m_nNumBytesRemaining;
#endif

  SOURCE_FORCEINLINE CFixedCommandStorageBuffer(void) {
    m_pDataOut = m_Data;
#ifndef NDEBUG
    m_nNumBytesRemaining = N;
#endif
  }

  SOURCE_FORCEINLINE void EnsureCapacity(size_t sz) {
    Assert(m_nNumBytesRemaining >= sz);
  }

  template <class T>
  SOURCE_FORCEINLINE void Put(T const &nValue) {
    EnsureCapacity(sizeof(T));
    *(reinterpret_cast<T *>(m_pDataOut)) = nValue;
    m_pDataOut += sizeof(nValue);
#ifndef NDEBUG
    m_nNumBytesRemaining -= sizeof(nValue);
#endif
  }

  SOURCE_FORCEINLINE void PutInt(int nValue) { Put(nValue); }

  SOURCE_FORCEINLINE void PutFloat(float nValue) { Put(nValue); }

  SOURCE_FORCEINLINE void PutPtr(void *pPtr) { Put(pPtr); }

  SOURCE_FORCEINLINE void PutMemory(const void *pMemory, size_t nBytes) {
    EnsureCapacity(nBytes);
    memcpy(m_pDataOut, pMemory, nBytes);
    m_pDataOut += nBytes;
  }

  SOURCE_FORCEINLINE uint8_t *Base(void) { return m_Data; }

  SOURCE_FORCEINLINE void Reset(void) {
    m_pDataOut = m_Data;
#ifndef NDEBUG
    m_nNumBytesRemaining = N;
#endif
  }

  SOURCE_FORCEINLINE size_t Size(void) const { return m_pDataOut - m_Data; }
};

template <class S>
class CCommandBufferBuilder {
 public:
  S m_Storage;

  SOURCE_FORCEINLINE void End(void) { m_Storage.PutInt(CBCMD_END); }

  SOURCE_FORCEINLINE IMaterialVar *Param(int nVar) const {
    return CBaseShader::s_ppParams[nVar];
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstants(int nFirstConstant, int nConstants) {
    m_Storage.PutInt(CBCMD_SET_PIXEL_SHADER_FLOAT_CONST);
    m_Storage.PutInt(nFirstConstant);
    m_Storage.PutInt(nConstants);
  }

  SOURCE_FORCEINLINE void OutputConstantData(float const *pSrcData) {
    m_Storage.PutFloat(pSrcData[0]);
    m_Storage.PutFloat(pSrcData[1]);
    m_Storage.PutFloat(pSrcData[2]);
    m_Storage.PutFloat(pSrcData[3]);
  }

  SOURCE_FORCEINLINE void OutputConstantData4(float flVal0, float flVal1, float flVal2,
                                       float flVal3) {
    m_Storage.PutFloat(flVal0);
    m_Storage.PutFloat(flVal1);
    m_Storage.PutFloat(flVal2);
    m_Storage.PutFloat(flVal3);
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstant(int nFirstConstant,
                                          float const *pSrcData,
                                          int nNumConstantsToSet) {
    SetPixelShaderConstants(nFirstConstant, nNumConstantsToSet);
    m_Storage.PutMemory(pSrcData, 4 * sizeof(float) * nNumConstantsToSet);
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstant(int nFirstConstant, int nVar) {
    SetPixelShaderConstant(nFirstConstant, Param(nVar)->GetVecValue());
  }

  void SetPixelShaderConstantGammaToLinear(int pixelReg, int constantVar) {
    float val[4];
    Param(constantVar)->GetVecValue(val, 3);
    val[0] = val[0] > 1.0f ? val[0] : GammaToLinear(val[0]);
    val[1] = val[1] > 1.0f ? val[1] : GammaToLinear(val[1]);
    val[2] = val[2] > 1.0f ? val[2] : GammaToLinear(val[2]);
    val[3] = 1.0;
    SetPixelShaderConstant(pixelReg, val);
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstant(int nFirstConstant,
                                          float const *pSrcData) {
    SetPixelShaderConstants(nFirstConstant, 1);
    OutputConstantData(pSrcData);
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstant4(int nFirstConstant, float flVal0,
                                           float flVal1, float flVal2,
                                           float flVal3) {
    SetPixelShaderConstants(nFirstConstant, 1);
    OutputConstantData4(flVal0, flVal1, flVal2, flVal3);
  }

  SOURCE_FORCEINLINE void SetPixelShaderConstant_W(int pixelReg, int constantVar,
                                            float fWValue) {
    if (constantVar != -1) {
      float val[3];
      Param(constantVar)->GetVecValue(val, 3);
      SetPixelShaderConstant4(pixelReg, val[0], val[1], val[2], fWValue);
    }
  }

  SOURCE_FORCEINLINE void SetVertexShaderConstant(int nFirstConstant,
                                           float const *pSrcData) {
    m_Storage.PutInt(CBCMD_SET_VERTEX_SHADER_FLOAT_CONST);
    m_Storage.PutInt(nFirstConstant);
    m_Storage.PutInt(1);
    OutputConstantData(pSrcData);
  }

  SOURCE_FORCEINLINE void SetVertexShaderConstant(int nFirstConstant,
                                           float const *pSrcData, int nConsts) {
    m_Storage.PutInt(CBCMD_SET_VERTEX_SHADER_FLOAT_CONST);
    m_Storage.PutInt(nFirstConstant);
    m_Storage.PutInt(nConsts);
    m_Storage.PutMemory(pSrcData, 4 * nConsts * sizeof(float));
  }

  SOURCE_FORCEINLINE void SetVertexShaderConstant4(int nFirstConstant, float flVal0,
                                            float flVal1, float flVal2,
                                            float flVal3) {
    m_Storage.PutInt(CBCMD_SET_VERTEX_SHADER_FLOAT_CONST);
    m_Storage.PutInt(nFirstConstant);
    m_Storage.PutInt(1);
    m_Storage.PutFloat(flVal0);
    m_Storage.PutFloat(flVal1);
    m_Storage.PutFloat(flVal2);
    m_Storage.PutFloat(flVal3);
  }

  void SetVertexShaderTextureTransform(int vertexReg, int transformVar) {
    Vector4D transformation[2];
    IMaterialVar *pTransformationVar = Param(transformVar);
    if (pTransformationVar &&
        (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)) {
      const VMatrix &mat = pTransformationVar->GetMatrixValue();
      transformation[0].Init(mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
      transformation[1].Init(mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
    } else {
      transformation[0].Init(1.0f, 0.0f, 0.0f, 0.0f);
      transformation[1].Init(0.0f, 1.0f, 0.0f, 0.0f);
    }
    SetVertexShaderConstant(vertexReg, transformation[0].Base(), 2);
  }

  void SetVertexShaderTextureScaledTransform(int vertexReg, int transformVar,
                                             int scaleVar) {
    Vector4D transformation[2];
    IMaterialVar *pTransformationVar = Param(transformVar);
    if (pTransformationVar &&
        (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)) {
      const VMatrix &mat = pTransformationVar->GetMatrixValue();
      transformation[0].Init(mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
      transformation[1].Init(mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
    } else {
      transformation[0].Init(1.0f, 0.0f, 0.0f, 0.0f);
      transformation[1].Init(0.0f, 1.0f, 0.0f, 0.0f);
    }

    Vector2D scale(1, 1);
    IMaterialVar *pScaleVar = Param(scaleVar);
    if (pScaleVar) {
      if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
        pScaleVar->GetVecValue(scale.Base(), 2);
      else if (pScaleVar->IsDefined())
        scale[0] = scale[1] = pScaleVar->GetFloatValue();
    }

    // Apply the scaling
    transformation[0][0] *= scale[0];
    transformation[0][1] *= scale[1];
    transformation[1][0] *= scale[0];
    transformation[1][1] *= scale[1];
    transformation[0][3] *= scale[0];
    transformation[1][3] *= scale[1];
    SetVertexShaderConstant(vertexReg, transformation[0].Base(), 2);
  }

  SOURCE_FORCEINLINE void SetEnvMapTintPixelShaderDynamicState(int pixelReg,
                                                        int tintVar) {
    if (g_pConfig->bShowSpecular && mat_fullbright.GetInt() != 2) {
      SetPixelShaderConstant(pixelReg, Param(tintVar)->GetVecValue());
    } else {
      SetPixelShaderConstant4(pixelReg, 0.0, 0.0, 0.0, 0.0);
    }
  }

  SOURCE_FORCEINLINE void SetEnvMapTintPixelShaderDynamicStateGammaToLinear(
      int pixelReg, int tintVar) {
    if (g_pConfig->bShowSpecular && mat_fullbright.GetInt() != 2) {
      float color[4];
      color[3] = 1.0;
      Param(tintVar)->GetLinearVecValue(color, 3);
      SetPixelShaderConstant(pixelReg, color);
    } else {
      SetPixelShaderConstant4(pixelReg, 0.0, 0.0, 0.0, 0.0);
    }
  }

  SOURCE_FORCEINLINE void StoreEyePosInPixelShaderConstant(int nConst) {
    m_Storage.PutInt(CBCMD_STORE_EYE_POS_IN_PSCONST);
    m_Storage.PutInt(nConst);
  }

  SOURCE_FORCEINLINE void CommitPixelShaderLighting(int nConst) {
    m_Storage.PutInt(CBCMD_COMMITPIXELSHADERLIGHTING);
    m_Storage.PutInt(nConst);
  }

  SOURCE_FORCEINLINE void SetPixelShaderStateAmbientLightCube(int nConst) {
    m_Storage.PutInt(CBCMD_SETPIXELSHADERSTATEAMBIENTLIGHTCUBE);
    m_Storage.PutInt(nConst);
  }

  SOURCE_FORCEINLINE void SetAmbientCubeDynamicStateVertexShader(void) {
    m_Storage.PutInt(CBCMD_SETAMBIENTCUBEDYNAMICSTATEVERTEXSHADER);
  }

  SOURCE_FORCEINLINE void SetPixelShaderFogParams(int nReg) {
    m_Storage.PutInt(CBCMD_SETPIXELSHADERFOGPARAMS);
    m_Storage.PutInt(nReg);
  }

  SOURCE_FORCEINLINE void BindStandardTexture(Sampler_t nSampler,
                                       StandardTextureId_t nTextureId) {
    m_Storage.PutInt(CBCMD_BIND_STANDARD_TEXTURE);
    m_Storage.PutInt(nSampler);
    m_Storage.PutInt(nTextureId);
  }

  SOURCE_FORCEINLINE void BindTexture(Sampler_t nSampler,
                               ShaderAPITextureHandle_t hTexture) {
    m_Storage.PutInt(CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE);
    m_Storage.PutInt(nSampler);
    m_Storage.PutInt(hTexture);
  }

  SOURCE_FORCEINLINE void BindTexture(CBaseVSShader *pShader, Sampler_t nSampler,
                               int nTextureVar, int nFrameVar) {
    ShaderAPITextureHandle_t hTexture =
        pShader->GetShaderAPITextureBindHandle(nTextureVar, nFrameVar);
    BindTexture(nSampler, hTexture);
  }

  SOURCE_FORCEINLINE void BindMultiTexture(CBaseVSShader *pShader, Sampler_t nSampler1,
                                    Sampler_t nSampler2, int nTextureVar,
                                    int nFrameVar) {
    ShaderAPITextureHandle_t hTexture =
        pShader->GetShaderAPITextureBindHandle(nTextureVar, nFrameVar, 0);
    BindTexture(nSampler1, hTexture);
    hTexture =
        pShader->GetShaderAPITextureBindHandle(nTextureVar, nFrameVar, 1);
    BindTexture(nSampler2, hTexture);
  }

  SOURCE_FORCEINLINE void SetPixelShaderIndex(int nIndex) {
    m_Storage.PutInt(CBCMD_SET_PSHINDEX);
    m_Storage.PutInt(nIndex);
  }

  SOURCE_FORCEINLINE void SetVertexShaderIndex(int nIndex) {
    m_Storage.PutInt(CBCMD_SET_VSHINDEX);
    m_Storage.PutInt(nIndex);
  }

  SOURCE_FORCEINLINE void SetDepthFeatheringPixelShaderConstant(
      int iConstant, float fDepthBlendScale) {
    m_Storage.PutInt(CBCMD_SET_DEPTH_FEATHERING_CONST);
    m_Storage.PutInt(iConstant);
    m_Storage.PutFloat(fDepthBlendScale);
  }

  SOURCE_FORCEINLINE void Goto(uint8_t *pCmdBuf) {
    m_Storage.PutInt(CBCMD_JUMP);
    m_Storage.PutPtr(pCmdBuf);
  }

  SOURCE_FORCEINLINE void Call(uint8_t *pCmdBuf) {
    m_Storage.PutInt(CBCMD_JSR);
    m_Storage.PutPtr(pCmdBuf);
  }

  SOURCE_FORCEINLINE void Reset(void) { m_Storage.Reset(); }

  SOURCE_FORCEINLINE size_t Size(void) const { return m_Storage.Size(); }

  SOURCE_FORCEINLINE uint8_t *Base(void) { return m_Storage.Base(); }
};

#endif  // commandbuilder_h
