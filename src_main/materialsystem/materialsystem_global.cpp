// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "base/include/windows/windows_light.h"

#include <malloc.h>
#include "filesystem.h"
#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "shadersystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

int g_FrameNum;
IShaderAPI* g_pShaderAPI = 0;
IShaderDeviceMgr* g_pShaderDeviceMgr = 0;
IShaderDevice* g_pShaderDevice = 0;
IShaderShadow* g_pShaderShadow = 0;
