// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: This header, which must be the final include in a .cpp (or .h) file,
// causes all crt methods to use debugging versions of the memory allocators.
// NOTE: Use memdbgoff.h to disable memory debugging.

// SPECIAL NOTE! This file must *not* use include guards; we need to be able
// to include this potentially multiple times (since we can deactivate debugging
// by including memdbgoff.h)

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include "base/include/base_types.h"
#include "build/include/build_config.h"

// SPECIAL NOTE #2: This must be the final include in a .cpp or .h file!!!

#if !defined(NDEBUG) && !defined(USE_MEM_DEBUG)
#define USE_MEM_DEBUG 1
#endif

// If debug build or ndebug and not already included MS custom alloc files, or
// already included this file
#if (!defined(NDEBUG) || !defined(_INC_CRTDBG)) || defined(MEMDBGON_H)

#include <malloc.h>
#include "base/include/macros.h"
#include "tier0/include/memalloc.h"

#if defined(USE_MEM_DEBUG)
#if defined(OS_POSIX)

#define SOURCE_MEM_NORMAL_BLOCK 1

#include <glob.h>
#include <sys/types.h>
#include <cstddef>
#include <new>

#if !defined(DID_THE_OPERATOR_NEW)
#define DID_THE_OPERATOR_NEW
inline void *operator new(usize nSize, i32 blah, const ch *pFileName,
                          i32 nLine) {
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}
inline void *operator new[](usize nSize, i32 blah, const ch *pFileName,
                            i32 nLine) {
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}
#endif

#else  // defined(OS_POSIX)

// Include crtdbg.h and make sure _DEBUG is set to 1.
#if defined(NDEBUG)
#define _DEBUG 1
#include <crtdbg.h>
#undef _DEBUG
#else
#include <crtdbg.h>
#endif  // defined(NDEBUG)

#ifndef SOURCE_MEM_NORMAL_BLOCK
#define SOURCE_MEM_NORMAL_BLOCK _NORMAL_BLOCK
#endif

#endif  // defined(OS_POSIX)
#endif

#include "tier0/include/memdbgoff.h"

// Debug/non-debug agnostic elements

#define MEM_OVERRIDE_ON 1

#undef malloc
#undef realloc
#undef calloc
#undef _expand
#undef free
#undef _msize
#undef _aligned_malloc
#undef _aligned_free

#ifndef MEMDBGON_H
inline void *MemAlloc_InlineCallocMemset(void *pMem, usize nCount,
                                         usize nElementSize) {
  memset(pMem, 0, nElementSize * nCount);
  return pMem;
}
#endif

#define calloc(c, s) MemAlloc_InlineCallocMemset(malloc((c) * (s)), (c), (s))
#define free(p) g_pMemAlloc->Free(p)
#define _msize(p) g_pMemAlloc->GetSize(p)
#define _expand(p, s) _expand_NoLongerSupported(p, s)
#define _aligned_free(p) MemAlloc_FreeAligned(p)

// Debug path
#if defined(USE_MEM_DEBUG)

#define malloc(s) g_pMemAlloc->Alloc(s, __FILE__, __LINE__)
#define realloc(p, s) g_pMemAlloc->Realloc(p, s, __FILE__, __LINE__)
#define _aligned_malloc(s, a) MemAlloc_AllocAligned(s, a, __FILE__, __LINE__)

#define _malloc_dbg(s, t, f, l) WHYCALLINGTHISDIRECTLY(s)

#if defined(__AFX_H__) && defined(DEBUG_NEW)
#define new DEBUG_NEW
#else
#undef new
#define MEMALL_DEBUG_NEW new (SOURCE_MEM_NORMAL_BLOCK, __FILE__, __LINE__)
#define new MEMALL_DEBUG_NEW
#endif

#undef _strdup
#undef strdup
#undef _wcsdup
#undef wcsdup

#define _strdup(s) MemAlloc_StrDup(s, __FILE__, __LINE__)
#define strdup(s) MemAlloc_StrDup(s, __FILE__, __LINE__)
#define _wcsdup(s) MemAlloc_WcStrDup(s, __FILE__, __LINE__)
#define wcsdup(s) MemAlloc_WcStrDup(s, __FILE__, __LINE__)

// Make sure we don't define strdup twice
#if !defined(MEMDBGON_H)

inline ch *MemAlloc_StrDup(const ch *pString, const ch *pFileName, u32 nLine) {
  ch *pMemory;

  if (!pString) return nullptr;

  usize len = strlen(pString) + 1;
  if ((pMemory = (ch *)g_pMemAlloc->Alloc(len, pFileName, nLine)) != nullptr) {
    return strcpy(pMemory, pString);
  }

  return nullptr;
}

inline wch *MemAlloc_WcStrDup(const wch *pString, const ch *pFileName,
                              u32 nLine) {
  wch *pMemory;

  if (!pString) return nullptr;

  usize len = (wcslen(pString) + 1);
  if ((pMemory = (wch *)g_pMemAlloc->Alloc(len * sizeof(wch), pFileName,
                                           nLine)) != nullptr) {
    return wcscpy(pMemory, pString);
  }

  return nullptr;
}

#endif  // DBMEM_DEFINED_STRDUP

#else
// Release path

#define malloc(s) g_pMemAlloc->Alloc(s)
#define realloc(p, s) g_pMemAlloc->Realloc(p, s)
#define _aligned_malloc(s, a) MemAlloc_AllocAligned(s, a)

#ifndef _malloc_dbg
#define _malloc_dbg(s, t, f, l) WHYCALLINGTHISDIRECTLY(s)
#endif

#undef new

#undef _strdup
#undef strdup
#undef _wcsdup
#undef wcsdup

#define _strdup(s) MemAlloc_StrDup(s)
#define strdup(s) MemAlloc_StrDup(s)
#define _wcsdup(s) MemAlloc_WcStrDup(s)
#define wcsdup(s) MemAlloc_WcStrDup(s)

// Make sure we don't define strdup twice
#if !defined(MEMDBGON_H)

inline ch *MemAlloc_StrDup(const ch *pString) {
  ch *pMemory;

  if (!pString) return nullptr;

  usize len = strlen(pString) + 1;
  if ((pMemory = (ch *)g_pMemAlloc->Alloc(len)) != nullptr) {
    strcpy_s(pMemory, len, pString);
    return pMemory;
  }

  return nullptr;
}

inline wch *MemAlloc_WcStrDup(const wch *pString) {
  wch *pMemory;

  if (!pString) return nullptr;

  usize len = (wcslen(pString) + 1);
  if ((pMemory = (wch *)g_pMemAlloc->Alloc(len * sizeof(wch))) != nullptr) {
    wcscpy_s(pMemory, len, pString);
    return pMemory;
  }

  return nullptr;
}

#endif  // DBMEM_DEFINED_STRDUP

#endif  // USE_MEM_DEBUG

#define MEMDBGON_H  // Defined here so can be used above

#else

#if defined(USE_MEM_DEBUG)
#ifndef _STATIC_LINKED
#pragma message( \
    "Note: file includes crtdbg.h directly, therefore will cannot use memdbgon.h in non-debug build")
#else
#error \
    "Error: file includes crtdbg.h directly, therefore will cannot use memdbgon.h in non-debug build. Not recoverable in static build"
#endif
#endif
#endif  // _INC_CRTDBG

#endif  // !STEAM && !NO_MALLOC_OVERRIDE
