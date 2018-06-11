// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_LOCALD3DTYPES_H_
#define MATERIALSYSTEM_SHADERAPIDX9_LOCALD3DTYPES_H_

#include "base/include/windows/windows_light.h"

#ifdef DX10

#include <d3d10.h>
#include "deps/dxsdk_jun2010/Include/D3DX10.h"

struct IDirect3D10BaseTexture {
  ID3D10Resource *m_pBaseTexture;
  ID3D10ShaderResourceView *m_pSRView;
  ID3D10RenderTargetView *m_pRTView;
};

struct CDx10Types {
  typedef struct IDirect3D10BaseTexture IDirect3DTexture;
  // TODO(d.rattman): What is this called now ?
  // typedef ID3D10TextureCube IDirect3DCubeTexture;
  typedef ID3D10Texture3D IDirect3DVolumeTexture;
  typedef ID3D10Device IDirect3DDevice;
  typedef D3D10_VIEWPORT D3DVIEWPORT;
  typedef ID3D10Buffer IDirect3DIndexBuffer;
  typedef ID3D10Buffer IDirect3DVertexBuffer;
  typedef ID3D10VertexShader IDirect3DVertexShader;
  typedef ID3D10PixelShader IDirect3DPixelShader;
  typedef ID3D10ShaderResourceView IDirect3DSurface;
  typedef ID3DX10Font ID3DXFont;
  typedef ID3D10Query ID3DQuery;
};

#endif  // DX10

#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif
#include <d3d9.h>
#include "deps/dxsdk_jun2010/Include/d3dx9.h"
#include "deps/dxsdk_jun2010/Include/d3dx9core.h"

struct IDirect3DTexture9;
struct IDirect3DBaseTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexBuffer9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;
struct IDirect3DVolumeTexture9;

using D3DLIGHT9 = struct _D3DLIGHT9;
using D3DADAPTER_IDENTIFIER9 = struct _D3DADAPTER_IDENTIFIER9;
using D3DCAPS9 = struct _D3DCAPS9;
using D3DVIEWPORT9 = struct _D3DVIEWPORT9;
using D3DMATERIAL9 = struct _D3DMATERIAL9;
using IDirect3DTexture = IDirect3DTexture9;
using IDirect3DBaseTexture = IDirect3DBaseTexture9;
using IDirect3DCubeTexture = IDirect3DCubeTexture9;
using IDirect3DVolumeTexture = IDirect3DVolumeTexture9;
using IDirect3DDevice = IDirect3DDevice9;
using D3DMATERIAL = D3DMATERIAL9;
using D3DLIGHT = D3DLIGHT9;
using IDirect3DSurface = IDirect3DSurface9;
using D3DCAPS = D3DCAPS9;
using IDirect3DIndexBuffer = IDirect3DIndexBuffer9;
using IDirect3DVertexBuffer = IDirect3DVertexBuffer9;
using IDirect3DPixelShader = IDirect3DPixelShader9;

struct CDx9Types {
  using IDirect3DTexture = IDirect3DTexture9;
  using IDirect3DBaseTexture = IDirect3DBaseTexture9;
  using IDirect3DCubeTexture = IDirect3DCubeTexture9;
  using IDirect3DVolumeTexture = IDirect3DVolumeTexture9;
  using IDirect3DDevice = IDirect3DDevice9;
  using D3DMATERIAL = D3DMATERIAL9;
  using D3DLIGHT = D3DLIGHT9;
  using IDirect3DSurface = IDirect3DSurface9;
  using D3DCAPS = D3DCAPS9;
  using IDirect3DIndexBuffer = IDirect3DIndexBuffer9;
  using IDirect3DVertexBuffer = IDirect3DVertexBuffer9;
  using IDirect3DPixelShader = IDirect3DPixelShader9;
};

using HardwareShader_t = void *;

// The vertex and pixel shader type
using VertexShader_t = int;
using PixelShader_t = int;

// Bitpattern for an invalid shader
#define INVALID_SHADER (0xFFFFFFFF)
#define INVALID_HARDWARE_SHADER (nullptr)

#define D3DSAMP_NOTSUPPORTED D3DSAMP_FORCE_DWORD
#define D3DRS_NOTSUPPORTED D3DRS_FORCE_DWORD

#endif  // MATERIALSYSTEM_SHADERAPIDX9_LOCALD3DTYPES_H_
