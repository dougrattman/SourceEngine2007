// Copyright © 1996-2018, Valve Corporation, All rights reserved.
// NOTE! This should never be called directly from leaf code
// Just use new, delete, malloc, free etc. They will call into this eventually.

#ifndef SOURCE_TIER0_MEMSTD_H_
#define SOURCE_TIER0_MEMSTD_H_

#include <malloc.h>
#include <algorithm>

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"
#endif

#include "mem_helpers.h"
#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/threadtools.h"
#include "tier0/include/tslist.h"

#pragma pack(4)

#define MIN_SBH_BLOCK 8
#define MIN_SBH_ALIGN 8
#define MAX_SBH_BLOCK 2048
#define MAX_POOL_REGION (4 * 1024 * 1024)
#define PAGE_SIZE (4 * 1024)
#define COMMIT_SIZE (16 * PAGE_SIZE)
#define NUM_POOLS 42

class CSmallBlockPool {
 public:
  void Init(usize nBlockSize, u8 *pBase, usize initialCommit = 0);
  usize GetBlockSize();
  bool IsOwner(void *p);
  void *Alloc();
  void Free(void *p);
  usize CountFreeBlocks();
  usize GetCommittedSize();
  usize CountCommittedBlocks();
  usize CountAllocatedBlocks();
  usize Compact();

 private:
  using FreeBlock_t = TSLNodeBase_t;
  class CFreeList : public CTSListBase {
   public:
    void Push(void *p) { CTSListBase::Push((TSLNodeBase_t *)p); }
  };

  CFreeList m_FreeList;
  usize m_nBlockSize;

  CInterlockedPtr<u8> m_pNextAlloc;
  u8 *m_pCommitLimit;
  u8 *m_pAllocLimit;
  u8 *m_pBase;

  CThreadFastMutex m_CommitMutex;
};

class CSmallBlockHeap {
 public:
  CSmallBlockHeap();
  bool ShouldUse(usize nBytes);
  bool IsOwner(void *p);
  void *Alloc(usize nBytes);
  void *Realloc(void *p, usize nBytes);
  void Free(void *p);
  usize GetSize(void *p);
  void DumpStats(FILE *pFile = nullptr);
  usize Compact();

 private:
  CSmallBlockPool *FindPool(usize nBytes);
  CSmallBlockPool *FindPool(void *p);

  CSmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
  CSmallBlockPool m_Pools[NUM_POOLS];
  u8 *m_pBase;
  u8 *m_pLimit;
};

class CStdMemAlloc : public IMemAlloc {
 public:
  CStdMemAlloc()
      : m_pfnFailHandler(DefaultFailHandler), m_sMemoryAllocFailed((usize)0) {}
  // Release versions
  virtual void *Alloc(usize nSize);
  virtual void *Realloc(void *pMem, usize nSize);
  virtual void Free(void *pMem);
  virtual void *Expand_NoLongerSupported(void *pMem, usize nSize);

  // Debug versions
  virtual void *Alloc(usize nSize, const ch *pFileName, i32 nLine);
  virtual void *Realloc(void *pMem, usize nSize, const ch *pFileName,
                        i32 nLine);
  virtual void Free(void *pMem, const ch *pFileName, i32 nLine);
  virtual void *Expand_NoLongerSupported(void *pMem, usize nSize,
                                         const ch *pFileName, i32 nLine);

  // Returns size of a particular allocation
  virtual usize GetSize(void *pMem);

  // Force file + line information for an allocation
  virtual void PushAllocDbgInfo(const ch *pFileName, i32 nLine);
  virtual void PopAllocDbgInfo();

  virtual long CrtSetBreakAlloc(long lNewBreakAlloc);
  virtual i32 CrtSetReportMode(i32 nReportType, i32 nReportMode);
  virtual i32 CrtIsValidHeapPointer(const void *pMem);
  virtual i32 CrtIsValidPointer(const void *pMem, u32 size, i32 access);
  virtual i32 CrtCheckMemory();
  virtual i32 CrtSetDbgFlag(i32 nNewFlag);
  virtual void CrtMemCheckpoint(_CrtMemState *pState);
  void *CrtSetReportFile(i32 nRptType, void *hFile);
  void *CrtSetReportHook(void *pfnNewHook);
  i32 CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                   const ch *szModule, const ch *pMsg);
  virtual i32 heapchk();

  virtual void DumpStats();
  virtual void DumpStatsFileBase(ch const *pchFileBase);

  virtual bool IsDebugHeap() { return false; }

  virtual void GetActualDbgInfo(const ch *&pFileName, i32 &nLine) {}
  virtual void RegisterAllocation(const ch *pFileName, i32 nLine,
                                  usize nLogicalSize, usize nActualSize,
                                  u32 nTime) {}
  virtual void RegisterDeallocation(const ch *pFileName, i32 nLine,
                                    usize nLogicalSize, usize nActualSize,
                                    u32 nTime) {}

  virtual i32 GetVersion() { return MEMALLOC_VERSION; }

  virtual void CompactHeap();

  virtual MemAllocFailHandler_t SetAllocFailHandler(
      MemAllocFailHandler_t pfnMemAllocFailHandler);
  usize CallAllocFailHandler(usize nBytes) {
    return (*m_pfnFailHandler)(nBytes);
  }

  static usize DefaultFailHandler(usize);
  void DumpBlockStats(void *p) {}

#ifdef OS_WIN
  CSmallBlockHeap m_SmallBlockHeap;
#endif

  virtual usize MemoryAllocFailed();

  void SetCRTAllocFailed(usize nMemSize);

  MemAllocFailHandler_t m_pfnFailHandler;
  usize m_sMemoryAllocFailed;
};

#endif  // !SOURCE_TIER0_MEMSTD_H_
