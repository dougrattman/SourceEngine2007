// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include "mem_std.h"

#include <malloc.h>
#include <algorithm>

#include "build/include/build_config.h"

#ifdef OS_WIN
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#endif

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"
#endif

#include "base/include/stdio_file_stream.h"
#include "mem_helpers.h"
#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/threadtools.h"

#ifdef TIME_ALLOC
CAverageCycleCounter g_MallocCounter;
CAverageCycleCounter g_ReallocCounter;
CAverageCycleCounter g_FreeCounter;

#define PrintOne(name)                                                        \
  Msg("%-48s: %6.4f avg (%8.1f total, %7.3f peak, %5d iters)\n", #name,       \
      g_##name##Counter.GetAverageMilliseconds(),                             \
      g_##name##Counter.GetTotalMilliseconds(),                               \
      g_##name##Counter.GetPeakMilliseconds(), g_##name##Counter.GetIters()); \
  memset(&g_##name##Counter, 0, sizeof(g_##name##Counter))

void PrintAllocTimes() {
  PrintOne(Malloc);
  PrintOne(Realloc);
  PrintOne(Free);
}

#define PROFILE_ALLOC(name) CAverageTimeMarker name##_ATM(&g_##name##Counter)

#else
#define PROFILE_ALLOC(name) ((void)0)
#define PrintAllocTimes() ((void)0)
#endif

#if (defined(NDEBUG) && !defined(USE_MEM_DEBUG))

// Singleton...

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4074)  // warning C4074: initializers put in compiler
                            // reserved initialization area
#pragma init_seg(compiler)
MSVC_END_WARNING_OVERRIDE_SCOPE()

static CStdMemAlloc s_StdMemAlloc CONSTRUCT_EARLY;

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_StdMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_StdMemAlloc;
#endif

#ifdef OS_WIN

// Small block heap (multi-pool)

#ifndef NO_SBH
#define UsingSBH() true
#else
#define UsingSBH() false
#endif

template <typename T>
inline T MemAlign(T val, usize alignment) {
  return (T)(((uintptr_t)val + alignment - 1) & ~(alignment - 1));
}

void CSmallBlockPool::Init(usize nBlockSize, u8 *pBase, usize initialCommit) {
  if (!(nBlockSize % MIN_SBH_ALIGN == 0 &&
        nBlockSize >= std::max(implicit_cast<usize>(MIN_SBH_BLOCK),
                               sizeof(TSLNodeBase_t))))
    DebuggerBreak();

  m_nBlockSize = nBlockSize;
  m_pCommitLimit = m_pNextAlloc = m_pBase = pBase;
  m_pAllocLimit = m_pBase + MAX_POOL_REGION;

  if (initialCommit) {
    initialCommit = MemAlign(initialCommit, PAGE_SIZE);
    if (!VirtualAlloc(m_pCommitLimit, initialCommit, VA_COMMIT_FLAGS,
                      PAGE_READWRITE)) {
      Assert(0);
      return;
    }
    m_pCommitLimit += initialCommit;
  }
}

usize CSmallBlockPool::GetBlockSize() { return m_nBlockSize; }

bool CSmallBlockPool::IsOwner(void *p) {
  return (p >= m_pBase && p < m_pAllocLimit);
}

void *CSmallBlockPool::Alloc() {
  void *pResult = m_FreeList.Pop();
  if (!pResult) {
    usize nBlockSize = m_nBlockSize;
    u8 *pCommitLimit, *pNextAlloc;
    for (;;) {
      pCommitLimit = m_pCommitLimit;
      pNextAlloc = m_pNextAlloc;
      if (pNextAlloc + nBlockSize <= pCommitLimit) {
        if (m_pNextAlloc.AssignIf(pNextAlloc, pNextAlloc + m_nBlockSize)) {
          pResult = pNextAlloc;
          break;
        }
      } else {
        AUTO_LOCK(m_CommitMutex);
        if (pCommitLimit == m_pCommitLimit) {
          if (pCommitLimit + COMMIT_SIZE <= m_pAllocLimit) {
            if (!VirtualAlloc(pCommitLimit, COMMIT_SIZE, VA_COMMIT_FLAGS,
                              PAGE_READWRITE)) {
              Assert(0);
              return nullptr;
            }

            m_pCommitLimit = pCommitLimit + COMMIT_SIZE;
          } else {
            return nullptr;
          }
        }
      }
    }
  }
  return pResult;
}

void CSmallBlockPool::Free(void *p) {
  Assert(IsOwner(p));

  m_FreeList.Push(p);
}

// Count the free blocks.
usize CSmallBlockPool::CountFreeBlocks() { return m_FreeList.Count(); }

// Size of committed memory managed by this heap:
usize CSmallBlockPool::GetCommittedSize() {
  usize totalSize = m_pCommitLimit - m_pBase;
  Assert(0 != m_nBlockSize);
  Assert(0 != (totalSize % GetBlockSize()));

  return totalSize;
}

// Return the total blocks memory is committed for in the heap
usize CSmallBlockPool::CountCommittedBlocks() {
  return GetCommittedSize() / GetBlockSize();
}

// Count the number of allocated blocks in the heap:
usize CSmallBlockPool::CountAllocatedBlocks() {
  return CountCommittedBlocks() -
         (CountFreeBlocks() +
          (m_pCommitLimit - (u8 *)m_pNextAlloc) / GetBlockSize());
}

usize CSmallBlockPool::Compact() {
  usize nBytesFreed = 0;
  if (m_FreeList.Count()) {
    usize nFree = CountFreeBlocks();
    FreeBlock_t **pSortArray = (FreeBlock_t **)malloc(
        nFree * sizeof(FreeBlock_t *));  // can't use new because will reenter

    if (!pSortArray) {
      return 0;
    }

    usize i = 0;
    while (i < nFree) {
      pSortArray[i++] = m_FreeList.Pop();
    }

    std::sort(pSortArray, pSortArray + nFree);

    u8 *pOldNextAlloc = m_pNextAlloc;

    for (i = nFree; i >= 1; i--) {
      if ((u8 *)pSortArray[i - 1] ==
          static_cast<u8 *>(m_pNextAlloc) - m_nBlockSize) {
        pSortArray[i - 1] = nullptr;
        m_pNextAlloc -= m_nBlockSize;
      } else {
        break;
      }
    }

    if (pOldNextAlloc != m_pNextAlloc) {
      u8 *pNewCommitLimit = MemAlign((u8 *)m_pNextAlloc, PAGE_SIZE);
      if (pNewCommitLimit < m_pCommitLimit) {
        nBytesFreed = m_pCommitLimit - pNewCommitLimit;
        VirtualFree(pNewCommitLimit, nBytesFreed, MEM_DECOMMIT);
        m_pCommitLimit = pNewCommitLimit;
      }
    }

    if (pSortArray[0]) {
      for (i = 0; i < nFree; i++) {
        if (!pSortArray[i]) {
          break;
        }
        m_FreeList.Push(pSortArray[i]);
      }
    }

    free(pSortArray);
  }

  return nBytesFreed;
}

//

#define GetInitialCommitForPool(i) 0

CSmallBlockHeap::CSmallBlockHeap() {
  if (!UsingSBH()) {
    return;
  }

  m_pBase = (u8 *)VirtualAlloc(nullptr, NUM_POOLS * MAX_POOL_REGION,
                               VA_RESERVE_FLAGS, PAGE_NOACCESS);
  m_pLimit = m_pBase + NUM_POOLS * MAX_POOL_REGION;

  // Build a lookup table used to find the correct pool based on size
  const i32 MAX_TABLE = MAX_SBH_BLOCK >> 2;
  i32 i = 0;
  usize nBytesElement = 0;
  u8 *pCurBase = m_pBase;
  CSmallBlockPool *pCurPool = nullptr;
  i32 iCurPool = 0;

  // Blocks sized 0 - 128 are in pools in increments of 8
  for (; i < 32; i++) {  //-V112
    if ((i + 1) % 2 == 1) {
      nBytesElement += 8;
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  // Blocks sized 129 - 256 are in pools in increments of 16
  for (; i < 64; i++) {
    if ((i + 1) % 4 == 1) {  //-V112
      nBytesElement += 16;
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  // Blocks sized 257 - 512 are in pools in increments of 32
  for (; i < 128; i++) {
    if ((i + 1) % 8 == 1) {
      nBytesElement += 32;  //-V112
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  // Blocks sized 513 - 768 are in pools in increments of 64
  for (; i < 192; i++) {
    if ((i + 1) % 16 == 1) {
      nBytesElement += 64;
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  // Blocks sized 769 - 1024 are in pools in increments of 128
  for (; i < 256; i++) {
    if ((i + 1) % 32 == 1) {  //-V112
      nBytesElement += 128;
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  // Blocks sized 1025 - 2048 are in pools in increments of 256
  for (; i < MAX_TABLE; i++) {
    if ((i + 1) % 64 == 1) {
      nBytesElement += 256;
      pCurPool = &m_Pools[iCurPool];
      pCurPool->Init(nBytesElement, pCurBase,
                     GetInitialCommitForPool(iCurPool));
      iCurPool++;
      m_PoolLookup[i] = pCurPool;
      pCurBase += MAX_POOL_REGION;
    } else {
      m_PoolLookup[i] = pCurPool;
    }
  }

  Assert(iCurPool == NUM_POOLS);
}

bool CSmallBlockHeap::ShouldUse(usize nBytes) {
  return (UsingSBH() && nBytes <= MAX_SBH_BLOCK);
}

bool CSmallBlockHeap::IsOwner(void *p) {
  return (UsingSBH() && p >= m_pBase && p < m_pLimit);
}

void *CSmallBlockHeap::Alloc(usize nBytes) {
  if (nBytes == 0) {
    nBytes = 1;
  }
  Assert(ShouldUse(nBytes));
  CSmallBlockPool *pPool = FindPool(nBytes);

  void *p = pPool->Alloc();
  if (p) {
    return p;
  }

  if (s_StdMemAlloc.CallAllocFailHandler(nBytes) >= nBytes) {
    p = pPool->Alloc();
    if (p) {
      return p;
    }
  }

  void *pRet = malloc(nBytes);
  if (!pRet) {
    s_StdMemAlloc.SetCRTAllocFailed(nBytes);
  }
  return pRet;
}

void *CSmallBlockHeap::Realloc(void *p, usize nBytes) {
  if (nBytes == 0) {
    nBytes = 1;
  }

  CSmallBlockPool *pOldPool = FindPool(p);
  CSmallBlockPool *pNewPool = (ShouldUse(nBytes)) ? FindPool(nBytes) : nullptr;

  if (pOldPool == pNewPool) {
    return p;
  }

  void *pNewBlock = nullptr;

  if (pNewPool) {
    pNewBlock = pNewPool->Alloc();

    if (!pNewBlock) {
      if (s_StdMemAlloc.CallAllocFailHandler(nBytes) >= nBytes) {
        pNewBlock = pNewPool->Alloc();
      }
    }
  }

  if (!pNewBlock) {
    pNewBlock = malloc(nBytes);
    if (!pNewBlock) {
      s_StdMemAlloc.SetCRTAllocFailed(nBytes);
    }
  }

  if (pNewBlock) {
    usize nBytesCopy = std::min(nBytes, pOldPool->GetBlockSize());
    memcpy(pNewBlock, p, nBytesCopy);
  }

  pOldPool->Free(p);

  return pNewBlock;
}

void CSmallBlockHeap::Free(void *p) {
  CSmallBlockPool *pPool = FindPool(p);
  pPool->Free(p);
}

usize CSmallBlockHeap::GetSize(void *p) {
  CSmallBlockPool *pPool = FindPool(p);
  return pPool->GetBlockSize();
}

void CSmallBlockHeap::DumpStats(source::stdio_file_stream file) {
  file.print("Small Block Heap Stats:\n");

  for (i32 i = 0; i < NUM_POOLS; i++) {
    // output for vxconsole parsing
    file.print(
        "Pool %i: Size: %zu Allocated: %zu Free: %zu Committed: %zu "
        "CommittedSize: %zu\n",
        i, m_Pools[i].GetBlockSize(), m_Pools[i].CountAllocatedBlocks(),
        m_Pools[i].CountFreeBlocks(), m_Pools[i].CountCommittedBlocks(),
        m_Pools[i].GetCommittedSize());
  }
}

usize CSmallBlockHeap::Compact() {
  usize nBytesFreed = 0;
  for (i32 i = 0; i < NUM_POOLS; i++) {
    nBytesFreed += m_Pools[i].Compact();
  }
  return nBytesFreed;
}

CSmallBlockPool *CSmallBlockHeap::FindPool(usize nBytes) {
  return m_PoolLookup[(nBytes - 1) >> 2];
}

CSmallBlockPool *CSmallBlockHeap::FindPool(void *p) {
  usize i = ((u8 *)p - m_pBase) / MAX_POOL_REGION;
  return &m_Pools[i];
}

#endif

// Release versions

void *CStdMemAlloc::Alloc(usize nSize) {
  PROFILE_ALLOC(Malloc);

#ifdef OS_WIN
  if (m_SmallBlockHeap.ShouldUse(nSize)) {
    void *pMem = m_SmallBlockHeap.Alloc(nSize);
    ApplyMemoryInitializations(pMem, nSize);
    return pMem;
  }
#endif

  void *pMem = malloc(nSize);
  ApplyMemoryInitializations(pMem, nSize);
  if (!pMem) {
    SetCRTAllocFailed(nSize);
  }
  return pMem;
}

void *CStdMemAlloc::Realloc(void *pMem, usize nSize) {
  if (!pMem) {
    return Alloc(nSize);
  }

  PROFILE_ALLOC(Realloc);

#ifdef OS_WIN
  if (m_SmallBlockHeap.IsOwner(pMem)) {
    return m_SmallBlockHeap.Realloc(pMem, nSize);
  }
#endif

  void *pRet = realloc(pMem, nSize);
  if (!pRet) {
    SetCRTAllocFailed(nSize);
  }
  return pRet;
}

void CStdMemAlloc::Free(void *pMem) {
  if (!pMem) {
    return;
  }

  PROFILE_ALLOC(Free);

#ifdef OS_WIN
  if (m_SmallBlockHeap.IsOwner(pMem)) {
    m_SmallBlockHeap.Free(pMem);
    return;
  }
#endif

  free(pMem);
}

void *CStdMemAlloc::Expand_NoLongerSupported(void *pMem, usize nSize) {
  return nullptr;
}

// Debug versions

void *CStdMemAlloc::Alloc(usize nSize, const ch *pFileName, i32 nLine) {
  return CStdMemAlloc::Alloc(nSize);
}

void *CStdMemAlloc::Realloc(void *pMem, usize nSize, const ch *pFileName,
                            i32 nLine) {
  return CStdMemAlloc::Realloc(pMem, nSize);
}

void CStdMemAlloc::Free(void *pMem, const ch *pFileName, i32 nLine) {
  CStdMemAlloc::Free(pMem);
}

void *CStdMemAlloc::Expand_NoLongerSupported(void *pMem, usize nSize,
                                             const ch *pFileName, i32 nLine) {
  return nullptr;
}

// Returns size of a particular allocation

usize CStdMemAlloc::GetSize(void *pMem) {
#ifdef OS_WIN
  if (!pMem) return CalcHeapUsed();

  if (m_SmallBlockHeap.IsOwner(pMem)) return m_SmallBlockHeap.GetSize(pMem);

  return _msize(pMem);
#elif OS_POSIX
  Assert("GetSize() not implemented");
#endif
}

// Force file + line information for an allocation

void CStdMemAlloc::PushAllocDbgInfo(const ch *pFileName, i32 nLine) {}

void CStdMemAlloc::PopAllocDbgInfo() {}

// TODO(d.rattman): Remove when we make our own heap! Crt stuff we're currently
// using

long CStdMemAlloc::CrtSetBreakAlloc(long lNewBreakAlloc) { return 0; }

i32 CStdMemAlloc::CrtSetReportMode(i32 nReportType, i32 nReportMode) {
  return 0;
}

i32 CStdMemAlloc::CrtIsValidHeapPointer(const void *pMem) { return 1; }

i32 CStdMemAlloc::CrtIsValidPointer(const void *pMem, u32 size, i32 access) {
  return 1;
}

i32 CStdMemAlloc::CrtCheckMemory() { return 1; }

i32 CStdMemAlloc::CrtSetDbgFlag(i32 nNewFlag) { return 0; }

void CStdMemAlloc::CrtMemCheckpoint(_CrtMemState *pState) {}

// TODO(d.rattman): Remove when we have our own allocator
void *CStdMemAlloc::CrtSetReportFile(i32 nRptType, void *file) { return 0; }

void *CStdMemAlloc::CrtSetReportHook(void *pfnNewHook) { return 0; }

i32 CStdMemAlloc::CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                               const ch *szModule, const ch *pMsg) {
  return 0;
}

i32 CStdMemAlloc::heapchk() {
#ifdef OS_WIN
  return _HEAPOK;
#elif OS_POSIX
  return 1;
#endif
}

void CStdMemAlloc::DumpStats() { DumpStatsFileBase(kMemoryStatsDumpFileName); }

void CStdMemAlloc::DumpStatsFileBase(ch const *file_name) {
#ifdef OS_WIN
  auto [file, errno_info] =
      source::stdio_file_stream_factory::open(file_name, "wt");
  if (errno_info.is_success()) {
    // Dump statistics to small block heap
    m_SmallBlockHeap.DumpStats(std::move(file));
  }
#endif
}

void CStdMemAlloc::CompactHeap() {
#if !defined(NO_SBH) && defined(OS_WIN)
  usize nBytesRecovered = m_SmallBlockHeap.Compact();
  Msg("Compact freed %zu bytes\n", nBytesRecovered);
#endif
}

MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler(
    MemAllocFailHandler_t pfnMemAllocFailHandler) {
  MemAllocFailHandler_t pfnPrevious = m_pfnFailHandler;
  m_pfnFailHandler = pfnMemAllocFailHandler;
  return pfnPrevious;
}

usize CStdMemAlloc::DefaultFailHandler(usize nBytes) { return 0; }

void CStdMemAlloc::SetCRTAllocFailed(usize nSize) {
  m_sMemoryAllocFailed = nSize;
}

usize CStdMemAlloc::MemoryAllocFailed() { return m_sMemoryAllocFailed; }

#endif

#endif  // STEAM
