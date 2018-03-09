// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#ifndef STEAM

#ifdef TIER0_VALIDATE_HEAP

#include <malloc.h>
#include "mem_helpers.h"
#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"

extern IMemAlloc *g_pActualAlloc;

// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually

class CValidateAlloc : public IMemAlloc {
 public:
  enum {
    HEAP_PREFIX_BUFFER_SIZE = 12,
    HEAP_SUFFIX_BUFFER_SIZE = 8,
  };

  CValidateAlloc();

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
  virtual i32 CrtIsValidPointer(const void *pMem, u32 i32 size, i32 access);
  virtual i32 CrtCheckMemory();
  virtual i32 CrtSetDbgFlag(i32 nNewFlag);
  virtual void CrtMemCheckpoint(_CrtMemState *pState);
  void *CrtSetReportFile(i32 nRptType, void *file);
  void *CrtSetReportHook(void *pfnNewHook);
  i32 CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                   const ch *szModule, const ch *pMsg);
  virtual i32 heapchk();

  virtual void DumpStats() {}
  virtual void DumpStatsFileBase(ch const *pchFileBase) {}

  virtual bool IsDebugHeap() { return true; }

  virtual i32 GetVersion() { return MEMALLOC_VERSION; }

  virtual void CompactHeap();
  virtual MemAllocFailHandler_t SetAllocFailHandler(
      MemAllocFailHandler_t pfnMemAllocFailHandler);

 private:
  struct HeapPrefix_t {
    HeapPrefix_t *m_pPrev;
    HeapPrefix_t *m_pNext;
    i32 m_nSize;
    u32 ch m_Prefix[HEAP_PREFIX_BUFFER_SIZE];
  };

  struct HeapSuffix_t {
    u32 ch m_Suffix[HEAP_SUFFIX_BUFFER_SIZE];
  };

 private:
  // Returns the actual debug info
  void GetActualDbgInfo(const ch *&pFileName, i32 &nLine);

  // Updates stats
  void RegisterAllocation(const ch *pFileName, i32 nLine, usize nLogicalSize,
                          usize nActualSize, u32 nTime);
  void RegisterDeallocation(const ch *pFileName, i32 nLine, usize nLogicalSize,
                            usize nActualSize, u32 nTime);

  HeapSuffix_t *Suffix(HeapPrefix_t *pPrefix);
  void *AllocationStart(HeapPrefix_t *pBase);
  HeapPrefix_t *PrefixFromAllocation(void *pAlloc);
  const HeapPrefix_t *PrefixFromAllocation(const void *pAlloc);

  // Add to the list!
  void AddToList(HeapPrefix_t *pHeap, i32 nSize);

  // Remove from the list!
  void RemoveFromList(HeapPrefix_t *pHeap);

  // Validate the allocation
  bool ValidateAllocation(HeapPrefix_t *pHeap);

 private:
  HeapPrefix_t *m_pFirstAllocation;
  ch m_pPrefixImage[HEAP_PREFIX_BUFFER_SIZE];
  ch m_pSuffixImage[HEAP_SUFFIX_BUFFER_SIZE];
};

// Singleton...

static CValidateAlloc s_ValidateAlloc;
IMemAlloc *g_pMemAlloc = &s_ValidateAlloc;

// Constructor.

CValidateAlloc::CValidateAlloc() {
  m_pFirstAllocation = 0;
  memset(m_pPrefixImage, 0xBE, HEAP_PREFIX_BUFFER_SIZE);
  memset(m_pSuffixImage, 0xAF, HEAP_SUFFIX_BUFFER_SIZE);
}

// Accessors...

inline CValidateAlloc::HeapSuffix_t *CValidateAlloc::Suffix(
    HeapPrefix_t *pPrefix) {
  return reinterpret_cast<HeapSuffix_t *>((u32 ch *)(pPrefix + 1) +
                                          pPrefix->m_nSize);
}

inline void *CValidateAlloc::AllocationStart(HeapPrefix_t *pBase) {
  return static_cast<void *>(pBase + 1);
}

inline CValidateAlloc::HeapPrefix_t *CValidateAlloc::PrefixFromAllocation(
    void *pAlloc) {
  if (!pAlloc) return nullptr;

  return ((HeapPrefix_t *)pAlloc) - 1;
}

inline const CValidateAlloc::HeapPrefix_t *CValidateAlloc::PrefixFromAllocation(
    const void *pAlloc) {
  return ((const HeapPrefix_t *)pAlloc) - 1;
}

// Add to the list!

void CValidateAlloc::AddToList(HeapPrefix_t *pHeap, i32 nSize) {
  pHeap->m_pPrev = nullptr;
  pHeap->m_pNext = m_pFirstAllocation;
  if (m_pFirstAllocation) {
    m_pFirstAllocation->m_pPrev = pHeap;
  }
  pHeap->m_nSize = nSize;

  m_pFirstAllocation = pHeap;

  HeapSuffix_t *pSuffix = Suffix(pHeap);
  memcpy(pHeap->m_Prefix, m_pPrefixImage, HEAP_PREFIX_BUFFER_SIZE);
  memcpy(pSuffix->m_Suffix, m_pSuffixImage, HEAP_SUFFIX_BUFFER_SIZE);
}

// Remove from the list!

void CValidateAlloc::RemoveFromList(HeapPrefix_t *pHeap) {
  if (!pHeap) return;

  ValidateAllocation(pHeap);
  if (pHeap->m_pPrev) {
    pHeap->m_pPrev->m_pNext = pHeap->m_pNext;
  } else {
    m_pFirstAllocation = pHeap->m_pNext;
  }

  if (pHeap->m_pNext) {
    pHeap->m_pNext->m_pPrev = pHeap->m_pPrev;
  }
}

// Validate the allocation

bool CValidateAlloc::ValidateAllocation(HeapPrefix_t *pHeap) {
  HeapSuffix_t *pSuffix = Suffix(pHeap);

  bool bOk = true;
  if (memcmp(pHeap->m_Prefix, m_pPrefixImage, HEAP_PREFIX_BUFFER_SIZE)) {
    bOk = false;
  }

  if (memcmp(pSuffix->m_Suffix, m_pSuffixImage, HEAP_SUFFIX_BUFFER_SIZE)) {
    bOk = false;
  }

  if (!bOk) {
    Warning("Memory trash detected in allocation %X!\n", (void *)(pHeap + 1));
    Assert(0);
  }

  return bOk;
}

// Release versions

void *CValidateAlloc::Alloc(usize nSize) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  HeapPrefix_t *pHeap = (HeapPrefix_t *)g_pActualAlloc->Alloc(nActualSize);
  AddToList(pHeap, nSize);
  return AllocationStart(pHeap);
}

void *CValidateAlloc::Realloc(void *pMem, usize nSize) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  pHeap = (HeapPrefix_t *)g_pActualAlloc->Realloc(pHeap, nActualSize);
  AddToList(pHeap, nSize);

  return AllocationStart(pHeap);
}

void CValidateAlloc::Free(void *pMem) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  g_pActualAlloc->Free(pHeap);
}

void *CValidateAlloc::Expand_NoLongerSupported(void *pMem, usize nSize) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  pHeap = (HeapPrefix_t *)g_pActualAlloc->Expand_NoLongerSupported(pHeap,
                                                                   nActualSize);
  AddToList(pHeap, nSize);

  return AllocationStart(pHeap);
}

// Debug versions

void *CValidateAlloc::Alloc(usize nSize, const ch *pFileName, i32 nLine) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  HeapPrefix_t *pHeap =
      (HeapPrefix_t *)g_pActualAlloc->Alloc(nActualSize, pFileName, nLine);
  AddToList(pHeap, nSize);
  return AllocationStart(pHeap);
}

void *CValidateAlloc::Realloc(void *pMem, usize nSize, const ch *pFileName,
                              i32 nLine) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  pHeap = (HeapPrefix_t *)g_pActualAlloc->Realloc(pHeap, nActualSize, pFileName,
                                                  nLine);
  AddToList(pHeap, nSize);

  return AllocationStart(pHeap);
}

void CValidateAlloc::Free(void *pMem, const ch *pFileName, i32 nLine) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  g_pActualAlloc->Free(pHeap, pFileName, nLine);
}

void *CValidateAlloc::Expand_NoLongerSupported(void *pMem, usize nSize,
                                               const ch *pFileName, i32 nLine) {
  Assert(heapchk() == _HEAPOK);
  Assert(CrtCheckMemory());
  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  RemoveFromList(pHeap);

  i32 nActualSize = nSize + sizeof(HeapPrefix_t) + sizeof(HeapSuffix_t);
  pHeap = (HeapPrefix_t *)g_pActualAlloc->Expand_NoLongerSupported(
      pHeap, nActualSize, pFileName, nLine);
  AddToList(pHeap, nSize);

  return AllocationStart(pHeap);
}

// Returns size of a particular allocation

usize CValidateAlloc::GetSize(void *pMem) {
  if (!pMem) return CalcHeapUsed();

  HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  return pHeap->m_nSize;
}

// Force file + line information for an allocation

void CValidateAlloc::PushAllocDbgInfo(const ch *pFileName, i32 nLine) {
  g_pActualAlloc->PushAllocDbgInfo(pFileName, nLine);
}

void CValidateAlloc::PopAllocDbgInfo() { g_pActualAlloc->PopAllocDbgInfo(); }

// TODO(d.rattman): Remove when we make our own heap! Crt stuff we're currently using

long CValidateAlloc::CrtSetBreakAlloc(long lNewBreakAlloc) {
  return g_pActualAlloc->CrtSetBreakAlloc(lNewBreakAlloc);
}

i32 CValidateAlloc::CrtSetReportMode(i32 nReportType, i32 nReportMode) {
  return g_pActualAlloc->CrtSetReportMode(nReportType, nReportMode);
}

i32 CValidateAlloc::CrtIsValidHeapPointer(const void *pMem) {
  const HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  return g_pActualAlloc->CrtIsValidHeapPointer(pHeap);
}

i32 CValidateAlloc::CrtIsValidPointer(const void *pMem, u32 i32 size,
                                      i32 access) {
  const HeapPrefix_t *pHeap = PrefixFromAllocation(pMem);
  return g_pActualAlloc->CrtIsValidPointer(pHeap, size, access);
}

i32 CValidateAlloc::CrtCheckMemory() {
  return g_pActualAlloc->CrtCheckMemory();
}

i32 CValidateAlloc::CrtSetDbgFlag(i32 nNewFlag) {
  return g_pActualAlloc->CrtSetDbgFlag(nNewFlag);
}

void CValidateAlloc::CrtMemCheckpoint(_CrtMemState *pState) {
  g_pActualAlloc->CrtMemCheckpoint(pState);
}

void *CValidateAlloc::CrtSetReportFile(i32 nRptType, void *file) {
  return g_pActualAlloc->CrtSetReportFile(nRptType, file);
}

void *CValidateAlloc::CrtSetReportHook(void *pfnNewHook) {
  return g_pActualAlloc->CrtSetReportHook(pfnNewHook);
}

i32 CValidateAlloc::CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                                 const ch *szModule, const ch *pMsg) {
  return g_pActualAlloc->CrtDbgReport(nRptType, szFile, nLine, szModule, pMsg);
}

i32 CValidateAlloc::heapchk() {
  bool bOk = true;

  // Validate the heap
  HeapPrefix_t *pHeap = m_pFirstAllocation;
  for (pHeap = m_pFirstAllocation; pHeap; pHeap = pHeap->m_pNext) {
    if (!ValidateAllocation(pHeap)) {
      bOk = false;
    }
  }

#ifdef OS_WIN
  return bOk ? _HEAPOK : 0;
#elif OS_POSIX
  return bOk;
#endif
}

// Returns the actual debug info
void CValidateAlloc::GetActualDbgInfo(const ch *&pFileName, i32 &nLine) {
  g_pActualAlloc->GetActualDbgInfo(pFileName, nLine);
}

// Updates stats
void CValidateAlloc::RegisterAllocation(const ch *pFileName, i32 nLine,
                                        usize nLogicalSize, usize nActualSize,
                                        u32 nTime) {
  g_pActualAlloc->RegisterAllocation(pFileName, nLine, nLogicalSize,
                                     nActualSize, nTime);
}

void CValidateAlloc::RegisterDeallocation(const ch *pFileName, i32 nLine,
                                          usize nLogicalSize, usize nActualSize,
                                          u32 nTime) {
  g_pActualAlloc->RegisterDeallocation(pFileName, nLine, nLogicalSize,
                                       nActualSize, nTime);
}

void CValidateAlloc::CompactHeap() { g_pActualAlloc->CompactHeap(); }

MemAllocFailHandler_t CValidateAlloc::SetAllocFailHandler(
    MemAllocFailHandler_t pfnMemAllocFailHandler) {
  return g_pActualAlloc->SetAllocFailHandler(pfnMemAllocFailHandler);
}

#endif  // TIER0_VALIDATE_HEAP

#endif  // STEAM
