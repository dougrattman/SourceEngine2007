// Copyright © 1996-2017, Valve Corporation, All rights reserved.
// NOTE! This should never be called directly from leaf code
// Just use new, delete, malloc, free etc. They will call into this eventually.

#ifndef SOURCE_TIER0_MEMSTD_H_
#define SOURCE_TIER0_MEMSTD_H_

#ifdef _WIN32
#include "winlite.h"
#endif

#include <malloc.h>
#include <algorithm>
#include "mem_helpers.h"
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"

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
  void Init(size_t nBlockSize, uint8_t *pBase, size_t initialCommit = 0);
  size_t GetBlockSize();
  bool IsOwner(void *p);
  void *Alloc();
  void Free(void *p);
  size_t CountFreeBlocks();
  size_t GetCommittedSize();
  size_t CountCommittedBlocks();
  size_t CountAllocatedBlocks();
  size_t Compact();

 private:
  typedef TSLNodeBase_t FreeBlock_t;
  class CFreeList : public CTSListBase {
   public:
    void Push(void *p) { CTSListBase::Push((TSLNodeBase_t *)p); }
  };

  CFreeList m_FreeList;

  size_t m_nBlockSize;

  CInterlockedPtr<uint8_t> m_pNextAlloc;
  uint8_t *m_pCommitLimit;
  uint8_t *m_pAllocLimit;
  uint8_t *m_pBase;

  CThreadFastMutex m_CommitMutex;
};

class CSmallBlockHeap {
 public:
  CSmallBlockHeap();
  bool ShouldUse(size_t nBytes);
  bool IsOwner(void *p);
  void *Alloc(size_t nBytes);
  void *Realloc(void *p, size_t nBytes);
  void Free(void *p);
  size_t GetSize(void *p);
  void DumpStats(FILE *pFile = nullptr);
  size_t Compact();

 private:
  CSmallBlockPool *FindPool(size_t nBytes);
  CSmallBlockPool *FindPool(void *p);

  CSmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
  CSmallBlockPool m_Pools[NUM_POOLS];
  uint8_t *m_pBase;
  uint8_t *m_pLimit;
};

class CStdMemAlloc : public IMemAlloc {
 public:
  CStdMemAlloc()
      : m_pfnFailHandler(DefaultFailHandler), m_sMemoryAllocFailed((size_t)0) {}
  // Release versions
  virtual void *Alloc(size_t nSize);
  virtual void *Realloc(void *pMem, size_t nSize);
  virtual void Free(void *pMem);
  virtual void *Expand_NoLongerSupported(void *pMem, size_t nSize);

  // Debug versions
  virtual void *Alloc(size_t nSize, const char *pFileName, int nLine);
  virtual void *Realloc(void *pMem, size_t nSize, const char *pFileName,
                        int nLine);
  virtual void Free(void *pMem, const char *pFileName, int nLine);
  virtual void *Expand_NoLongerSupported(void *pMem, size_t nSize,
                                         const char *pFileName, int nLine);

  // Returns size of a particular allocation
  virtual size_t GetSize(void *pMem);

  // Force file + line information for an allocation
  virtual void PushAllocDbgInfo(const char *pFileName, int nLine);
  virtual void PopAllocDbgInfo();

  virtual long CrtSetBreakAlloc(long lNewBreakAlloc);
  virtual int CrtSetReportMode(int nReportType, int nReportMode);
  virtual int CrtIsValidHeapPointer(const void *pMem);
  virtual int CrtIsValidPointer(const void *pMem, unsigned int size,
                                int access);
  virtual int CrtCheckMemory(void);
  virtual int CrtSetDbgFlag(int nNewFlag);
  virtual void CrtMemCheckpoint(_CrtMemState *pState);
  void *CrtSetReportFile(int nRptType, void *hFile);
  void *CrtSetReportHook(void *pfnNewHook);
  int CrtDbgReport(int nRptType, const char *szFile, int nLine,
                   const char *szModule, const char *pMsg);
  virtual int heapchk();

  virtual void DumpStats();
  virtual void DumpStatsFileBase(char const *pchFileBase);

  virtual bool IsDebugHeap() { return false; }

  virtual void GetActualDbgInfo(const char *&pFileName, int &nLine) {}
  virtual void RegisterAllocation(const char *pFileName, int nLine,
                                  size_t nLogicalSize, size_t nActualSize,
                                  unsigned nTime) {}
  virtual void RegisterDeallocation(const char *pFileName, int nLine,
                                    size_t nLogicalSize, size_t nActualSize,
                                    unsigned nTime) {}

  virtual int GetVersion() { return MEMALLOC_VERSION; }

  virtual void CompactHeap();

  virtual MemAllocFailHandler_t SetAllocFailHandler(
      MemAllocFailHandler_t pfnMemAllocFailHandler);
  size_t CallAllocFailHandler(size_t nBytes) {
    return (*m_pfnFailHandler)(nBytes);
  }

  static size_t DefaultFailHandler(size_t);
  void DumpBlockStats(void *p) {}

#ifdef _WIN32
  CSmallBlockHeap m_SmallBlockHeap;
#endif

#if defined(_MEMTEST)
  virtual void SetStatsExtraInfo(const char *pMapName, const char *pComment);
#endif

  virtual size_t MemoryAllocFailed();

  void SetCRTAllocFailed(size_t nMemSize);

  MemAllocFailHandler_t m_pfnFailHandler;
  size_t m_sMemoryAllocFailed;
};

#endif  // !SOURCE_TIER0_MEMSTD_H_
