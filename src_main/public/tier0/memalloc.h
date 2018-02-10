// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: This header should never be used directly from leaf code!!!
// Instead, just add the file memoverride.cpp into your project and all this
// will automagically be used

#ifndef SOURCE_TIER0_MEMALLOC_H_
#define SOURCE_TIER0_MEMALLOC_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

// Define this in release to get memory tracking even in release builds
//#define USE_MEM_DEBUG 1

#if defined(_MEMTEST)
#define USE_MEM_DEBUG 1
#endif

// Undefine this if using a compiler lacking threadsafe RTTI (like vc6)
#define MEM_DEBUG_CLASSNAME 1

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include <cstddef>
#include "tier0/include/mem.h"

struct _CrtMemState;

#define MEMALLOC_VERSION 1

typedef usize (*MemAllocFailHandler_t)(usize);

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
abstract_class IMemAlloc {
 public:
  // Release versions
  virtual void *Alloc(usize nSize) = 0;
  virtual void *Realloc(void *pMem, usize nSize) = 0;
  virtual void Free(void *pMem) = 0;
  virtual void *Expand_NoLongerSupported(void *pMem, usize nSize) = 0;

  // Debug versions
  virtual void *Alloc(usize nSize, const ch *pFileName, i32 nLine) = 0;
  virtual void *Realloc(void *pMem, usize nSize, const ch *pFileName,
                        i32 nLine) = 0;
  virtual void Free(void *pMem, const ch *pFileName, i32 nLine) = 0;
  virtual void *Expand_NoLongerSupported(void *pMem, usize nSize,
                                         const ch *pFileName, i32 nLine) = 0;

  // Returns size of a particular allocation
  virtual usize GetSize(void *pMem) = 0;

  // Force file + line information for an allocation
  virtual void PushAllocDbgInfo(const ch *pFileName, i32 nLine) = 0;
  virtual void PopAllocDbgInfo() = 0;

  // FIXME: Remove when we have our own allocator
  // these methods of the Crt debug code is used in our codebase currently
  virtual long CrtSetBreakAlloc(long lNewBreakAlloc) = 0;
  virtual i32 CrtSetReportMode(i32 nReportType, i32 nReportMode) = 0;
  virtual i32 CrtIsValidHeapPointer(const void *pMem) = 0;
  virtual i32 CrtIsValidPointer(const void *pMem, u32 size, i32 access) = 0;
  virtual i32 CrtCheckMemory(void) = 0;
  virtual i32 CrtSetDbgFlag(i32 nNewFlag) = 0;
  virtual void CrtMemCheckpoint(_CrtMemState * pState) = 0;

  // FIXME: Make a better stats interface
  virtual void DumpStats() = 0;
  virtual void DumpStatsFileBase(ch const *pchFileBase) = 0;

  // FIXME: Remove when we have our own allocator
  virtual void *CrtSetReportFile(i32 nRptType, void *hFile) = 0;
  virtual void *CrtSetReportHook(void *pfnNewHook) = 0;
  virtual i32 CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                           const ch *szModule, const ch *pMsg) = 0;

  virtual i32 heapchk() = 0;

  virtual bool IsDebugHeap() = 0;

  virtual void GetActualDbgInfo(const ch *&pFileName, i32 &nLine) = 0;
  virtual void RegisterAllocation(const ch *pFileName, i32 nLine,
                                  usize nLogicalSize, usize nActualSize,
                                  u32 nTime) = 0;
  virtual void RegisterDeallocation(const ch *pFileName, i32 nLine,
                                    usize nLogicalSize, usize nActualSize,
                                    u32 nTime) = 0;

  virtual i32 GetVersion() = 0;

  virtual void CompactHeap() = 0;

  // Function called when malloc fails or memory limits hit to attempt to free
  // up memory (can come in any thread)
  virtual MemAllocFailHandler_t SetAllocFailHandler(
      MemAllocFailHandler_t pfnMemAllocFailHandler) = 0;

  virtual void DumpBlockStats(void *) = 0;

#if defined(_MEMTEST)
  virtual void SetStatsExtraInfo(const ch *pMapName, const ch *pComment) = 0;
#endif

  // Returns 0 if no failure, otherwise the usize of the last requested chunk
  virtual usize MemoryAllocFailed() = 0;
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
MEM_INTERFACE IMemAlloc *g_pMemAlloc;

//-----------------------------------------------------------------------------

inline void *MemAlloc_AllocAligned(usize size, usize align) {
  u8 *pAlloc, *pResult;

  if (!IsPowerOfTwo(align)) return nullptr;

  align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

  if ((pAlloc = (u8 *)g_pMemAlloc->Alloc(sizeof(void *) + align + size)) ==
      (u8 *)nullptr)
    return nullptr;

  pResult = (u8 *)((usize)(pAlloc + sizeof(void *) + align) & ~align);
  ((u8 **)(pResult))[-1] = pAlloc;

  return (void *)pResult;
}

inline void *MemAlloc_AllocAligned(usize size, usize align, const ch *pszFile,
                                   i32 nLine) {
  u8 *pAlloc, *pResult;

  if (!IsPowerOfTwo(align)) return nullptr;

  align = (align > sizeof(void *) ? align : sizeof(void *)) - 1;

  if ((pAlloc = (u8 *)g_pMemAlloc->Alloc(sizeof(void *) + align + size, pszFile,
                                         nLine)) == (u8 *)nullptr)
    return nullptr;

  pResult = (u8 *)((usize)(pAlloc + sizeof(void *) + align) & ~align);
  ((u8 **)(pResult))[-1] = pAlloc;

  return (void *)pResult;
}

inline void *MemAlloc_ReallocAligned(void *ptr, usize size, usize align) {
  if (!IsPowerOfTwo(align)) return nullptr;

  // Don't change alignment between allocation + reallocation.
  if (((usize)ptr & (align - 1)) != 0) return nullptr;

  if (!ptr) return MemAlloc_AllocAligned(size, align);

  void *pAlloc, *pResult;

  // Figure out the actual allocation point
  pAlloc = ptr;
  pAlloc = (void *)(((usize)pAlloc & ~(sizeof(void *) - 1)) - sizeof(void *));
  pAlloc = *((void **)pAlloc);

  // See if we have enough space
  usize nOffset = (usize)ptr - (usize)pAlloc;
  usize nOldSize = g_pMemAlloc->GetSize(pAlloc);
  if (nOldSize >= size + nOffset) return ptr;

  pResult = MemAlloc_AllocAligned(size, align);
  memcpy(pResult, ptr, nOldSize - nOffset);
  g_pMemAlloc->Free(pAlloc);
  return pResult;
}

inline void MemAlloc_FreeAligned(void *pMemBlock) {
  void *pAlloc;

  if (pMemBlock == nullptr) return;

  pAlloc = pMemBlock;

  // pAlloc points to the pointer to starting of the memory block
  pAlloc = (void *)(((usize)pAlloc & ~(sizeof(void *) - 1)) - sizeof(void *));

  // pAlloc is the pointer to the start of memory block
  pAlloc = *((void **)pAlloc);
  g_pMemAlloc->Free(pAlloc);
}

inline usize MemAlloc_GetSizeAligned(void *pMemBlock) {
  void *pAlloc;

  if (pMemBlock == nullptr) return 0;

  pAlloc = pMemBlock;

  // pAlloc points to the pointer to starting of the memory block
  pAlloc = (void *)(((usize)pAlloc & ~(sizeof(void *) - 1)) - sizeof(void *));

  // pAlloc is the pointer to the start of memory block
  pAlloc = *((void **)pAlloc);
  return g_pMemAlloc->GetSize(pAlloc) - ((u8 *)pMemBlock - (u8 *)pAlloc);
}

//-----------------------------------------------------------------------------

#if (!defined(NDEBUG) || defined(USE_MEM_DEBUG))
#define MEM_ALLOC_CREDIT_(tag) \
  CMemAllocAttributeAlloction memAllocAttributeAlloction(tag, __LINE__)
#define MemAlloc_PushAllocDbgInfo(pszFile, line) \
  g_pMemAlloc->PushAllocDbgInfo(pszFile, line)
#define MemAlloc_PopAllocDbgInfo() g_pMemAlloc->PopAllocDbgInfo()
#define MemAlloc_RegisterAllocation(pFileName, nLine, nLogicalSize,            \
                                    nActualSize, nTime)                        \
  g_pMemAlloc->RegisterAllocation(pFileName, nLine, nLogicalSize, nActualSize, \
                                  nTime)
#define MemAlloc_RegisterDeallocation(pFileName, nLine, nLogicalSize, \
                                      nActualSize, nTime)             \
  g_pMemAlloc->RegisterDeallocation(pFileName, nLine, nLogicalSize,   \
                                    nActualSize, nTime)
#else
#define MEM_ALLOC_CREDIT_(tag) ((void)0)
#define MemAlloc_PushAllocDbgInfo(pszFile, line) ((void)0)
#define MemAlloc_PopAllocDbgInfo() ((void)0)
#define MemAlloc_RegisterAllocation(pFileName, nLine, nLogicalSize, \
                                    nActualSize, nTime)             \
  ((void)0)
#define MemAlloc_RegisterDeallocation(pFileName, nLine, nLogicalSize, \
                                      nActualSize, nTime)             \
  ((void)0)
#endif

//-----------------------------------------------------------------------------

class CMemAllocAttributeAlloction {
 public:
  CMemAllocAttributeAlloction(const ch *pszFile, i32 line) {
    MemAlloc_PushAllocDbgInfo(pszFile, line);
  }

  ~CMemAllocAttributeAlloction() { MemAlloc_PopAllocDbgInfo(); }
};

#define MEM_ALLOC_CREDIT() MEM_ALLOC_CREDIT_(__FILE__)

//-----------------------------------------------------------------------------

#if defined(OS_WIN) && (!defined(NDEBUG) || defined(USE_MEM_DEBUG))

#include <typeinfo.h>

// MEM_DEBUG_CLASSNAME is opt-in.
// Note: typeid().name() is not threadsafe, so if the project needs to access it
// in multiple threads simultaneously, it'll need a mutex.
#if defined(_CPPRTTI) && defined(MEM_DEBUG_CLASSNAME)
#define MEM_ALLOC_CREDIT_CLASS() MEM_ALLOC_CREDIT_(typeid(*this).name())
#define MEM_ALLOC_CLASSNAME(type) (typeid((type *)(0)).name())
#else
#define MEM_ALLOC_CREDIT_CLASS() MEM_ALLOC_CREDIT_(__FILE__)
#define MEM_ALLOC_CLASSNAME(type) (__FILE__)
#endif

// MEM_ALLOC_CREDIT_FUNCTION is used when no this pointer is available ( inside
// 'new' overloads, for example )
#ifdef COMPILER_MSVC
#define MEM_ALLOC_CREDIT_FUNCTION() MEM_ALLOC_CREDIT_(__FUNCTION__)
#else
#define MEM_ALLOC_CREDIT_FUNCTION() (__FILE__)
#endif

#else
#define MEM_ALLOC_CREDIT_CLASS()
#define MEM_ALLOC_CLASSNAME(type) nullptr
#define MEM_ALLOC_CREDIT_FUNCTION()
#endif

//-----------------------------------------------------------------------------

#if (!defined(NDEBUG) || defined(USE_MEM_DEBUG))
struct MemAllocFileLine_t {
  const ch *pszFile;
  i32 line;
};

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING(tag)                     \
  static CUtlMap<void *, MemAllocFileLine_t, i32> g_##tag##Allocs( \
      DefLessFunc(void *));                                        \
  static const ch *g_psz##tag##Alloc =                             \
      strcpy((ch *)g_pMemAlloc->Alloc(strlen(#tag "Alloc") + 1,    \
                                      "intentional leak", 0),      \
             #tag "Alloc");

#define MemAlloc_RegisterExternalAllocation(tag, p, size)                    \
  if (!p)                                                                    \
    ;                                                                        \
  else {                                                                     \
    MemAllocFileLine_t fileLine = {g_psz##tag##Alloc, 0};                    \
    g_pMemAlloc->GetActualDbgInfo(fileLine.pszFile, fileLine.line);          \
    if (fileLine.pszFile != g_psz##tag##Alloc) {                             \
      g_##tag##Allocs.Insert(p, fileLine);                                   \
    }                                                                        \
                                                                             \
    MemAlloc_RegisterAllocation(fileLine.pszFile, fileLine.line, size, size, \
                                0);                                          \
  }

#define MemAlloc_RegisterExternalDeallocation(tag, p, size)                    \
  if (!p)                                                                      \
    ;                                                                          \
  else {                                                                       \
    MemAllocFileLine_t fileLine = {g_psz##tag##Alloc, 0};                      \
    CUtlMap<void *, MemAllocFileLine_t, i32>::IndexType_t iRecordedFileLine =  \
        g_##tag##Allocs.Find(p);                                               \
    if (iRecordedFileLine != g_##tag##Allocs.InvalidIndex()) {                 \
      fileLine = g_##tag##Allocs[iRecordedFileLine];                           \
      g_##tag##Allocs.RemoveAt(iRecordedFileLine);                             \
    }                                                                          \
                                                                               \
    MemAlloc_RegisterDeallocation(fileLine.pszFile, fileLine.line, size, size, \
                                  0);                                          \
  }

#else

#define MEMALLOC_DEFINE_EXTERNAL_TRACKING(tag)
#define MemAlloc_RegisterExternalAllocation(tag, p, size) ((void)0)
#define MemAlloc_RegisterExternalDeallocation(tag, p, size) ((void)0)

#endif

//-----------------------------------------------------------------------------

#endif  // !STEAM && !NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------

#if !defined(STEAM) && defined(NO_MALLOC_OVERRIDE)

#define MEM_ALLOC_CREDIT_(tag) ((void)0)
#define MEM_ALLOC_CREDIT() MEM_ALLOC_CREDIT_(__FILE__)
#define MEM_ALLOC_CREDIT_CLASS()
#define MEM_ALLOC_CLASSNAME(type) nullptr

#endif  // !STEAM && NO_MALLOC_OVERRIDE

//-----------------------------------------------------------------------------

#endif  // SOURCE_TIER0_MEMALLOC_H_
