// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#include "pch_tier0.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include <malloc.h>
#include <algorithm>

#if defined(_WIN32)
#include "winlite.h"
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#endif

#include "mem_helpers.h"
#include "memstd.h"
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"

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

#if (!defined(_DEBUG) && !defined(USE_MEM_DEBUG))

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
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

#ifdef _WIN32
//-----------------------------------------------------------------------------
// Small block heap (multi-pool)
//-----------------------------------------------------------------------------

#ifndef NO_SBH
#define UsingSBH() true
#else
#define UsingSBH() false
#endif

template <typename T>
inline T MemAlign(T val, size_t alignment) {
  return (T)(((uintptr_t)val + alignment - 1) & ~(alignment - 1));
}

void CSmallBlockPool::Init(size_t nBlockSize, uint8_t *pBase,
                           size_t initialCommit) {
  if (!(nBlockSize % MIN_SBH_ALIGN == 0 && nBlockSize >= MIN_SBH_BLOCK &&
        nBlockSize >= sizeof(TSLNodeBase_t)))
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

size_t CSmallBlockPool::GetBlockSize() { return m_nBlockSize; }

bool CSmallBlockPool::IsOwner(void *p) {
  return (p >= m_pBase && p < m_pAllocLimit);
}

void *CSmallBlockPool::Alloc() {
  void *pResult = m_FreeList.Pop();
  if (!pResult) {
    size_t nBlockSize = m_nBlockSize;
    uint8_t *pCommitLimit, *pNextAlloc;
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
size_t CSmallBlockPool::CountFreeBlocks() { return m_FreeList.Count(); }

// Size of committed memory managed by this heap:
size_t CSmallBlockPool::GetCommittedSize() {
  size_t totalSize = m_pCommitLimit - m_pBase;
  Assert(0 != m_nBlockSize);
  Assert(0 != (totalSize % GetBlockSize()));

  return totalSize;
}

// Return the total blocks memory is committed for in the heap
size_t CSmallBlockPool::CountCommittedBlocks() {
  return GetCommittedSize() / GetBlockSize();
}

// Count the number of allocated blocks in the heap:
size_t CSmallBlockPool::CountAllocatedBlocks() {
  return CountCommittedBlocks() -
         (CountFreeBlocks() +
          (m_pCommitLimit - (uint8_t *)m_pNextAlloc) / GetBlockSize());
}

size_t CSmallBlockPool::Compact() {
  size_t nBytesFreed = 0;
  if (m_FreeList.Count()) {
    size_t nFree = CountFreeBlocks();
    FreeBlock_t **pSortArray = (FreeBlock_t **)malloc(
        nFree * sizeof(FreeBlock_t *));  // can't use new because will reenter

    if (!pSortArray) {
      return 0;
    }

    size_t i = 0;
    while (i < nFree) {
      pSortArray[i++] = m_FreeList.Pop();
    }

    std::sort(pSortArray, pSortArray + nFree);

    uint8_t *pOldNextAlloc = m_pNextAlloc;

    for (i = nFree; i >= 1; i--) {
      if ((uint8_t *)pSortArray[i - 1] ==
          static_cast<uint8_t *>(m_pNextAlloc) - m_nBlockSize) {
        pSortArray[i - 1] = nullptr;
        m_pNextAlloc -= m_nBlockSize;
      } else {
        break;
      }
    }

    if (pOldNextAlloc != m_pNextAlloc) {
      uint8_t *pNewCommitLimit = MemAlign((uint8_t *)m_pNextAlloc, PAGE_SIZE);
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

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define GetInitialCommitForPool(i) 0

CSmallBlockHeap::CSmallBlockHeap() {
  if (!UsingSBH()) {
    return;
  }

  m_pBase = (uint8_t *)VirtualAlloc(nullptr, NUM_POOLS * MAX_POOL_REGION,
                                    VA_RESERVE_FLAGS, PAGE_NOACCESS);
  m_pLimit = m_pBase + NUM_POOLS * MAX_POOL_REGION;

  // Build a lookup table used to find the correct pool based on size
  const int MAX_TABLE = MAX_SBH_BLOCK >> 2;
  int i = 0;
  size_t nBytesElement = 0;
  uint8_t *pCurBase = m_pBase;
  CSmallBlockPool *pCurPool = nullptr;
  int iCurPool = 0;

  // Blocks sized 0 - 128 are in pools in increments of 8
  for (; i < 32; i++) {
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
    if ((i + 1) % 4 == 1) {
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
      nBytesElement += 32;
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
    if ((i + 1) % 32 == 1) {
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

bool CSmallBlockHeap::ShouldUse(size_t nBytes) {
  return (UsingSBH() && nBytes <= MAX_SBH_BLOCK);
}

bool CSmallBlockHeap::IsOwner(void *p) {
  return (UsingSBH() && p >= m_pBase && p < m_pLimit);
}

void *CSmallBlockHeap::Alloc(size_t nBytes) {
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

void *CSmallBlockHeap::Realloc(void *p, size_t nBytes) {
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
    size_t nBytesCopy = min(nBytes, pOldPool->GetBlockSize());
    memcpy(pNewBlock, p, nBytesCopy);
  }

  pOldPool->Free(p);

  return pNewBlock;
}

void CSmallBlockHeap::Free(void *p) {
  CSmallBlockPool *pPool = FindPool(p);
  pPool->Free(p);
}

size_t CSmallBlockHeap::GetSize(void *p) {
  CSmallBlockPool *pPool = FindPool(p);
  return pPool->GetBlockSize();
}

void CSmallBlockHeap::DumpStats(FILE *pFile) {
  bool bSpew = true;

  if (pFile) {
    for (int i = 0; i < NUM_POOLS; i++) {
      // output for vxconsole parsing
      fprintf(pFile,
              "Pool %i: Size: %zu Allocated: %zu Free: %zu Committed: %zu "
              "CommittedSize: %zu\n",
              i, m_Pools[i].GetBlockSize(), m_Pools[i].CountAllocatedBlocks(),
              m_Pools[i].CountFreeBlocks(), m_Pools[i].CountCommittedBlocks(),
              m_Pools[i].GetCommittedSize());
    }
    bSpew = false;
  }

  if (bSpew) {
    size_t bytesCommitted = 0, bytesAllocated = 0;

    for (int i = 0; i < NUM_POOLS; i++) {
      Msg("Pool %i: (size: %zu) blocks: allocated:%zu free:%zu committed:%zu "
          "(committed size:%zu kb)\n",
          i, m_Pools[i].GetBlockSize(), m_Pools[i].CountAllocatedBlocks(),
          m_Pools[i].CountFreeBlocks(), m_Pools[i].CountCommittedBlocks(),
          m_Pools[i].GetCommittedSize() / 1024);

      bytesCommitted += m_Pools[i].GetCommittedSize();
      bytesAllocated +=
          (m_Pools[i].CountAllocatedBlocks() * m_Pools[i].GetBlockSize());
    }

    Msg("Totals: Committed:%zu kb Allocated:%zu kb\n", bytesCommitted / 1024,
        bytesAllocated / 1024);
  }
}

size_t CSmallBlockHeap::Compact() {
  size_t nBytesFreed = 0;
  for (int i = 0; i < NUM_POOLS; i++) {
    nBytesFreed += m_Pools[i].Compact();
  }
  return nBytesFreed;
}

CSmallBlockPool *CSmallBlockHeap::FindPool(size_t nBytes) {
  return m_PoolLookup[(nBytes - 1) >> 2];
}

CSmallBlockPool *CSmallBlockHeap::FindPool(void *p) {
  size_t i = ((uint8_t *)p - m_pBase) / MAX_POOL_REGION;
  return &m_Pools[i];
}

#endif

//-----------------------------------------------------------------------------
// Release versions
//-----------------------------------------------------------------------------

void *CStdMemAlloc::Alloc(size_t nSize) {
  PROFILE_ALLOC(Malloc);

#ifdef _WIN32
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

void *CStdMemAlloc::Realloc(void *pMem, size_t nSize) {
  if (!pMem) {
    return Alloc(nSize);
  }

  PROFILE_ALLOC(Realloc);

#ifdef _WIN32
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

#ifdef _WIN32
  if (m_SmallBlockHeap.IsOwner(pMem)) {
    m_SmallBlockHeap.Free(pMem);
    return;
  }
#endif

  free(pMem);
}

void *CStdMemAlloc::Expand_NoLongerSupported(void *pMem, size_t nSize) {
  return nullptr;
}

//-----------------------------------------------------------------------------
// Debug versions
//-----------------------------------------------------------------------------
void *CStdMemAlloc::Alloc(size_t nSize, const char *pFileName, int nLine) {
  return CStdMemAlloc::Alloc(nSize);
}

void *CStdMemAlloc::Realloc(void *pMem, size_t nSize, const char *pFileName,
                            int nLine) {
  return CStdMemAlloc::Realloc(pMem, nSize);
}

void CStdMemAlloc::Free(void *pMem, const char *pFileName, int nLine) {
  CStdMemAlloc::Free(pMem);
}

void *CStdMemAlloc::Expand_NoLongerSupported(void *pMem, size_t nSize,
                                             const char *pFileName, int nLine) {
  return nullptr;
}

//-----------------------------------------------------------------------------
// Returns size of a particular allocation
//-----------------------------------------------------------------------------
size_t CStdMemAlloc::GetSize(void *pMem) {
#ifdef _WIN32
  if (!pMem) return CalcHeapUsed();

  if (m_SmallBlockHeap.IsOwner(pMem)) return m_SmallBlockHeap.GetSize(pMem);

  return _msize(pMem);
#elif _LINUX
  Assert("GetSize() not implemented");
#endif
}

//-----------------------------------------------------------------------------
// Force file + line information for an allocation
//-----------------------------------------------------------------------------
void CStdMemAlloc::PushAllocDbgInfo(const char *pFileName, int nLine) {}

void CStdMemAlloc::PopAllocDbgInfo() {}

//-----------------------------------------------------------------------------
// FIXME: Remove when we make our own heap! Crt stuff we're currently using
//-----------------------------------------------------------------------------
long CStdMemAlloc::CrtSetBreakAlloc(long lNewBreakAlloc) { return 0; }

int CStdMemAlloc::CrtSetReportMode(int nReportType, int nReportMode) {
  return 0;
}

int CStdMemAlloc::CrtIsValidHeapPointer(const void *pMem) { return 1; }

int CStdMemAlloc::CrtIsValidPointer(const void *pMem, unsigned int size,
                                    int access) {
  return 1;
}

int CStdMemAlloc::CrtCheckMemory() { return 1; }

int CStdMemAlloc::CrtSetDbgFlag(int nNewFlag) { return 0; }

void CStdMemAlloc::CrtMemCheckpoint(_CrtMemState *pState) {}

// FIXME: Remove when we have our own allocator
void *CStdMemAlloc::CrtSetReportFile(int nRptType, void *file) { return 0; }

void *CStdMemAlloc::CrtSetReportHook(void *pfnNewHook) { return 0; }

int CStdMemAlloc::CrtDbgReport(int nRptType, const char *szFile, int nLine,
                               const char *szModule, const char *pMsg) {
  return 0;
}

int CStdMemAlloc::heapchk() {
#ifdef _WIN32
  return _HEAPOK;
#elif _LINUX
  return 1;
#endif
}

void CStdMemAlloc::DumpStats() { DumpStatsFileBase("memstats"); }

void CStdMemAlloc::DumpStatsFileBase(char const *pchFileBase) {
#ifdef _WIN32
  char filename[512];
  _snprintf_s(filename, ARRAYSIZE(filename) - 1, "%s.txt", pchFileBase);

  FILE *pFile = fopen(filename, "wt");
  if (pFile) {
    fprintf(pFile, "\nSBH:\n");
    m_SmallBlockHeap.DumpStats(pFile);  // Dump statistics to small block heap
    fclose(pFile);
  }
#endif
}

void CStdMemAlloc::CompactHeap() {
#if !defined(NO_SBH) && defined(_WIN32)
  size_t nBytesRecovered = m_SmallBlockHeap.Compact();
  Msg("Compact freed %zu bytes\n", nBytesRecovered);
#endif
}

MemAllocFailHandler_t CStdMemAlloc::SetAllocFailHandler(
    MemAllocFailHandler_t pfnMemAllocFailHandler) {
  MemAllocFailHandler_t pfnPrevious = m_pfnFailHandler;
  m_pfnFailHandler = pfnMemAllocFailHandler;
  return pfnPrevious;
}

size_t CStdMemAlloc::DefaultFailHandler(size_t nBytes) { return 0; }

#if defined(_MEMTEST)
void CStdMemAlloc::void SetStatsExtraInfo(const char *pMapName,
                                          const char *pComment) {}
#endif

void CStdMemAlloc::SetCRTAllocFailed(size_t nSize) {
  m_sMemoryAllocFailed = nSize;
}

size_t CStdMemAlloc::MemoryAllocFailed() { return m_sMemoryAllocFailed; }

#endif

#endif  // STEAM
