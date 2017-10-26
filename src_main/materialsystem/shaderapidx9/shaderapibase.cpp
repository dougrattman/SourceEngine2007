// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#undef PROTECTED_THINGS_ENABLE  // prevent warnings when windows.h gets included

#include "shaderapibase.h"

#include "shaderapi/ishaderutil.h"

// The Base implementation of the shader render class

CShaderAPIBase::CShaderAPIBase() {}

CShaderAPIBase::~CShaderAPIBase() {}

// Methods of IShaderDynamicAPI
void CShaderAPIBase::GetCurrentColorCorrection(
    ShaderColorCorrectionInfo_t* pInfo) {
  g_pShaderUtil->GetCurrentColorCorrection(pInfo);
}
