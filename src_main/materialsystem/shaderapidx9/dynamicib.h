// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com

#ifndef MATERIALSYSTEM_SHADERAPIDX9_DYNAMICIB_H_
#define MATERIALSYSTEM_SHADERAPIDX9_DYNAMICIB_H_

#include "Recording.h"
#include "ShaderAPIDX8_Global.h"
#include "locald3dtypes.h"
#include "shaderapi/IShaderUtil.h"

#include "locald3dtypes.h"
#include "tier0/include/memdbgon.h"
#include "tier1/strtools.h"
#include "tier1/utlqueue.h"

// Helper function to unbind an index buffer
void Unbind(IDirect3DIndexBuffer9 *pIndexBuffer);

class CIndexBuffer {
 public:
  CIndexBuffer(Direct3DDevice9Wrapper *d3d9, int count,
               bool bSoftwareVertexProcessing, bool dynamic = false)
      : m_pIB(0),
        m_Position(0),
        m_bFlush(true),
        m_bLocked(false),
        m_bDynamic(dynamic)
#ifdef VPROF_ENABLED
        ,
        m_Frame(-1)
#endif
  {
    // For write-combining, ensure we always have locked memory aligned to
    // 4-byte boundaries
    count = AlignValue(count, 2);
    m_IndexCount = count;

    MEM_ALLOC_D3D_CREDIT();

#ifdef CHECK_INDICES
    m_pShadowIndices = nullptr;
#endif

#ifdef RECORDING
    // assign a UID
    static unsigned int uid = 0;
    m_UID = uid++;
#endif

#ifdef _DEBUG
    ++s_BufferCount;
#endif

    D3DINDEXBUFFER_DESC desc;
    memset(&desc, 0x00, sizeof(desc));
    desc.Format = D3DFMT_INDEX16;
    desc.Size = sizeof(unsigned short) * count;
    desc.Type = D3DRTYPE_INDEXBUFFER;
    desc.Pool = D3DPOOL_DEFAULT;

    desc.Usage = D3DUSAGE_WRITEONLY;
    if (m_bDynamic) {
      desc.Usage |= D3DUSAGE_DYNAMIC;
    }
    if (bSoftwareVertexProcessing) {
      desc.Usage |= D3DUSAGE_SOFTWAREPROCESSING;
    }

    RECORD_COMMAND(DX8_CREATE_INDEX_BUFFER, 6);
    RECORD_INT(m_UID);
    RECORD_INT(count * IndexSize());
    RECORD_INT(desc.Usage);
    RECORD_INT(desc.Format);
    RECORD_INT(desc.Pool);
    RECORD_INT(m_bDynamic);

#ifdef CHECK_INDICES
    Assert(desc.Format == D3DFMT_INDEX16);
    m_pShadowIndices = new unsigned short[count];
    m_NumIndices = count;
#endif

    HRESULT hr =
        d3d9->CreateIndexBuffer(count * IndexSize(), desc.Usage, desc.Format,
                                desc.Pool, &m_pIB, nullptr);
    if (hr != D3D_OK) {
      Warning("DynamicIndexBuffer: CreateIndexBuffer failed (0x%.8x).\n", hr);

      if (hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY) {
        // Don't have the memory for this. Try flushing all managed resources
        // out of vid mem and try again.
        // TODO(d.rattman): need to record this.
        hr = d3d9->EvictManagedResources();

        if (SUCCEEDED(hr)) {
          hr = d3d9->CreateIndexBuffer(count * IndexSize(), desc.Usage,
                                       desc.Format, desc.Pool, &m_pIB, nullptr);
        }
      }
    }

    Assert(m_pIB);
    Assert(hr == D3D_OK);

#ifdef MEASURE_DRIVER_ALLOCATIONS
    int nMemUsed = 1024;
    VPROF_INCREMENT_GROUP_COUNTER("ib count", COUNTER_GROUP_NO_RESET, 1);
    VPROF_INCREMENT_GROUP_COUNTER("ib driver mem", COUNTER_GROUP_NO_RESET,
                                  nMemUsed);
    VPROF_INCREMENT_GROUP_COUNTER("total driver mem", COUNTER_GROUP_NO_RESET,
                                  nMemUsed);
#endif

#if defined(_DEBUG)
    if (m_pIB) {
      D3DINDEXBUFFER_DESC real_desc;
      Assert(SUCCEEDED(m_pIB->GetDesc(&real_desc)));
      Assert(memcmp(&real_desc, &desc, sizeof(desc)) == 0);
    }
#endif

#ifdef VPROF_ENABLED
    if (!m_bDynamic) {
      VPROF_INCREMENT_GROUP_COUNTER(
          "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER,
          COUNTER_GROUP_TEXTURE_GLOBAL, IndexCount() * IndexSize());
    }
#endif
  }

  ~CIndexBuffer() {
#ifdef _DEBUG
    --s_BufferCount;
#endif

    Unlock(0);

#ifdef CHECK_INDICES
    if (m_pShadowIndices) {
      delete[] m_pShadowIndices;
      m_pShadowIndices = nullptr;
    }
#endif

#ifdef MEASURE_DRIVER_ALLOCATIONS
    int nMemUsed = 1024;
    VPROF_INCREMENT_GROUP_COUNTER("ib count", COUNTER_GROUP_NO_RESET, -1);
    VPROF_INCREMENT_GROUP_COUNTER("ib driver mem", COUNTER_GROUP_NO_RESET,
                                  -nMemUsed);
    VPROF_INCREMENT_GROUP_COUNTER("total driver mem", COUNTER_GROUP_NO_RESET,
                                  -nMemUsed);
#endif

    if (m_pIB) {
      RECORD_COMMAND(DX8_DESTROY_INDEX_BUFFER, 1);
      RECORD_INT(m_UID);

      Dx9Device()->Release(m_pIB);
    }

#ifdef VPROF_ENABLED
    if (!m_bDynamic) {
      VPROF_INCREMENT_GROUP_COUNTER(
          "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER,
          COUNTER_GROUP_TEXTURE_GLOBAL, -IndexCount() * IndexSize());
    }
#endif
  }

  LPDIRECT3DINDEXBUFFER GetInterface() const { return m_pIB; }

  // Use at beginning of frame to force a flush of VB contents on first draw.
  void FlushAtFrameStart() { m_bFlush = true; }

  // lock, unlock
  unsigned short *Lock(bool bReadOnly, int numIndices, int &startIndex,
                       int startPosition = -1) {
    Assert(!m_bLocked);

    unsigned short *pLockedData = nullptr;

    // For write-combining, ensure we always have locked memory aligned to
    // 4-byte boundaries
    if (m_bDynamic) numIndices = AlignValue(numIndices, 2);

    // Ensure there is enough space in the IB for this data
    if (numIndices > m_IndexCount) {
      Error("Too many indices for index buffer. Tell a programmer (%d>%d)\n",
            numIndices, m_IndexCount);
      Assert(false);
      return 0;
    }

    if (!m_pIB) return 0;

    DWORD dwFlags;

    if (m_bDynamic) {
      Assert(startPosition < 0);
      dwFlags = LOCKFLAGS_APPEND;

      // If either user forced us to flush,
      // or there is not enough space for the vertex data,
      // then flush the buffer contents
      // xbox must not append at position 0 because nooverwrite cannot be
      // guaranteed

      if (!m_Position || m_bFlush || !HasEnoughRoom(numIndices)) {
        m_bFlush = false;
        m_Position = 0;

        dwFlags = LOCKFLAGS_FLUSH;
      }
    } else {
      dwFlags = D3DLOCK_NOSYSLOCK;
    }

    if (bReadOnly) {
      dwFlags |= D3DLOCK_READONLY;
    }

    int position = m_Position;
    if (startPosition >= 0) {
      position = startPosition;
    }

    RECORD_COMMAND(DX8_LOCK_INDEX_BUFFER, 4);
    RECORD_INT(m_UID);
    RECORD_INT(position * IndexSize());
    RECORD_INT(numIndices * IndexSize());
    RECORD_INT(dwFlags);

#ifdef CHECK_INDICES
    m_LockedStartIndex = position;
    m_LockedNumIndices = numIndices;
#endif

    HRESULT hr =
        m_bDynamic
            ? Dx9Device()->Lock(
                  m_pIB, position * IndexSize(), numIndices * IndexSize(),
                  reinterpret_cast<void **>(&pLockedData), dwFlags, &m_LockData)
            : Dx9Device()->Lock(
                  m_pIB, position * IndexSize(), numIndices * IndexSize(),
                  reinterpret_cast<void **>(&pLockedData), dwFlags);

    switch (hr) {
      case D3DERR_INVALIDCALL:
        Warning(
            "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line "
            "%d(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(),
            numIndices * IndexSize(), dwFlags);
        break;
      case D3DERR_DRIVERINTERNALERROR:
        Warning(
            "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on "
            "line "
            "%d (offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(),
            numIndices * IndexSize(), dwFlags);
        break;
      case D3DERR_OUTOFVIDEOMEMORY:
        Warning(
            "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line "
            "%d "
            "(offset %d, size %d, flags 0x%x)\n",
            V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(),
            numIndices * IndexSize(), dwFlags);
        break;
    }

    Assert(pLockedData != nullptr);

    startIndex = position;

    Assert(m_bLocked == false);
    m_bLocked = true;

    return pLockedData;
  }

  void Unlock(int numIndices) {
    Assert((m_Position + numIndices) <= m_IndexCount);

    if (!m_bLocked) return;

    // For write-combining, ensure we always have locked memory aligned to
    // 4-byte boundaries
    if (m_bDynamic) AlignValue(numIndices, 2);

    if (!m_pIB) return;

    RECORD_COMMAND(DX8_UNLOCK_INDEX_BUFFER, 1);
    RECORD_INT(m_UID);

#ifdef CHECK_INDICES
    m_LockedStartIndex = 0;
    m_LockedNumIndices = 0;
#endif

    if (m_bDynamic) {
      Dx9Device()->Unlock(m_pIB, &m_LockData, numIndices * IndexSize());
    } else {
      Dx9Device()->Unlock(m_pIB);
    }

    m_Position += numIndices;
    m_bLocked = false;
  }

  // Index position
  int IndexPosition() const { return m_Position; }

  // Index size
  int IndexSize() const { return sizeof(unsigned short); }

  // Index count
  int IndexCount() const { return m_IndexCount; }

  // Do we have enough room without discarding?
  bool HasEnoughRoom(int numIndices) const {
    return (numIndices + m_Position) <= m_IndexCount;
  }

  bool IsDynamic() const { return m_bDynamic; }

  // Block until there's a free portion of the buffer of this size, m_Position
  // will be updated to point at where this section starts.
  void BlockUntilUnused(int nAllocationSize) {
    Assert(nAllocationSize <= m_IndexCount);
  }

#ifdef CHECK_INDICES
  void UpdateShadowIndices(unsigned short *pData) {
    Assert(m_LockedStartIndex + m_LockedNumIndices <= m_NumIndices);
    memcpy(m_pShadowIndices + m_LockedStartIndex, pData,
           m_LockedNumIndices * IndexSize());
  }

  unsigned short GetShadowIndex(int i) {
    Assert(i >= 0 && i < (int)m_NumIndices);
    return m_pShadowIndices[i];
  }
#endif

  // UID
  unsigned int UID() const {
#ifdef RECORDING
    return m_UID;
#else
    return 0;
#endif
  }

  void HandlePerFrameTextureStats(int frame) {
#ifdef VPROF_ENABLED
    if (m_Frame != frame && !m_bDynamic) {
      m_Frame = frame;
      VPROF_INCREMENT_GROUP_COUNTER(
          "TexGroup_frame_" TEXTURE_GROUP_STATIC_INDEX_BUFFER,
          COUNTER_GROUP_TEXTURE_PER_FRAME, IndexCount() * IndexSize());
    }
#endif
  }

  static int BufferCount() {
#ifdef _DEBUG
    return s_BufferCount;
#else
    return 0;
#endif
  }

  // Marks a fence indicating when this buffer was used
  void MarkUsedInRendering() {}

 private:
  enum LOCK_FLAGS {
    LOCKFLAGS_FLUSH = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
    LOCKFLAGS_APPEND = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE
  };

  LPDIRECT3DINDEXBUFFER m_pIB;

  int m_IndexCount;
  int m_Position;
  unsigned char m_bLocked : 1;
  unsigned char m_bFlush : 1;
  unsigned char m_bDynamic : 1;

#ifdef VPROF_ENABLED
  int m_Frame;
#endif

#ifdef _DEBUG
  static int s_BufferCount;
#endif

#ifdef RECORDING
  unsigned int m_UID;
#endif

  LockedBufferContext m_LockData;

 protected:
#ifdef CHECK_INDICES
  unsigned short *m_pShadowIndices;
  unsigned int m_NumIndices;
  unsigned int m_LockedStartIndex;
  unsigned int m_LockedNumIndices;
#endif
};

#ifdef _DEBUG
int CIndexBuffer::s_BufferCount = 0;
#endif

#include "tier0/include/memdbgoff.h"

#endif  // MATERIALSYSTEM_SHADERAPIDX9_DYNAMICIB_H_
