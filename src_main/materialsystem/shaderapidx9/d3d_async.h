// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_D3DASYNC_H_
#define MATERIALSYSTEM_SHADERAPIDX9_D3DASYNC_H_

#define NOMINMAX

#include "base/include/base_types.h"
#include "base/include/windows/windows_light.h"

#include <d3d9.h>
#include "dx9sdk/include/d3dx9.h"
#include "recording.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"
#include "tier1/strtools.h"

#ifdef NDEBUG
#define DO_D3D(x) return Dx9Device()->x
#else
#define DO_D3D(x)                                               \
  {                                                             \
    HRESULT hr = Dx9Device()->x;                                \
    AssertMsg1(SUCCEEDED(hr), #x " call failed (0x%8.x).", hr); \
    return hr;                                                  \
  }
#endif

#define PUSHBUFFER_NELEMS 4096

enum PushBufferState {
  PUSHBUFFER_AVAILABLE,
  PUSHBUFFER_BEING_FILLED,
  PUSHBUFFER_SUBMITTED,
  PUSHBUFFER_BEING_USED_FOR_LOCKEDDATA,
};

class PushBuffer {
  friend class Direct3DDevice9Wrapper;

  volatile PushBufferState m_State;
  u32 m_BufferData[PUSHBUFFER_NELEMS];

 public:
  PushBuffer() : m_State{PUSHBUFFER_AVAILABLE} {}
};

// When running multithreaded, lock for write calls actually return a pointer to
// temporary memory buffer. When the buffer is later unlocked by the caller,
// data must be queued with the Unlock() that lets the d3d thread know how much
// data to copy from where.  One possible optimization for things which write a
// lot of data into lock buffers woudl be to proviude a way for the caller to
// occasionally check if the Lock() has been dequeued. If so, the the data
// pushed so far could be copied asynchronously into the buffer, while the
// caller would be told to switch to writing directly to the vertex buffer.
//
// another possibility would be lock()ing in advance for large ones, such as the
// world renderer, or keeping multiple locked vb's open for meshbuilder.
struct LockedBufferContext {
  // If a push buffer was used to hold the temporary data, this will be non-0.
  // If memory had to be malloc'd, this will be set.
  void *m_pMallocedMemory;

  PushBuffer *m_pPushBuffer;

  // # of bytes malloced if mallocedmem ptr non-0.
  size_t m_MallocSize;

  LockedBufferContext()
      : m_pPushBuffer{nullptr}, m_pMallocedMemory{nullptr}, m_MallocSize{0} {}
};

// Push buffer commands follow.
enum PushBufferCommand {
  PBCMD_END,                // at end of push buffer
  PBCMD_SET_RENDERSTATE,    // state, val
  PBCMD_SET_TEXTURE,        // stage, txtr
  PBCMD_DRAWPRIM,           // prim type, start v, nprims
  PBCMD_DRAWINDEXEDPRIM,    // prim type, baseidx, minidx, numv, starti, pcount
  PBCMD_SET_PIXEL_SHADER,   // shaderptr
  PBCMD_SET_VERTEX_SHADER,  // shaderptr
  PBCMD_SET_PIXEL_SHADER_CONSTANT,           // startreg, nregs, data...
  PBCMD_SET_BOOLEAN_PIXEL_SHADER_CONSTANT,   // startreg, nregs, data...
  PBCMD_SET_INTEGER_PIXEL_SHADER_CONSTANT,   // startreg, nregs, data...
  PBCMD_SET_VERTEX_SHADER_CONSTANT,          // startreg, nregs, data...
  PBCMD_SET_BOOLEAN_VERTEX_SHADER_CONSTANT,  // startreg, nregs, data...
  PBCMD_SET_INTEGER_VERTEX_SHADER_CONSTANT,  // startreg, nregs, data...
  PBCMD_SET_RENDER_TARGET,                   // idx, targetptr
  PBCMD_SET_DEPTH_STENCIL_SURFACE,           // surfptr
  PBCMD_SET_STREAM_SOURCE,                   // idx, sptr, ofs, stride
  PBCMD_SET_INDICES,                         // idxbuffer
  PBCMD_SET_SAMPLER_STATE,                   // stage, state, val
  PBCMD_UNLOCK_VB,                           // vptr
  PBCMD_UNLOCK_IB,                           // idxbufptr
  PBCMD_SETVIEWPORT,                         // vp_struct
  PBCMD_CLEAR,  // count, n rect structs, flags, color, z, stencil
  PBCMD_SET_VERTEXDECLARATION,  // vdeclptr
  PBCMD_BEGIN_SCENE,            //
  PBCMD_END_SCENE,              //
  PBCMD_PRESENT,                // complicated..see code
  PBCMD_SETCLIPPLANE,           // idx, 4 floats
  PBCMD_STRETCHRECT,            // see code
  PBCMD_ASYNC_LOCK_VB,          // see code
  PBCMD_ASYNC_UNLOCK_VB,
  PBCMD_ASYNC_LOCK_IB,  // see code
  PBCMD_ASYNC_UNLOCK_IB,
};

#define N_DWORDS(x) ((sizeof(x) + 3) / sizeof(DWORD))
#define N_DWORDS_IN_PTR (N_DWORDS(void *))

class Direct3DDevice9Wrapper {
 public:
  Direct3DDevice9Wrapper()
      : d3d9_device_{nullptr},
        async_thread_handle_{0},
        current_push_buffer_{nullptr},
        output_ptr_{nullptr},
        push_buffer_free_slots_{0} {}

  // This is what the worker thread runs.
  void RunThread();

  void SetASyncMode(bool onoff);

  SOURCE_FORCEINLINE bool IsActive() const { return d3d9_device_ != nullptr; }

  SOURCE_FORCEINLINE void SetDevicePtr(IDirect3DDevice9 *d3d9_device) {
    d3d9_device_ = d3d9_device;
  }

  SOURCE_FORCEINLINE void ShutDownDevice() {
    if (ASyncMode()) {
      // sync w/ thread
    }
    d3d9_device_ = nullptr;
  }

  SOURCE_FORCEINLINE HRESULT
  SetDepthStencilSurface(IDirect3DSurface9 *new_d3d9_surface_stencil) {
    if (ASyncMode()) {
      Push(PBCMD_SET_DEPTH_STENCIL_SURFACE, new_d3d9_surface_stencil);
      return S_OK;
    }

    DO_D3D(SetDepthStencilSurface(new_d3d9_surface_stencil));
  }

  HRESULT CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage,
                            D3DFORMAT Format, D3DPOOL Pool,
                            IDirect3DCubeTexture9 **ppCubeTexture,
                            HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool,
                             ppCubeTexture, pSharedHandle));
  }

  HRESULT CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels,
                              DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                              IDirect3DVolumeTexture9 **ppVolumeTexture,
                              HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format,
                               Pool, ppVolumeTexture, pSharedHandle));
  }

  HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                      D3DPOOL Pool,
                                      IDirect3DSurface9 **ppSurface,
                                      HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface,
                                       pSharedHandle));
  }

  HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage,
                        D3DFORMAT Format, D3DPOOL Pool,
                        IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture,
                         pSharedHandle));
  }

  HRESULT GetRenderTargetData(IDirect3DSurface9 *pRenderTarget,
                              IDirect3DSurface9 *pDestSurface) {
    Synchronize();
    DO_D3D(GetRenderTargetData(pRenderTarget, pDestSurface));
  }

  HRESULT GetDeviceCaps(D3DCAPS9 *pCaps) {
    Synchronize();
    DO_D3D(GetDeviceCaps(pCaps));
  }

  LPCSTR GetPixelShaderProfile() {
    Synchronize();
    return D3DXGetPixelShaderProfile(d3d9_device_);
  }

  HRESULT TestCooperativeLevel() {
    Synchronize();
    DO_D3D(TestCooperativeLevel());
  }

  HRESULT GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface) {
    Synchronize();
    DO_D3D(GetFrontBufferData(iSwapChain, pDestSurface));
  }

  void SetGammaRamp(u32 swapchain, u32 flags, const D3DGAMMARAMP *pRamp) {
    Synchronize();
    Dx9Device()->SetGammaRamp(swapchain, flags, pRamp);
  }

  HRESULT GetTexture(u32 Stage, IDirect3DBaseTexture9 **ppTexture) {
    Synchronize();
    DO_D3D(GetTexture(Stage, ppTexture));
  }

  HRESULT GetFVF(DWORD *pFVF) {
    Synchronize();
    DO_D3D(GetFVF(pFVF));
  }

  HRESULT GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) {
    Synchronize();
    DO_D3D(GetDepthStencilSurface(ppZStencilSurface));
  }

  SOURCE_FORCEINLINE HRESULT SetClipPlane(u32 idx, const float *pplane) {
    RECORD_COMMAND(DX8_SET_CLIP_PLANE, 5);
    RECORD_INT(idx);
    RECORD_FLOAT(pplane[0]);
    RECORD_FLOAT(pplane[1]);
    RECORD_FLOAT(pplane[2]);
    RECORD_FLOAT(pplane[3]);

    if (ASyncMode()) {
      AllocatePushBufferSpace(6);
      output_ptr_[0] = PBCMD_SETCLIPPLANE;
      output_ptr_[1] = idx;
      memcpy(output_ptr_ + 2, pplane, 4 * sizeof(float));
      output_ptr_ += 6;
      return S_OK;
    }

    DO_D3D(SetClipPlane(idx, pplane));
  }

  SOURCE_FORCEINLINE HRESULT
  SetVertexDeclaration(IDirect3DVertexDeclaration9 *decl) {
    RECORD_COMMAND(DX8_SET_VERTEX_DECLARATION, 1);
    RECORD_PTR(decl);

    if (ASyncMode()) {
      Push(PBCMD_SET_VERTEXDECLARATION, decl);
      return S_OK;
    }

    DO_D3D(SetVertexDeclaration(decl));
  }

  SOURCE_FORCEINLINE HRESULT SetViewport(const D3DVIEWPORT9 *vp) {
    RECORD_COMMAND(DX8_SET_VIEWPORT, 1);
    RECORD_STRUCT(vp, sizeof(*vp));

    if (ASyncMode()) {
      PushStruct(PBCMD_SETVIEWPORT, vp);
      return S_OK;
    }

    DO_D3D(SetViewport(vp));
  }

  HRESULT GetRenderTarget(DWORD RenderTargetIndex,
                          IDirect3DSurface9 **ppRenderTarget) {
    Synchronize();
    DO_D3D(GetRenderTarget(RenderTargetIndex, ppRenderTarget));
  }

  HRESULT CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery) {
    Synchronize();
    DO_D3D(CreateQuery(Type, ppQuery));
  }

  HRESULT CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                             D3DMULTISAMPLE_TYPE MultiSample,
                             DWORD MultisampleQuality, BOOL Lockable,
                             IDirect3DSurface9 **ppSurface,
                             HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateRenderTarget(Width, Height, Format, MultiSample,
                              MultisampleQuality, Lockable, ppSurface,
                              pSharedHandle));
  }

  HRESULT CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                    D3DMULTISAMPLE_TYPE MultiSample,
                                    DWORD MultisampleQuality, BOOL Discard,
                                    IDirect3DSurface9 **ppSurface,
                                    HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateDepthStencilSurface(Width, Height, Format, MultiSample,
                                     MultisampleQuality, Discard, ppSurface,
                                     pSharedHandle));
  }

  SOURCE_FORCEINLINE HRESULT SetRenderTarget(u32 idx,
                                             IDirect3DSurface9 *new_rt) {
    if (ASyncMode()) {
      Push(PBCMD_SET_RENDER_TARGET, idx, new_rt);
      return S_OK;
    }

    // NOTE: If the debug runtime breaks here on the shadow depth render
    // target that is normal.  dx9 doesn't directly support shadow depth
    // texturing so we are forced to initialize this texture without the
    // render target flag.
    DO_D3D(SetRenderTarget(idx, new_rt));
  }

  SOURCE_FORCEINLINE HRESULT LightEnable(u32 lidx, BOOL onoff) {
    RECORD_COMMAND(DX8_LIGHT_ENABLE, 2);
    RECORD_INT(lidx);
    RECORD_INT(onoff);

    Synchronize();
    return d3d9_device_->LightEnable(lidx, onoff);
  }

  SOURCE_FORCEINLINE HRESULT SetRenderState(D3DRENDERSTATETYPE state, u32 val) {
    RECORD_RENDER_STATE(state, val);

    if (ASyncMode()) {
      Push(PBCMD_SET_RENDERSTATE, state, val);
      return S_OK;
    }

    DO_D3D(SetRenderState(state, val));
  }

  SOURCE_FORCEINLINE HRESULT SetScissorRect(const RECT *pScissorRect) {
    RECORD_COMMAND(DX8_SET_SCISSOR_RECT, 1);
    RECORD_STRUCT(pScissorRect, sizeof(*pScissorRect));

    if (ASyncMode()) {
      // Is this just for XBox?
      Assert(0);
      return E_FAIL;
    }

    DO_D3D(SetScissorRect(pScissorRect));
  }

  SOURCE_FORCEINLINE HRESULT SetVertexShaderConstantF(
      u32 StartRegister, const float *pConstantData, u32 Vector4fCount) {
    RECORD_COMMAND(DX8_SET_VERTEX_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(Vector4fCount);
    RECORD_STRUCT(pConstantData, Vector4fCount * 4 * sizeof(float));

    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + 4 * Vector4fCount);
      output_ptr_[0] = PBCMD_SET_VERTEX_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = Vector4fCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(float) * 4 * Vector4fCount);
      output_ptr_ += 3 + 4 * Vector4fCount;
      return S_OK;
    }

    DO_D3D(
        SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount));
  }

  SOURCE_FORCEINLINE HRESULT SetVertexShaderConstantB(u32 StartRegister,
                                                      const int *pConstantData,
                                                      u32 BoolCount) {
    RECORD_COMMAND(DX8_SET_VERTEX_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(BoolCount);
    RECORD_STRUCT(pConstantData, BoolCount * sizeof(int));
    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + BoolCount);
      output_ptr_[0] = PBCMD_SET_BOOLEAN_VERTEX_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = BoolCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(int) * BoolCount);
      output_ptr_ += 3 + BoolCount;
      return S_OK;
    }

    DO_D3D(SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount));
  }

  SOURCE_FORCEINLINE HRESULT SetVertexShaderConstantI(UINT StartRegister,
                                                      CONST int *pConstantData,
                                                      UINT Vector4IntCount) {
    RECORD_COMMAND(DX8_SET_VERTEX_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(Vector4IntCount);
    RECORD_STRUCT(pConstantData, Vector4IntCount * 4 * sizeof(int));

    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + 4 * Vector4IntCount);
      output_ptr_[0] = PBCMD_SET_INTEGER_VERTEX_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = Vector4IntCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(int) * 4 * Vector4IntCount);
      output_ptr_ += 3 + 4 * Vector4IntCount;
      return S_OK;
    }

    DO_D3D(SetVertexShaderConstantI(StartRegister, pConstantData,
                                    Vector4IntCount));
  }

  SOURCE_FORCEINLINE HRESULT SetPixelShaderConstantF(UINT StartRegister,
                                                     CONST float *pConstantData,
                                                     UINT Vector4fCount) {
    RECORD_COMMAND(DX8_SET_PIXEL_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(Vector4fCount);
    RECORD_STRUCT(pConstantData, Vector4fCount * 4 * sizeof(float));

    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + 4 * Vector4fCount);
      output_ptr_[0] = PBCMD_SET_PIXEL_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = Vector4fCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(float) * 4 * Vector4fCount);
      output_ptr_ += 3 + 4 * Vector4fCount;
      return S_OK;
    }

    DO_D3D(
        SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount));
  }

  SOURCE_FORCEINLINE HRESULT SetPixelShaderConstantB(UINT StartRegister,
                                                     CONST int *pConstantData,
                                                     UINT BoolCount) {
    RECORD_COMMAND(DX8_SET_PIXEL_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(BoolCount);
    RECORD_STRUCT(pConstantData, BoolCount * sizeof(int));

    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + BoolCount);
      output_ptr_[0] = PBCMD_SET_BOOLEAN_PIXEL_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = BoolCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(int) * BoolCount);
      output_ptr_ += 3 + BoolCount;
      return S_OK;
    }

    DO_D3D(SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount));
  }

  SOURCE_FORCEINLINE HRESULT SetPixelShaderConstantI(UINT StartRegister,
                                                     CONST int *pConstantData,
                                                     UINT Vector4IntCount) {
    RECORD_COMMAND(DX8_SET_PIXEL_SHADER_CONSTANT, 3);
    RECORD_INT(StartRegister);
    RECORD_INT(Vector4IntCount);
    RECORD_STRUCT(pConstantData, Vector4IntCount * 4 * sizeof(int));

    if (ASyncMode()) {
      AllocatePushBufferSpace(3 + 4 * Vector4IntCount);
      output_ptr_[0] = PBCMD_SET_INTEGER_PIXEL_SHADER_CONSTANT;
      output_ptr_[1] = StartRegister;
      output_ptr_[2] = Vector4IntCount;
      memcpy(output_ptr_ + 3, pConstantData, sizeof(int) * 4 * Vector4IntCount);
      output_ptr_ += 3 + 4 * Vector4IntCount;
      return S_OK;
    }

    DO_D3D(
        SetPixelShaderConstantI(StartRegister, pConstantData, Vector4IntCount));
  }

  SOURCE_FORCEINLINE HRESULT StretchRect(IDirect3DSurface9 *pSourceSurface,
                                         CONST RECT *pSourceRect,
                                         IDirect3DSurface9 *pDestSurface,
                                         CONST RECT *pDestRect,
                                         D3DTEXTUREFILTERTYPE Filter) {
    if (ASyncMode()) {
      AllocatePushBufferSpace(1 + 1 + 1 + N_DWORDS(RECT) + 1 + 1 +
                              N_DWORDS(RECT) + 1);
      *(output_ptr_++) = PBCMD_STRETCHRECT;
      *(output_ptr_++) = (int)pSourceSurface;
      *(output_ptr_++) = (pSourceRect != NULL);

      if (pSourceRect) memcpy(output_ptr_, pSourceRect, sizeof(RECT));

      output_ptr_ += N_DWORDS(RECT);
      *(output_ptr_++) = (int)pDestSurface;
      *(output_ptr_++) = (pDestRect != NULL);

      if (pDestRect) memcpy(output_ptr_, pDestRect, sizeof(RECT));

      output_ptr_ += N_DWORDS(RECT);
      *(output_ptr_++) = Filter;

      return S_OK;  // !bug!
    }

    DO_D3D(StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect,
                       Filter));
  }

  SOURCE_FORCEINLINE void BeginScene() {
    RECORD_COMMAND(DX8_BEGIN_SCENE, 0);

    if (ASyncMode())
      Push(PBCMD_BEGIN_SCENE);
    else
      Dx9Device()->BeginScene();
  }

  SOURCE_FORCEINLINE HRESULT EndScene() {
    RECORD_COMMAND(DX8_END_SCENE, 0);

    if (ASyncMode()) {
      Push(PBCMD_END_SCENE);
      return S_OK;
    }

    DO_D3D(EndScene());
  }

  SOURCE_FORCEINLINE HRESULT Lock(IDirect3DVertexBuffer9 *vb, size_t offset,
                                  size_t size, void **ptr, DWORD flags) {
    Assert(size);  // lock size of 0 = unknown entire size of buffer = bad
    Synchronize();

    HRESULT hr = vb->Lock(offset, size, ptr, flags);
    switch (hr) {
      case S_OK:
        break;
      case D3DERR_INVALIDCALL:
        Warning(
            "D3DERR_INVALIDCALL - Vertex Buffer Lock Failed in %s on line "
            "%d(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_DRIVERINTERNALERROR:
        Warning(
            "D3DERR_DRIVERINTERNALERROR - Vertex Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        Warning(
            "D3DERR_OUTOFVIDEOMEMORY - Vertex Buffer Lock Failed in %s on line "
            "%d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      default:
        Warning(
            "(0x%.8x) - Vertex Buffer Lock Failed in %s on line "
            "%d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
    }

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT Lock(IDirect3DVertexBuffer9 *vb, size_t offset,
                                  size_t size, void **ptr, DWORD flags,
                                  LockedBufferContext *lb) {
    // asynchronous write-only dynamic vb lock
    if (ASyncMode()) {
      AsynchronousLock(vb, offset, size, ptr, flags, lb);
      return S_OK;
    }

    HRESULT hr = vb->Lock(offset, size, ptr, flags);
    switch (hr) {
      case S_OK:
        break;
      case D3DERR_INVALIDCALL:
        Warning(
            "D3DERR_INVALIDCALL - Vertex Buffer Lock Failed in %s on line "
            "%d(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_DRIVERINTERNALERROR:
        Warning(
            "D3DERR_DRIVERINTERNALERROR - Vertex Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        Warning(
            "D3DERR_OUTOFVIDEOMEMORY - Vertex Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      default:
        Warning(
            "(0x%.8x) - Vertex Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
    }

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT Lock(IDirect3DIndexBuffer9 *ib, size_t offset,
                                  size_t size, void **ptr, DWORD flags) {
    Synchronize();

    HRESULT hr = ib->Lock(offset, size, ptr, flags);
    switch (hr) {
      case S_OK:
        break;
      case D3DERR_INVALIDCALL:
        Warning(
            "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line "
            "%d(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_DRIVERINTERNALERROR:
        Warning(
            "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        Warning(
            "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line "
            "%d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      default:
        Warning(
            "(0x%.8x) - Index Buffer Lock Failed in %s on line "
            "%d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
    }

    return hr;
  }

  // asycnhronous lock of index buffer
  SOURCE_FORCEINLINE HRESULT Lock(IDirect3DIndexBuffer9 *ib, size_t offset,
                                  size_t size, void **ptr, DWORD flags,
                                  LockedBufferContext *lb) {
    if (ASyncMode()) {
      AsynchronousLock(ib, offset, size, ptr, flags, lb);
      return S_OK;
    }

    HRESULT hr = ib->Lock(offset, size, ptr, flags);
    switch (hr) {
      case S_OK:
        break;
      case D3DERR_INVALIDCALL:
        Warning(
            "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line "
            "%d(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_DRIVERINTERNALERROR:
        Warning(
            "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        Warning(
            "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
      default:
        Warning(
            "(0x%.8x) - Index Buffer Lock Failed in %s on "
            "line %d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags);
        break;
    }

    return hr;
  }

  SOURCE_FORCEINLINE ULONG Release(IDirect3DIndexBuffer9 *ib) {
    Synchronize();
    return ib->Release();
  }

  SOURCE_FORCEINLINE ULONG Release(IDirect3DVertexBuffer9 *vb) {
    Synchronize();
    return vb->Release();
  }

  SOURCE_FORCEINLINE HRESULT Unlock(IDirect3DVertexBuffer9 *vb) {
    if (ASyncMode()) {
      Push(PBCMD_UNLOCK_VB, vb);
      return S_OK;
    }

    HRESULT hr = vb->Unlock();
    if (SUCCEEDED(hr)) return hr;

    Warning("Vertex Buffer Unlock Failed in %s on line %d\n",
            V_UnqualifiedFileName(__FILE__), __LINE__);

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT Unlock(IDirect3DVertexBuffer9 *vb,
                                    LockedBufferContext *lb,
                                    size_t unlock_size) {
    if (ASyncMode()) {
      AllocatePushBufferSpace(1 + N_DWORDS_IN_PTR +
                              N_DWORDS(LockedBufferContext) + 1);
      *(output_ptr_++) = PBCMD_ASYNC_UNLOCK_VB;
      *((IDirect3DVertexBuffer9 **)output_ptr_) = vb;
      output_ptr_ += N_DWORDS_IN_PTR;
      *((LockedBufferContext *)output_ptr_) = *lb;
      output_ptr_ += N_DWORDS(LockedBufferContext);
      *(output_ptr_++) = unlock_size;
      return S_OK;
    }

    HRESULT hr = vb->Unlock();
    if (SUCCEEDED(hr)) return hr;

    Warning("Vertex Buffer Unlock Failed in %s on line %d\n",
            V_UnqualifiedFileName(__FILE__), __LINE__);

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT Unlock(IDirect3DIndexBuffer9 *ib) {
    if (ASyncMode()) {
      Push(PBCMD_UNLOCK_IB, ib);
      return S_OK;
    }

    HRESULT hr = ib->Unlock();
    if (SUCCEEDED(hr)) return hr;

    Warning("Index Buffer Unlock Failed in %s on line %d\n",
            V_UnqualifiedFileName(__FILE__), __LINE__);

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT Unlock(IDirect3DIndexBuffer9 *ib,
                                    LockedBufferContext *lb,
                                    size_t unlock_size) {
    if (ASyncMode()) {
      AllocatePushBufferSpace(1 + N_DWORDS_IN_PTR +
                              N_DWORDS(LockedBufferContext) + 1);
      *(output_ptr_++) = PBCMD_ASYNC_UNLOCK_IB;
      *((IDirect3DIndexBuffer9 **)output_ptr_) = ib;
      output_ptr_ += N_DWORDS_IN_PTR;
      *((LockedBufferContext *)output_ptr_) = *lb;
      output_ptr_ += N_DWORDS(LockedBufferContext);
      *(output_ptr_++) = unlock_size;
      return S_OK;
    }

    HRESULT hr = ib->Unlock();
    if (SUCCEEDED(hr)) return hr;

    Warning("Index Buffer Unlock Failed in %s on line %d\n",
            V_UnqualifiedFileName(__FILE__), __LINE__);

    return hr;
  }

  SOURCE_FORCEINLINE HRESULT ShowCursor(BOOL onoff) {
    Synchronize();
    DO_D3D(ShowCursor(onoff));
  }

  SOURCE_FORCEINLINE HRESULT Clear(int count, D3DRECT const *pRects, int Flags,
                                   D3DCOLOR color, float Z, int stencil) {
    if (ASyncMode()) {
      int n_rects_words = count * N_DWORDS(D3DRECT);
      AllocatePushBufferSpace(2 + n_rects_words + 4);
      *(output_ptr_++) = PBCMD_CLEAR;
      *(output_ptr_++) = count;
      if (count) {
        memcpy(output_ptr_, pRects, count * sizeof(D3DRECT));
        output_ptr_ += n_rects_words;
      }
      *(output_ptr_++) = Flags;
      *((D3DCOLOR *)output_ptr_) = color;
      output_ptr_++;
      *((float *)output_ptr_) = Z;
      output_ptr_++;
      *(output_ptr_++) = stencil;
      return S_OK;
    }

    DO_D3D(Clear(count, pRects, Flags, color, Z, stencil));
  }

  SOURCE_FORCEINLINE HRESULT Reset(D3DPRESENT_PARAMETERS *parms) {
    RECORD_COMMAND(DX8_RESET, 1);
    RECORD_STRUCT(parms, sizeof(*parms));

    Synchronize();
    DO_D3D(Reset(parms));
  }

  SOURCE_FORCEINLINE ULONG Release() {
    Synchronize();
    return Dx9Device()->Release();
  }

  SOURCE_FORCEINLINE HRESULT SetTexture(int stage,
                                        IDirect3DBaseTexture9 *txtr) {
    RECORD_COMMAND(DX8_SET_TEXTURE, 3);
    RECORD_INT(stage);
    RECORD_INT(-1);
    RECORD_INT(-1);

    if (ASyncMode()) {
      Push(PBCMD_SET_TEXTURE, stage, txtr);
      return S_OK;
    }

    DO_D3D(SetTexture(stage, txtr));
  }

  SOURCE_FORCEINLINE HRESULT SetTransform(D3DTRANSFORMSTATETYPE mtrx_id,
                                          D3DXMATRIX const *mt) {
    RECORD_COMMAND(DX8_SET_TRANSFORM, 2);
    RECORD_INT(mtrx_id);
    RECORD_STRUCT(mt, sizeof(D3DXMATRIX));

    Synchronize();
    DO_D3D(SetTransform(mtrx_id, mt));
  }

  SOURCE_FORCEINLINE HRESULT SetSamplerState(int stage,
                                             D3DSAMPLERSTATETYPE state,
                                             DWORD val) {
    RECORD_SAMPLER_STATE(stage, state, val);

    if (ASyncMode()) {
      Push(PBCMD_SET_SAMPLER_STATE, stage, state, val);
      return S_OK;
    }

    DO_D3D(SetSamplerState(stage, state, val));
  }

  SOURCE_FORCEINLINE HRESULT SetFVF(u32 fvf) {
    Synchronize();
    DO_D3D(SetFVF(fvf));
  }

  SOURCE_FORCEINLINE HRESULT
  SetTextureStageState(int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val) {
    RECORD_TEXTURE_STAGE_STATE(stage, state, val);
    Synchronize();
    DO_D3D(SetTextureStageState(stage, state, val));
  }

  SOURCE_FORCEINLINE HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,
                                           UINT StartVertex,
                                           UINT PrimitiveCount) {
    RECORD_COMMAND(DX8_DRAW_PRIMITIVE, 3);
    RECORD_INT(PrimitiveType);
    RECORD_INT(StartVertex);
    RECORD_INT(PrimitiveCount);

    if (ASyncMode()) {
      Push(PBCMD_DRAWPRIM, PrimitiveType, StartVertex, PrimitiveCount);
      SubmitIfNotBusy();
      return S_OK;
    }

    DO_D3D(DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount));
  }

  HRESULT CreateVertexDeclaration(CONST D3DVERTEXELEMENT9 *pVertexElements,
                                  IDirect3DVertexDeclaration9 **ppDecl) {
    Synchronize();
    DO_D3D(CreateVertexDeclaration(pVertexElements, ppDecl));
  }

  HRESULT ValidateDevice(DWORD *pNumPasses) {
    Synchronize();
    DO_D3D(ValidateDevice(pNumPasses));
  }

  HRESULT CreateVertexShader(CONST DWORD *pFunction,
                             IDirect3DVertexShader9 **ppShader) {
    Synchronize();
    DO_D3D(CreateVertexShader(pFunction, ppShader));
  }

  HRESULT CreatePixelShader(CONST DWORD *pFunction,
                            IDirect3DPixelShader9 **ppShader) {
    Synchronize();
    DO_D3D(CreatePixelShader(pFunction, ppShader));
  }

  SOURCE_FORCEINLINE HRESULT SetIndices(IDirect3DIndexBuffer9 *pIndexData) {
    if (ASyncMode()) {
      Push(PBCMD_SET_INDICES, pIndexData);
      return S_OK;
    }
    DO_D3D(SetIndices(pIndexData));
  }

  SOURCE_FORCEINLINE HRESULT
  SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData,
                  UINT OffsetInBytes, UINT Stride) {
    if (ASyncMode()) {
      Push(PBCMD_SET_STREAM_SOURCE, StreamNumber, pStreamData, OffsetInBytes,
           Stride);
      return S_OK;
    }

    DO_D3D(SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride));
  }

  HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                             IDirect3DVertexBuffer9 **ppVertexBuffer,
                             HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer,
                              pSharedHandle));
  }

  HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                            D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer,
                            HANDLE *pSharedHandle) {
    Synchronize();
    DO_D3D(CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer,
                             pSharedHandle));
  }

  SOURCE_FORCEINLINE HRESULT DrawIndexedPrimitive(
      D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinIndex,
      UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) {
    RECORD_COMMAND(DX8_DRAW_INDEXED_PRIMITIVE, 6);
    RECORD_INT(Type);
    RECORD_INT(BaseVertexIndex);
    RECORD_INT(MinIndex);
    RECORD_INT(NumVertices);
    RECORD_INT(StartIndex);
    RECORD_INT(PrimitiveCount);

    if (ASyncMode()) {
      Push(PBCMD_DRAWINDEXEDPRIM, Type, BaseVertexIndex, MinIndex, NumVertices,
           StartIndex, PrimitiveCount);
      // SubmitIfNotBusy();
      return S_OK;
    }

    DO_D3D(DrawIndexedPrimitive(Type, BaseVertexIndex, MinIndex, NumVertices,
                                StartIndex, PrimitiveCount));
  }

  HRESULT SetMaterial(D3DMATERIAL9 const *mat) {
    RECORD_COMMAND(DX8_SET_MATERIAL, 1);
    RECORD_STRUCT(&mat, sizeof(mat));
    Synchronize();
    DO_D3D(SetMaterial(mat));
  }

  SOURCE_FORCEINLINE HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader) {
    RECORD_COMMAND(DX8_SET_PIXEL_SHADER, 1);
    RECORD_PTR(pShader);

    if (ASyncMode()) {
      Push(PBCMD_SET_PIXEL_SHADER, pShader);
      return S_OK;
    }

    DO_D3D(SetPixelShader(pShader));
  }

  SOURCE_FORCEINLINE HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader) {
    if (ASyncMode()) {
      Push(PBCMD_SET_VERTEX_SHADER, pShader);
      return S_OK;
    }

    DO_D3D(SetVertexShader(pShader));
  }

  HRESULT EvictManagedResources() {
    if (Dx9Device()) {
      // people call this before creating the device
      Synchronize();
      DO_D3D(EvictManagedResources());
    }
    return S_OK;
  }

  HRESULT SetLight(int i, D3DLIGHT9 const *l) {
    RECORD_COMMAND(DX8_SET_LIGHT, 2);
    RECORD_INT(i);
    RECORD_STRUCT(l, sizeof(*l));

    Synchronize();
    DO_D3D(SetLight(i, l));
  }

  HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                 UINT MinVertexIndex, UINT NumVertices,
                                 UINT PrimitiveCount, CONST void *pIndexData,
                                 D3DFORMAT IndexDataFormat,
                                 CONST void *pVertexStreamZeroData,
                                 UINT VertexStreamZeroStride) {
    Synchronize();
    DO_D3D(DrawIndexedPrimitiveUP(
        PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData,
        IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride));
  }

  HRESULT Present(CONST RECT *pSourceRect, CONST RECT *pDestRect,
                  HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion) {
    RECORD_COMMAND(DX8_PRESENT, 0);

    if (ASyncMode()) {
      // need to deal with ret code here
      AllocatePushBufferSpace(1 + 1 + N_DWORDS(RECT) + 1 + N_DWORDS(RECT) + 1 +
                              1 + N_DWORDS(RGNDATA));
      *(output_ptr_++) = PBCMD_PRESENT;
      *(output_ptr_++) = (pSourceRect != NULL);
      if (pSourceRect) memcpy(output_ptr_, pSourceRect, sizeof(RECT));
      output_ptr_ += N_DWORDS(RECT);
      *(output_ptr_++) = (pDestRect != NULL);
      if (pDestRect) memcpy(output_ptr_, pDestRect, sizeof(RECT));
      output_ptr_ += N_DWORDS(RECT);
      *(output_ptr_++) = (u32)hDestWindowOverride;
      *(output_ptr_++) = (pDirtyRegion != NULL);
      if (pDirtyRegion) memcpy(output_ptr_, pDirtyRegion, sizeof(RGNDATA));
      output_ptr_ += N_DWORDS(RGNDATA);
      // not good - caller wants to here about lost devices
      return S_OK;
    }

    DO_D3D(Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion));
  }

 private:
  IDirect3DDevice9 *d3d9_device_;
  uintptr_t async_thread_handle_;
  PushBuffer *current_push_buffer_;
  u32 *output_ptr_;
  size_t push_buffer_free_slots_;

  PushBuffer *FindFreePushBuffer(PushBufferState newstate);  // find a free push
                                                             // buffer and
                                                             // change its state

  void GetPushBuffer();  // set us up to point at a new push buffer
  void SubmitPushBufferAndGetANewOne();  // submit the current push buffer
  void ExecutePushBuffer(PushBuffer const *pb);

  void Synchronize();  // wait for all commands to be done

  void SubmitIfNotBusy();

  template <class T>
  SOURCE_FORCEINLINE void PushStruct(PushBufferCommand cmd, T const *str) {
    int nwords = N_DWORDS(T);
    AllocatePushBufferSpace(1 + nwords);
    output_ptr_[0] = cmd;
    memcpy(output_ptr_ + 1, str, sizeof(T));
    output_ptr_ += 1 + nwords;
  }

  SOURCE_FORCEINLINE void AllocatePushBufferSpace(size_t nSlots) {
    // check for N slots of space, and decrement amount of space left
    if (nSlots > push_buffer_free_slots_)  // out of room?
    {
      SubmitPushBufferAndGetANewOne();
    }
    push_buffer_free_slots_ -= nSlots;
  }

  // simple methods for pushing a few words into output buffer
  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd) {
    AllocatePushBufferSpace(1);
    output_ptr_[0] = cmd;
    output_ptr_++;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, int arg1) {
    AllocatePushBufferSpace(2);
    output_ptr_[0] = cmd;
    output_ptr_[1] = arg1;
    output_ptr_ += 2;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, void *ptr) {
    AllocatePushBufferSpace(1 + N_DWORDS_IN_PTR);
    *(output_ptr_++) = cmd;
    *((void **)output_ptr_) = ptr;
    output_ptr_ += N_DWORDS_IN_PTR;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, void *ptr, void *ptr1) {
    AllocatePushBufferSpace(1 + 2 * N_DWORDS_IN_PTR);
    *(output_ptr_++) = cmd;
    *((void **)output_ptr_) = ptr;
    output_ptr_ += N_DWORDS_IN_PTR;
    *((void **)output_ptr_) = ptr1;
    output_ptr_ += N_DWORDS_IN_PTR;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, void *arg1, u32 arg2,
                               u32 arg3, u32 arg4, void *arg5) {
    AllocatePushBufferSpace(1 + N_DWORDS_IN_PTR + 1 + 1 + 1 + N_DWORDS_IN_PTR);
    *(output_ptr_++) = cmd;
    *((void **)output_ptr_) = arg1;
    output_ptr_ += N_DWORDS_IN_PTR;
    *(output_ptr_++) = arg2;
    *(output_ptr_++) = arg3;
    *(output_ptr_++) = arg4;
    *((void **)output_ptr_) = arg5;
    output_ptr_ += N_DWORDS_IN_PTR;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, u32 arg1, void *ptr) {
    AllocatePushBufferSpace(2 + N_DWORDS_IN_PTR);
    *(output_ptr_++) = cmd;
    *(output_ptr_++) = arg1;
    *((void **)output_ptr_) = ptr;
    output_ptr_ += N_DWORDS_IN_PTR;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, u32 arg1, void *ptr,
                               int arg2, int arg3) {
    AllocatePushBufferSpace(4 + N_DWORDS_IN_PTR);
    *(output_ptr_++) = cmd;
    *(output_ptr_++) = arg1;
    *((void **)output_ptr_) = ptr;
    output_ptr_ += N_DWORDS_IN_PTR;
    output_ptr_[0] = arg2;
    output_ptr_[1] = arg3;
    output_ptr_ += 2;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, int arg1, int arg2) {
    AllocatePushBufferSpace(3);
    output_ptr_[0] = cmd;
    output_ptr_[1] = arg1;
    output_ptr_[2] = arg2;
    output_ptr_ += 3;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, int arg1, int arg2,
                               int arg3) {
    AllocatePushBufferSpace(4);
    output_ptr_[0] = cmd;
    output_ptr_[1] = arg1;
    output_ptr_[2] = arg2;
    output_ptr_[3] = arg3;
    output_ptr_ += 4;
  }

  SOURCE_FORCEINLINE void Push(PushBufferCommand cmd, int arg1, int arg2,
                               int arg3, int arg4, int arg5, int arg6) {
    AllocatePushBufferSpace(7);
    output_ptr_[0] = cmd;
    output_ptr_[1] = arg1;
    output_ptr_[2] = arg2;
    output_ptr_[3] = arg3;
    output_ptr_[4] = arg4;
    output_ptr_[5] = arg5;
    output_ptr_[6] = arg6;
    output_ptr_ += 7;
  }

  SOURCE_FORCEINLINE bool ASyncMode() const {
    return async_thread_handle_ != 0;
  }

  SOURCE_FORCEINLINE IDirect3DDevice9 *Dx9Device() const {
    return d3d9_device_;
  }

  void AsynchronousLock(IDirect3DVertexBuffer9 *vb, size_t offset, size_t size,
                        void **ptr, DWORD flags, LockedBufferContext *lb);

  void AsynchronousLock(IDirect3DIndexBuffer9 *ib, size_t offset, size_t size,
                        void **ptr, DWORD flags, LockedBufferContext *lb);

  // handlers for push buffer contexts
  void HandleAsynchronousLockVBCommand(u32 const *dptr);
  void HandleAsynchronousUnLockVBCommand(u32 const *dptr);
  void HandleAsynchronousLockIBCommand(u32 const *dptr);
  void HandleAsynchronousUnLockIBCommand(u32 const *dptr);
};

#endif  // MATERIALSYSTEM_SHADERAPIDX9_D3DASYNC_H_
