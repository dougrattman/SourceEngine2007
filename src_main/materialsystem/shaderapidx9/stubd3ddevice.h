// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $


#ifndef STUBD3DDEVICE_H
#define STUBD3DDEVICE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef STUBD3D

#include "locald3dtypes.h"
#include "FileSystem.h"

#ifdef USE_FOPEN
#include <cstdio>
#define FPRINTF fprintf
#else
#define FPRINTF s_pFileSystem->FPrintf
#endif

#ifdef USE_FOPEN

static FILE *s_FileHandle;

#else

static IFileSystem *s_pFileSystem;
static FileHandle_t s_FileHandle;

#endif



class CStubD3DTexture : public IDirect3DTexture8
{
private:
	IDirect3DTexture8 *m_pTexture;
	IDirect3DDevice8 *m_pDevice;

public:
	CStubD3DTexture( IDirect3DTexture8 *pTexture, IDirect3DDevice8 *pDevice )
	{
		m_pTexture = pTexture;
		m_pDevice = pDevice;
	}

    /*** IUnknown methods ***/
    HRESULT SOURCE_STDCALL QueryInterface(REFIID riid, void** ppvObj)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::QueryInterface\n" );
		return m_pTexture->QueryInterface( riid, ppvObj );
	}

    ULONG SOURCE_STDCALL AddRef()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::AddRef\n" );
		return m_pTexture->AddRef();
	}

    ULONG SOURCE_STDCALL Release()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::Release\n" );
		return m_pTexture->Release();
	}

    /*** IDirect3DBaseTexture8 methods ***/
    HRESULT SOURCE_STDCALL GetDevice( IDirect3DDevice8** ppDevice )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetDevice\n" );
#if 0
		*ppDevice = m_pDevice;
		return D3D_OK;		
#else
		return m_pTexture->GetDevice( ppDevice );
#endif
	}

    HRESULT SOURCE_STDCALL SetPrivateData( REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetPrivateData\n" );
		return m_pTexture->SetPrivateData( refguid, pData, SizeOfData, Flags );
	}

    HRESULT SOURCE_STDCALL GetPrivateData( REFGUID refguid,void* pData,DWORD* pSizeOfData )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPrivateData\n" );
		return m_pTexture->GetPrivateData( refguid, pData, pSizeOfData );
	}

    HRESULT SOURCE_STDCALL FreePrivateData( REFGUID refguid )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPrivateData\n" );
		return m_pTexture->FreePrivateData( refguid );
	}
	
    DWORD SOURCE_STDCALL SetPriority( DWORD PriorityNew )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetPriority\n" );
		return m_pTexture->SetPriority( PriorityNew );
	}
	
    DWORD SOURCE_STDCALL GetPriority()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPriority\n" );
		return m_pTexture->GetPriority();
	}
	
    void SOURCE_STDCALL PreLoad()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::PreLoad\n" );
		m_pTexture->PreLoad();
	}
	
    D3DRESOURCETYPE SOURCE_STDCALL GetType()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetType\n" );
		return m_pTexture->GetType();
	}
	
    DWORD SOURCE_STDCALL SetLOD( DWORD LODNew )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetLOD\n" );
		return m_pTexture->SetLOD( LODNew );
	}
	
    DWORD SOURCE_STDCALL GetLOD()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLOD\n" );
		return m_pTexture->GetLOD();
	}
	
    DWORD SOURCE_STDCALL GetLevelCount()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLevelCount\n" );
		return m_pTexture->GetLevelCount();
	}
	
    HRESULT SOURCE_STDCALL GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLevelCount\n" );
		return m_pTexture->GetLevelDesc( Level, pDesc );
	}
	
    HRESULT SOURCE_STDCALL GetSurfaceLevel(UINT Level,IDirect3DSurface8** ppSurfaceLevel)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetSurfaceLevel\n" );
		return m_pTexture->GetSurfaceLevel( Level, ppSurfaceLevel );
	}
	
    HRESULT SOURCE_STDCALL LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::LockRect\n" );
		return m_pTexture->LockRect( Level, pLockedRect, pRect, Flags );
	}
	
    HRESULT SOURCE_STDCALL UnlockRect(UINT Level)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::UnlockRect\n" );
		return m_pTexture->UnlockRect( Level );
	}
	
    HRESULT SOURCE_STDCALL AddDirtyRect( CONST RECT* pDirtyRect )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::AddDirtyRect\n" );
		return m_pTexture->AddDirtyRect( pDirtyRect );
	}
};

class CStubD3DDevice : public IDirect3DDevice8
{
public:
	CStubD3DDevice( IDirect3DDevice8 *pD3DDevice, IFileSystem *pFileSystem )
	{
		Assert( pD3DDevice );
		m_pD3DDevice = pD3DDevice;
#ifdef USE_FOPEN
		s_FileHandle = fopen( "stubd3d.txt", "w" );
#else
		Assert( pFileSystem );
		s_pFileSystem = pFileSystem;
		s_FileHandle = pFileSystem->Open( "stubd3d.txt", "w" );
#endif
	}

	~CStubD3DDevice()
	{
#ifdef USE_FOPEN
		fclose( s_FileHandle );
#else
		s_pFileSystem->Close( s_FileHandle );
#endif
	}

private:
	IDirect3DDevice8 *m_pD3DDevice;

public:
    /*** IUnknown methods ***/
    HRESULT SOURCE_STDCALL QueryInterface(REFIID riid, void** ppvObj)
	{
		FPRINTF( s_FileHandle, "QueryInterface\n" );
		return m_pD3DDevice->QueryInterface( riid, ppvObj );
	}

    ULONG SOURCE_STDCALL AddRef()
	{
		FPRINTF( s_FileHandle, "AddRef\n" );
		return m_pD3DDevice->AddRef();
	}

    ULONG SOURCE_STDCALL Release()
	{
		FPRINTF( s_FileHandle, "Release\n" );
		return m_pD3DDevice->Release();
		delete this;
	}

    /*** IDirect3DDevice8 methods ***/
    HRESULT SOURCE_STDCALL TestCooperativeLevel()
	{
		FPRINTF( s_FileHandle, "TestCooperativeLevel\n" );
		return m_pD3DDevice->TestCooperativeLevel();
	}

    UINT SOURCE_STDCALL GetAvailableTextureMem()
	{
		FPRINTF( s_FileHandle, "GetAvailableTextureMem\n" );
		return m_pD3DDevice->GetAvailableTextureMem();
	}

    HRESULT SOURCE_STDCALL ResourceManagerDiscardBytes(DWORD Bytes)
	{
		FPRINTF( s_FileHandle, "ResourceManagerDiscardBytes\n" );
		return m_pD3DDevice->ResourceManagerDiscardBytes( Bytes );
	}

    HRESULT SOURCE_STDCALL GetDirect3D(IDirect3D8** ppD3D8)
	{
		FPRINTF( s_FileHandle, "GetDirect3D\n" );
		return m_pD3DDevice->GetDirect3D( ppD3D8 );
	}

    HRESULT SOURCE_STDCALL GetDeviceCaps(D3DCAPS8* pCaps)
	{
		FPRINTF( s_FileHandle, "GetDeviceCaps\n" );
		return m_pD3DDevice->GetDeviceCaps( pCaps );
	}

    HRESULT SOURCE_STDCALL GetDisplayMode(D3DDISPLAYMODE* pMode)
	{
		FPRINTF( s_FileHandle, "GetDisplayMode\n" );
		return m_pD3DDevice->GetDisplayMode( pMode );
	}

    HRESULT SOURCE_STDCALL GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
	{
		FPRINTF( s_FileHandle, "GetCreationParameters\n" );
		return m_pD3DDevice->GetCreationParameters( pParameters );
	}

    HRESULT SOURCE_STDCALL SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap)
	{
		FPRINTF( s_FileHandle, "SetCursorProperties\n" );
		return m_pD3DDevice->SetCursorProperties( XHotSpot, YHotSpot, pCursorBitmap );
	}

    void SOURCE_STDCALL SetCursorPosition(UINT XScreenSpace,UINT YScreenSpace,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "SetCursorPosition\n" );
		m_pD3DDevice->SetCursorPosition( XScreenSpace, YScreenSpace, Flags );
	}

    BOOL SOURCE_STDCALL ShowCursor(BOOL bShow)
	{
		FPRINTF( s_FileHandle, "ShowCursor\n" );
		return m_pD3DDevice->ShowCursor( bShow );
	}

    HRESULT SOURCE_STDCALL CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain)
	{
		FPRINTF( s_FileHandle, "CreateAdditionalSwapChain\n" );
		return m_pD3DDevice->CreateAdditionalSwapChain( pPresentationParameters, pSwapChain );
	}

    HRESULT SOURCE_STDCALL Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		FPRINTF( s_FileHandle, "Reset\n" );
		return m_pD3DDevice->Reset( pPresentationParameters );
	}

    HRESULT SOURCE_STDCALL Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
	{
		FPRINTF( s_FileHandle, "Present\n" );
		return m_pD3DDevice->Present( pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion );
	}

    HRESULT SOURCE_STDCALL GetBackBuffer(UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer)
	{
		FPRINTF( s_FileHandle, "GetBackBuffer\n" );
		return m_pD3DDevice->GetBackBuffer( BackBuffer, Type, ppBackBuffer );
	}

    HRESULT SOURCE_STDCALL GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
	{
		FPRINTF( s_FileHandle, "GetRasterStatus\n" );
		return m_pD3DDevice->GetRasterStatus( pRasterStatus );
	}

    void SOURCE_STDCALL SetGammaRamp(DWORD Flags,CONST D3DGAMMARAMP* pRamp)
	{
		FPRINTF( s_FileHandle, "SetGammaRamp\n" );
		m_pD3DDevice->SetGammaRamp( Flags, pRamp );
	}

    void SOURCE_STDCALL GetGammaRamp(D3DGAMMARAMP* pRamp)
	{
		FPRINTF( s_FileHandle, "GetGammaRamp\n" );
		m_pD3DDevice->GetGammaRamp( pRamp );
	}

    HRESULT SOURCE_STDCALL CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture)
	{
		FPRINTF( s_FileHandle, "CreateTexture\n" );
#if 0
		HRESULT ret = m_pD3DDevice->CreateTexture( Width, Height, Levels, Usage, Format, Pool, ppTexture );
		if( ret == D3D_OK )
		{
 *ppTexture = new CStubD3DTexture( *ppTexture, this );
 return ret;
		}
		else
		{
 return ret;
		}
#else
		return m_pD3DDevice->CreateTexture( Width, Height, Levels, Usage, Format, Pool, ppTexture );
#endif
	}

    HRESULT SOURCE_STDCALL CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture)
	{
		FPRINTF( s_FileHandle, "CreateVolumeTexture\n" );
		return m_pD3DDevice->CreateVolumeTexture( Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture );
	}

    HRESULT SOURCE_STDCALL CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture)
	{
		FPRINTF( s_FileHandle, "CreateCubeTexture\n" );
		return m_pD3DDevice->CreateCubeTexture( EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture );
	}

    HRESULT SOURCE_STDCALL CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer8** ppVertexBuffer)
	{
		FPRINTF( s_FileHandle, "CreateVertexBuffer\n" );
		return m_pD3DDevice->CreateVertexBuffer( Length, Usage, FVF, Pool, ppVertexBuffer );
	}

    HRESULT SOURCE_STDCALL CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer8** ppIndexBuffer)
	{
		FPRINTF( s_FileHandle, "CreateIndexBuffer\n" );
		return m_pD3DDevice->CreateIndexBuffer( Length, Usage, Format, Pool, ppIndexBuffer );
	}

    HRESULT SOURCE_STDCALL CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateRenderTarget\n" );
		return m_pD3DDevice->CreateRenderTarget( Width, Height, Format, MultiSample, Lockable, ppSurface );
	}

    HRESULT SOURCE_STDCALL CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateDepthStencilSurface\n" );
		return m_pD3DDevice->CreateDepthStencilSurface( Width, Height, Format, MultiSample, ppSurface );
	}

    HRESULT SOURCE_STDCALL CreateImageSurface(UINT Width,UINT Height,D3DFORMAT Format,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateImageSurface\n" );
		return m_pD3DDevice->CreateImageSurface( Width, Height, Format, ppSurface );
	}

    HRESULT SOURCE_STDCALL CopyRects(IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray)
	{
		FPRINTF( s_FileHandle, "CopyRects\n" );
		return m_pD3DDevice->CopyRects( pSourceSurface, pSourceRectsArray, cRects, pDestinationSurface, pDestPointsArray );
	}

    HRESULT SOURCE_STDCALL UpdateTexture(IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture)
	{
		FPRINTF( s_FileHandle, "UpdateTexture\n" );
		return m_pD3DDevice->UpdateTexture( pSourceTexture, pDestinationTexture );
	}

    HRESULT SOURCE_STDCALL GetFrontBuffer(IDirect3DSurface8* pDestSurface)
	{
		FPRINTF( s_FileHandle, "GetFrontBuffer\n" );
		return m_pD3DDevice->GetFrontBuffer( pDestSurface );
	}

    HRESULT SOURCE_STDCALL SetRenderTarget(IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil)
	{
		FPRINTF( s_FileHandle, "SetRenderTarget\n" );
		return m_pD3DDevice->SetRenderTarget( pRenderTarget, pNewZStencil );
	}

    HRESULT SOURCE_STDCALL GetRenderTarget(IDirect3DSurface8** ppRenderTarget)
	{
		FPRINTF( s_FileHandle, "GetRenderTarget\n" );
		return m_pD3DDevice->GetRenderTarget( ppRenderTarget );
	}

    HRESULT SOURCE_STDCALL GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface)
	{
		FPRINTF( s_FileHandle, "GetDepthStencilSurface\n" );
		return m_pD3DDevice->GetDepthStencilSurface( ppZStencilSurface );
	}

    HRESULT SOURCE_STDCALL BeginScene( void )
	{
		FPRINTF( s_FileHandle, "BeginScene\n" );
		return m_pD3DDevice->BeginScene();
	}

    HRESULT SOURCE_STDCALL EndScene()
	{
		FPRINTF( s_FileHandle, "EndScene\n" );
		return m_pD3DDevice->EndScene();
	}

    HRESULT SOURCE_STDCALL Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
	{
		FPRINTF( s_FileHandle, "Clear\n" );
		return m_pD3DDevice->Clear( Count, pRects, Flags, Color, Z, Stencil );
	}

    HRESULT SOURCE_STDCALL SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "SetTransform\n" );
		return m_pD3DDevice->SetTransform( State, pMatrix );
	}

    HRESULT SOURCE_STDCALL GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "GetTransform\n" );
		return m_pD3DDevice->GetTransform( State, pMatrix );
	}

    HRESULT SOURCE_STDCALL MultiplyTransform(D3DTRANSFORMSTATETYPE transformState,CONST D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "MultiplyTransform\n" );
		return m_pD3DDevice->MultiplyTransform( transformState, pMatrix );
	}

    HRESULT SOURCE_STDCALL SetViewport(CONST D3DVIEWPORT8* pViewport)
	{
		FPRINTF( s_FileHandle, "SetViewport\n" );
		return m_pD3DDevice->SetViewport( pViewport );
	}

    HRESULT SOURCE_STDCALL GetViewport(D3DVIEWPORT8* pViewport)
	{
		FPRINTF( s_FileHandle, "GetViewport\n" );
		return m_pD3DDevice->GetViewport( pViewport );
	}

    HRESULT SOURCE_STDCALL SetMaterial(CONST D3DMATERIAL8* pMaterial)
	{
		FPRINTF( s_FileHandle, "SetMaterial\n" );
		return m_pD3DDevice->SetMaterial( pMaterial );
	}

    HRESULT SOURCE_STDCALL GetMaterial(D3DMATERIAL8* pMaterial)
	{
		FPRINTF( s_FileHandle, "GetMaterial\n" );
		return m_pD3DDevice->GetMaterial( pMaterial );
	}

    HRESULT SOURCE_STDCALL SetLight(DWORD Index,CONST D3DLIGHT8* pLight)
	{
		FPRINTF( s_FileHandle, "SetLight\n" );
		return m_pD3DDevice->SetLight( Index, pLight );
	}

    HRESULT SOURCE_STDCALL GetLight(DWORD Index,D3DLIGHT8* pLight)
	{
		FPRINTF( s_FileHandle, "GetLight\n" );
		return m_pD3DDevice->GetLight( Index, pLight );
	}

    HRESULT SOURCE_STDCALL LightEnable(DWORD Index,BOOL Enable)
	{
		FPRINTF( s_FileHandle, "LightEnable\n" );
		return m_pD3DDevice->LightEnable( Index, Enable );
	}

    HRESULT SOURCE_STDCALL GetLightEnable(DWORD Index,BOOL* pEnable)
	{
		FPRINTF( s_FileHandle, "GetLightEnable\n" );
		return m_pD3DDevice->GetLightEnable( Index, pEnable );
	}

    HRESULT SOURCE_STDCALL SetClipPlane(DWORD Index,CONST float* pPlane)
	{
		FPRINTF( s_FileHandle, "SetClipPlane\n" );
		return m_pD3DDevice->SetClipPlane( Index, pPlane );
	}

    HRESULT SOURCE_STDCALL GetClipPlane(DWORD Index,float* pPlane)
	{
		FPRINTF( s_FileHandle, "GetClipPlane\n" );
		return m_pD3DDevice->GetClipPlane( Index, pPlane );
	}

    HRESULT SOURCE_STDCALL SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
	{
		FPRINTF( s_FileHandle, "SetRenderState\n" );
		return m_pD3DDevice->SetRenderState( State, Value );
	}

    HRESULT SOURCE_STDCALL GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue)
	{
		FPRINTF( s_FileHandle, "GetRenderState\n" );
		return m_pD3DDevice->GetRenderState( State, pValue );
	}

    HRESULT SOURCE_STDCALL BeginStateBlock(void)
	{
		FPRINTF( s_FileHandle, "BeginStateBlock\n" );
		return m_pD3DDevice->BeginStateBlock();
	}

    HRESULT SOURCE_STDCALL EndStateBlock(DWORD* pToken)
	{
		FPRINTF( s_FileHandle, "EndStateBlock\n" );
		return m_pD3DDevice->EndStateBlock( pToken );
	}

    HRESULT SOURCE_STDCALL ApplyStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "ApplyStateBlock\n" );
		return m_pD3DDevice->ApplyStateBlock( Token );
	}

    HRESULT SOURCE_STDCALL CaptureStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "CaptureStateBlock\n" );
		return m_pD3DDevice->CaptureStateBlock( Token );
	}

    HRESULT SOURCE_STDCALL DeleteStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "DeleteStateBlock\n" );
		return m_pD3DDevice->DeleteStateBlock( Token );
	}

    HRESULT SOURCE_STDCALL CreateStateBlock(D3DSTATEBLOCKTYPE Type,DWORD* pToken)
	{
		FPRINTF( s_FileHandle, "CreateStateBlock\n" );
		return m_pD3DDevice->CreateStateBlock( Type, pToken );
	}

    HRESULT SOURCE_STDCALL SetClipStatus(CONST D3DCLIPSTATUS8* pClipStatus)
	{
		FPRINTF( s_FileHandle, "SetClipStatus\n" );
		return m_pD3DDevice->SetClipStatus( pClipStatus );
	}

    HRESULT SOURCE_STDCALL GetClipStatus(D3DCLIPSTATUS8* pClipStatus)
	{
		FPRINTF( s_FileHandle, "GetClipStatus\n" );
		return m_pD3DDevice->GetClipStatus( pClipStatus );
	}

    HRESULT SOURCE_STDCALL GetTexture(DWORD Stage,IDirect3DBaseTexture8** ppTexture)
	{
		FPRINTF( s_FileHandle, "GetTexture\n" );
		return m_pD3DDevice->GetTexture( Stage, ppTexture );
	}

    HRESULT SOURCE_STDCALL SetTexture(DWORD Stage,IDirect3DBaseTexture8* pTexture)
	{
		FPRINTF( s_FileHandle, "SetTexture\n" );
		return m_pD3DDevice->SetTexture( Stage, pTexture );
	}

    HRESULT SOURCE_STDCALL GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
	{
		FPRINTF( s_FileHandle, "GetTextureStageState\n" );
		return m_pD3DDevice->GetTextureStageState( Stage, Type, pValue );
	}

    HRESULT SOURCE_STDCALL SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
	{
		FPRINTF( s_FileHandle, "SetTextureStageState\n" );
		return m_pD3DDevice->SetTextureStageState( Stage, Type, Value );
	}

    HRESULT SOURCE_STDCALL ValidateDevice(DWORD* pNumPasses)
	{
		FPRINTF( s_FileHandle, "ValidateDevice\n" );
#if 0
		return m_pD3DDevice->ValidateDevice( pNumPasses );
#else
		return D3D_OK;
#endif
	}

    HRESULT SOURCE_STDCALL GetInfo(DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize)
	{
		FPRINTF( s_FileHandle, "GetInfo\n" );
		return m_pD3DDevice->GetInfo( DevInfoID, pDevInfoStruct, DevInfoStructSize );
	}

    HRESULT SOURCE_STDCALL SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
	{
		FPRINTF( s_FileHandle, "SetPaletteEntries\n" );
		return m_pD3DDevice->SetPaletteEntries( PaletteNumber, pEntries );
	}

    HRESULT SOURCE_STDCALL GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries)
	{
		FPRINTF( s_FileHandle, "GetPaletteEntries\n" );
		return m_pD3DDevice->GetPaletteEntries( PaletteNumber, pEntries );
	}

    HRESULT SOURCE_STDCALL SetCurrentTexturePalette(UINT PaletteNumber)
	{
		FPRINTF( s_FileHandle, "SetCurrentTexturePalette\n" );
		return m_pD3DDevice->SetCurrentTexturePalette( PaletteNumber );
	}

    HRESULT SOURCE_STDCALL GetCurrentTexturePalette(UINT *PaletteNumber)
	{
		FPRINTF( s_FileHandle, "GetCurrentTexturePalette\n" );
		return m_pD3DDevice->GetCurrentTexturePalette( PaletteNumber );
	}

    HRESULT SOURCE_STDCALL DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
	{
		FPRINTF( s_FileHandle, "DrawPrimitive\n" );
		return m_pD3DDevice->DrawPrimitive( PrimitiveType, StartVertex, PrimitiveCount );
	}

    HRESULT SOURCE_STDCALL DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount)
	{
		FPRINTF( s_FileHandle, "DrawIndexedPrimitive\n" );
		return m_pD3DDevice->DrawIndexedPrimitive( primitiveType,minIndex,NumVertices,startIndex,primCount );
	}

    HRESULT SOURCE_STDCALL DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{
		FPRINTF( s_FileHandle, "DrawPrimitiveUP\n" );
		return m_pD3DDevice->DrawPrimitiveUP( PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride );
	}

    HRESULT SOURCE_STDCALL DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{
		FPRINTF( s_FileHandle, "DrawIndexedPrimitiveUP\n" );
		return m_pD3DDevice->DrawIndexedPrimitiveUP( PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat,pVertexStreamZeroData, VertexStreamZeroStride );
	}

    HRESULT SOURCE_STDCALL ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "ProcessVertices\n" );
		return m_pD3DDevice->ProcessVertices( SrcStartIndex, DestIndex, VertexCount, pDestBuffer, Flags );
	}

    HRESULT SOURCE_STDCALL CreateVertexShader(CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage)
	{
		FPRINTF( s_FileHandle, "CreateVertexShader\n" );
		return m_pD3DDevice->CreateVertexShader( pDeclaration, pFunction, pHandle, Usage );
	}

    HRESULT SOURCE_STDCALL SetVertexShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "SetVertexShader\n" );
		return m_pD3DDevice->SetVertexShader( Handle );
	}

    HRESULT SOURCE_STDCALL GetVertexShader(DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "GetVertexShader\n" );
		return m_pD3DDevice->GetVertexShader( pHandle );
	}

    HRESULT SOURCE_STDCALL DeleteVertexShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "DeleteVertexShader\n" );
		return m_pD3DDevice->DeleteVertexShader( Handle );
	}

    HRESULT SOURCE_STDCALL SetVertexShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "SetVertexShaderConstant\n" );
		return m_pD3DDevice->SetVertexShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT SOURCE_STDCALL GetVertexShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderConstant\n" );
		return m_pD3DDevice->GetVertexShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT SOURCE_STDCALL GetVertexShaderDeclaration(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderDeclaration\n" );
		return m_pD3DDevice->GetVertexShaderDeclaration( Handle, pData, pSizeOfData );
	}

    HRESULT SOURCE_STDCALL GetVertexShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderFunction\n" );
		return m_pD3DDevice->GetVertexShaderFunction( Handle, pData, pSizeOfData );
	}

    HRESULT SOURCE_STDCALL SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride)
	{
		FPRINTF( s_FileHandle, "SetStreamSource\n" );
		return m_pD3DDevice->SetStreamSource( StreamNumber, pStreamData, Stride );
	}

    HRESULT SOURCE_STDCALL GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride)
	{
		FPRINTF( s_FileHandle, "GetStreamSource\n" );
		return m_pD3DDevice->GetStreamSource( StreamNumber, ppStreamData, pStride );
	}

    HRESULT SOURCE_STDCALL SetIndices(IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex)
	{
		FPRINTF( s_FileHandle, "SetIndices\n" );
		return m_pD3DDevice->SetIndices( pIndexData, BaseVertexIndex );
	}

    HRESULT SOURCE_STDCALL GetIndices(IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex)
	{
		FPRINTF( s_FileHandle, "GetIndices\n" );
		return m_pD3DDevice->GetIndices( ppIndexData, pBaseVertexIndex );
	}

    HRESULT SOURCE_STDCALL CreatePixelShader(CONST DWORD* pFunction,DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "CreatePixelShader\n" );
		return m_pD3DDevice->CreatePixelShader( pFunction, pHandle );
	}

    HRESULT SOURCE_STDCALL SetPixelShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "SetPixelShader\n" );
		return m_pD3DDevice->SetPixelShader( Handle );
	}

    HRESULT SOURCE_STDCALL GetPixelShader(DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "GetPixelShader\n" );
		return m_pD3DDevice->GetPixelShader( pHandle );
	}

    HRESULT SOURCE_STDCALL DeletePixelShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "DeletePixelShader\n" );
		return m_pD3DDevice->DeletePixelShader( Handle );
	}

    HRESULT SOURCE_STDCALL SetPixelShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "SetPixelShaderConstant\n" );
		return m_pD3DDevice->SetPixelShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT SOURCE_STDCALL GetPixelShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "GetPixelShaderConstant\n" );
		return m_pD3DDevice->GetPixelShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT SOURCE_STDCALL GetPixelShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetPixelShaderFunction\n" );
		return m_pD3DDevice->GetPixelShaderFunction( Handle, pData, pSizeOfData );
	}

    HRESULT SOURCE_STDCALL DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
	{
		FPRINTF( s_FileHandle, "DrawRectPatch\n" );
		return m_pD3DDevice->DrawRectPatch( Handle, pNumSegs, pRectPatchInfo );
	}

    HRESULT SOURCE_STDCALL DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
	{
		FPRINTF( s_FileHandle, "DrawTriPatch\n" );
		return m_pD3DDevice->DrawTriPatch( Handle, pNumSegs, pTriPatchInfo );
	}

    HRESULT SOURCE_STDCALL DeletePatch(UINT Handle)
	{
		FPRINTF( s_FileHandle, "DeletePatch\n" );
		return m_pD3DDevice->DeletePatch( Handle );
	}
};

#endif // STUBD3D

#endif // STUBD3DDEVICE_H

