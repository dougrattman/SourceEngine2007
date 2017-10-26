// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef LOCALD3DTYPES_H
#define LOCALD3DTYPES_H

#include "winlite.h"

#ifdef DX10

#include <d3d10.h>
#include "dx9sdk/include/d3dx10.h"

struct IDirect3D10BaseTexture {
  ID3D10Resource *m_pBaseTexture;
  ID3D10ShaderResourceView *m_pSRView;
  ID3D10RenderTargetView *m_pRTView;
};

class CDx10Types {
 public:
  typedef struct IDirect3D10BaseTexture IDirect3DTexture;
  // FIXME: What is this called now ?
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

  typedef ID3D10Device *LPDIRECT3DDEVICE;
  typedef ID3D10Buffer *LPDIRECT3DINDEXBUFFER;
  typedef ID3D10Buffer *LPDIRECT3DVERTEXBUFFER;
};

#endif  // DX10

#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif
#include <d3d9.h>
#include "dx9sdk/include/d3dx9.h"
#include "dx9sdk/include/d3dx9core.h"

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

typedef struct _D3DLIGHT9 D3DLIGHT9;
typedef struct _D3DADAPTER_IDENTIFIER9 D3DADAPTER_IDENTIFIER9;
typedef struct _D3DCAPS9 D3DCAPS9;
typedef struct _D3DVIEWPORT9 D3DVIEWPORT9;
typedef struct _D3DMATERIAL9 D3DMATERIAL9;
typedef IDirect3DTexture9 IDirect3DTexture;
typedef IDirect3DBaseTexture9 IDirect3DBaseTexture;
typedef IDirect3DCubeTexture9 IDirect3DCubeTexture;
typedef IDirect3DVolumeTexture9 IDirect3DVolumeTexture;
typedef IDirect3DDevice9 IDirect3DDevice;
typedef D3DMATERIAL9 D3DMATERIAL;
typedef D3DLIGHT9 D3DLIGHT;
typedef IDirect3DSurface9 IDirect3DSurface;
typedef D3DCAPS9 D3DCAPS;
typedef IDirect3DIndexBuffer9 IDirect3DIndexBuffer;
typedef IDirect3DVertexBuffer9 IDirect3DVertexBuffer;
typedef IDirect3DPixelShader9 IDirect3DPixelShader;
typedef IDirect3DDevice *LPDIRECT3DDEVICE;
typedef IDirect3DIndexBuffer *LPDIRECT3DINDEXBUFFER;
typedef IDirect3DVertexBuffer *LPDIRECT3DVERTEXBUFFER;

class CDx9Types {
 public:
  typedef IDirect3DTexture9 IDirect3DTexture;
  typedef IDirect3DBaseTexture9 IDirect3DBaseTexture;
  typedef IDirect3DCubeTexture9 IDirect3DCubeTexture;
  typedef IDirect3DVolumeTexture9 IDirect3DVolumeTexture;
  typedef IDirect3DDevice9 IDirect3DDevice;
  typedef D3DMATERIAL9 D3DMATERIAL;
  typedef D3DLIGHT9 D3DLIGHT;
  typedef IDirect3DSurface9 IDirect3DSurface;
  typedef D3DCAPS9 D3DCAPS;
  typedef IDirect3DIndexBuffer9 IDirect3DIndexBuffer;
  typedef IDirect3DVertexBuffer9 IDirect3DVertexBuffer;
  typedef IDirect3DPixelShader9 IDirect3DPixelShader;
  typedef IDirect3DDevice *LPDIRECT3DDEVICE;
  typedef IDirect3DIndexBuffer *LPDIRECT3DINDEXBUFFER;
  typedef IDirect3DVertexBuffer *LPDIRECT3DVERTEXBUFFER;
};

typedef void *HardwareShader_t;

//-----------------------------------------------------------------------------
// The vertex and pixel shader type
//-----------------------------------------------------------------------------
typedef int VertexShader_t;
typedef int PixelShader_t;

//-----------------------------------------------------------------------------
// Bitpattern for an invalid shader
//-----------------------------------------------------------------------------
#define INVALID_SHADER (0xFFFFFFFF)
#define INVALID_HARDWARE_SHADER (NULL)

#define D3DSAMP_NOTSUPPORTED D3DSAMP_FORCE_DWORD
#define D3DRS_NOTSUPPORTED D3DRS_FORCE_DWORD

#endif  // LOCALD3DTYPES_H
