// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator.

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#undef PROTECTED_THINGS_ENABLE  // allow use of _vsnprintf

#include "build/include/build_config.h"

#ifdef OS_WIN
// ARG: crtdbg is necessary for certain definitions below,
// but it also redefines malloc as a macro in release.
// To disable this, we gotta define _DEBUG before including it.. BLEAH!
#define _DEBUG 1
#include <crtdbg.h>
#ifdef NDEBUG
#undef _DEBUG
#endif

#elif OS_POSIX
#define __cdecl
#endif

#include <cassert>
#include <cstdio>
#include <cstring>

#ifdef OS_WIN
#include <cstdlib>
#include "base/include/windows/windows_light.h"
#include "tier0/include/minidump.h"
#endif

#include "tier0/include/memdbgoff.h"

#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/platform.h"

// Tags this DLL as debug
#ifdef _DEBUG
SOURCE_API_EXPORT void BuiltDebug() {}
#endif

#ifdef OS_WIN
const char *MakeModuleFileName() {
  if (g_pMemAlloc->IsDebugHeap()) {
    char *pszModuleName = (char *)HeapAlloc(
        GetProcessHeap(), 0, SOURCE_MAX_PATH);  // small leak, debug only

    MEMORY_BASIC_INFORMATION mbi;
    static int dummy;
    VirtualQuery(&dummy, &mbi, sizeof(mbi));

    GetModuleFileName(reinterpret_cast<HMODULE>(mbi.AllocationBase),
                      pszModuleName, SOURCE_MAX_PATH);
    char *pDot = strrchr(pszModuleName, '.');
    if (pDot) {
      char *pSlash = strrchr(pszModuleName, '\\');
      if (pSlash) {
        pszModuleName = pSlash + 1;
        *pDot = 0;
      }
    }

    return pszModuleName;
  }
  return NULL;
}

static void *AllocUnattributed(size_t nSize) {
  static const char *pszOwner = MakeModuleFileName();

  if (!pszOwner)
    return g_pMemAlloc->Alloc(nSize);
  else
    return g_pMemAlloc->Alloc(nSize, pszOwner, 0);
}

static void *ReallocUnattributed(void *pMem, size_t nSize) {
  static const char *pszOwner = MakeModuleFileName();

  if (!pszOwner)
    return g_pMemAlloc->Realloc(pMem, nSize);
  else
    return g_pMemAlloc->Realloc(pMem, nSize, pszOwner, 0);
}

#else
#define MakeModuleFileName() NULL
inline void *AllocUnattributed(size_t nSize) {
  return g_pMemAlloc->Alloc(nSize);
}

inline void *ReallocUnattributed(void *pMem, size_t nSize) {
  return g_pMemAlloc->Realloc(pMem, nSize);
}
#endif

// Standard functions in the CRT that we're going to override to call our
// allocator

#if defined(OS_WIN) && !defined(_STATIC_LINKED)
// This magic only works under win32
// under linux this malloc() overrides the libc malloc() and so we
// end up in a recursion (as g_pMemAlloc->Alloc() calls malloc)
#ifdef COMPILER_MSVC
#define ALLOC_CALL _CRTALLOCATOR _CRT_JIT_INTRINSIC _CRTRESTRICT
#define FREE_CALL
#else
#define ALLOC_CALL
#define FREE_CALL
#endif

extern "C" {

ALLOC_CALL void *malloc(size_t nSize) { return AllocUnattributed(nSize); }

FREE_CALL void free(void *pMem) { g_pMemAlloc->Free(pMem); }

ALLOC_CALL void *realloc(void *pMem, size_t nSize) {
  return ReallocUnattributed(pMem, nSize);
}

ALLOC_CALL void *calloc(size_t nCount, size_t nElementSize) {
  void *pMem = AllocUnattributed(nElementSize * nCount);
  memset(pMem, 0, nElementSize * nCount);
  return pMem;
}

}  // end extern "C"

// Non-standard MSVC functions that we're going to override to call our
// allocator

extern "C" {
// crt
void *__cdecl _malloc_crt(size_t size) { return AllocUnattributed(size); }

void *__cdecl _calloc_crt(size_t count, size_t size) {
  return _calloc_base(count, size);
}

void *__cdecl _realloc_crt(void *ptr, size_t size) {
  return _realloc_base(ptr, size);
}

void *__cdecl _recalloc_crt(void *ptr, size_t count, size_t size) {
  return _recalloc_base(ptr, count, size);
}

size_t __cdecl _msize_base(void *pMem) { return g_pMemAlloc->GetSize(pMem); }

size_t __cdecl _msize(void *pMem) { return _msize_base(pMem); }

size_t __cdecl msize(void *pMem) { return g_pMemAlloc->GetSize(pMem); }
//-V524

void *__cdecl _heap_alloc(size_t nSize) { return AllocUnattributed(nSize); }

void *__cdecl _nh_malloc(size_t nSize, int) { return AllocUnattributed(nSize); }

void *__cdecl _expand(void *pMem, size_t nSize) {
  Assert(0);
  return NULL;
}

unsigned int _amblksiz = 16;  // BYTES_PER_PARA;

HANDLE _crtheap = (HANDLE)1;  // PatM Can't be 0 or CRT pukes
int __active_heap = 1;

size_t __cdecl _get_sbh_threshold() { return 0; }

int __cdecl _set_sbh_threshold(size_t) { return 0; }

int __cdecl _heapchk() { return g_pMemAlloc->heapchk(); }

int __cdecl _heapmin() { return 1; }

int __cdecl _heapadd(void *, size_t) { return 0; }

int __cdecl _heapset(unsigned int) { return 0; }

size_t __cdecl _heapused(size_t *, size_t *) { return 0; }

#ifdef OS_WIN
int __cdecl _heapwalk(_HEAPINFO *) { return 0; }
#endif

}  // end extern "C"

// Debugging functions that we're going to override to call our allocator
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!

extern "C" {

void *malloc_db(size_t nSize, const char *pFileName, int nLine) {
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

void free_db(void *pMem, const char *pFileName, int nLine) {
  g_pMemAlloc->Free(pMem, pFileName, nLine);
}

void *realloc_db(void *pMem, size_t nSize, const char *pFileName, int nLine) {
  return g_pMemAlloc->Realloc(pMem, nSize, pFileName, nLine);
}

}  // end extern "C"

// These methods are standard MSVC heap initialization + shutdown methods

extern "C" {
int __cdecl _heap_init() { return g_pMemAlloc != nullptr; }

void __cdecl _heap_term() {}
}
#endif

// Prevents us from using an inappropriate new or delete method,
// ensures they are here even when linking against debug or release static
// libs

#ifndef NO_MEMOVERRIDE_NEW_DELETE
#ifdef OSX
void *__cdecl operator new(size_t nSize) throw(std::bad_alloc)
#else
void *__cdecl operator new(size_t nSize)
#endif
{
  return AllocUnattributed(nSize);
}

void *__cdecl operator new(size_t nSize, int nBlockUse, const char *pFileName,
                           int nLine) {
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

#ifdef OSX
void __cdecl operator delete(void *pMem) throw()
#else
void __cdecl operator delete(void *pMem)
#endif
{
  g_pMemAlloc->Free(pMem);
}

#ifdef OSX
void *__cdecl operator new[](size_t nSize) throw(std::bad_alloc)
#else
void *__cdecl operator new[](size_t nSize)
#endif
{
  return AllocUnattributed(nSize);
}

void *__cdecl operator new[](size_t nSize, int nBlockUse, const char *pFileName,
                             int nLine) {
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

#ifdef OSX
void __cdecl operator delete[](void *pMem) throw()
#else
void __cdecl operator delete[](void *pMem)
#endif
{
  g_pMemAlloc->Free(pMem);
}
#endif

// Override some debugging allocation methods in MSVC
// NOTE: These have to be here for release + debug builds in case we
// link to a debug static lib!!!

#ifndef _STATIC_LINKED
#ifdef OS_WIN

// This here just hides the internal file names, etc of allocations
// made in the c runtime library
#define CRT_INTERNAL_FILE_NAME "C-runtime internal"

class CAttibCRT {
 public:
  CAttibCRT(int nBlockUse) : m_nBlockUse(nBlockUse) {
    if (m_nBlockUse == _CRT_BLOCK) {
      g_pMemAlloc->PushAllocDbgInfo(CRT_INTERNAL_FILE_NAME, 0);
    }
  }

  ~CAttibCRT() {
    if (m_nBlockUse == _CRT_BLOCK) {
      g_pMemAlloc->PopAllocDbgInfo();
    }
  }

 private:
  int m_nBlockUse;
};

#define AttribIfCrt() CAttibCRT _attrib(nBlockUse)
#elif defined(OS_POSIX)
#define AttribIfCrt()
#endif  // OS_WIN

extern "C" {

void *__cdecl _nh_malloc_dbg(size_t nSize, int nFlag, int nBlockUse,
                             const char *pFileName, int nLine) {
  AttribIfCrt();
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

void *__cdecl _malloc_dbg(size_t nSize, int nBlockUse, const char *pFileName,
                          int nLine) {
  AttribIfCrt();
  return g_pMemAlloc->Alloc(nSize, pFileName, nLine);
}

void *__cdecl _calloc_dbg(size_t nNum, size_t nSize, int nBlockUse,
                          const char *pFileName, int nLine) {
  AttribIfCrt();
  void *pMem = g_pMemAlloc->Alloc(nSize * nNum, pFileName, nLine);
  memset(pMem, 0, nSize * nNum);
  return pMem;
}

void *__cdecl _calloc_dbg_impl(size_t nNum, size_t nSize, int nBlockUse,
                               const char *szFileName, int nLine,
                               int *errno_tmp) {
  return _calloc_dbg(nNum, nSize, nBlockUse, szFileName, nLine);
}

void *__cdecl _realloc_dbg(void *pMem, size_t nNewSize, int nBlockUse,
                           const char *pFileName, int nLine) {
  AttribIfCrt();
  return g_pMemAlloc->Realloc(pMem, nNewSize, pFileName, nLine);
}

void *__cdecl _expand_dbg(void *pMem, size_t nNewSize, int nBlockUse,
                          const char *pFileName, int nLine) {
  Assert(0);
  return NULL;
}

void __cdecl _free_dbg(void *pMem, int nBlockUse) {
  AttribIfCrt();
  g_pMemAlloc->Free(pMem);
}

size_t __cdecl _msize_dbg(void *pMem, int nBlockUse) {
#ifdef OS_WIN
  return _msize(pMem);
#elif OS_POSIX
  Assert("_msize_dbg unsupported");
  return 0;
#endif
}

#ifdef OS_WIN

#if defined(_DEBUG)
// aligned base
ALLOC_CALL void *__cdecl _aligned_malloc_base(size_t size, size_t align) {
  return MemAlloc_AllocAligned(size, align);
}

ALLOC_CALL void *__cdecl _aligned_realloc_base(void *ptr, size_t size,
                                               size_t align) {
  return MemAlloc_ReallocAligned(ptr, size, align);
}

ALLOC_CALL void *__cdecl _aligned_recalloc_base(void *ptr, size_t size,
                                                size_t align) {
  Error("Unsupported function\n");
  return NULL;
}

FREE_CALL void __cdecl _aligned_free_base(void *ptr) {
  MemAlloc_FreeAligned(ptr);
}

// aligned
ALLOC_CALL void *__cdecl _aligned_malloc(size_t size, size_t align) {
  return _aligned_malloc_base(size, align);
}

ALLOC_CALL void *__cdecl _aligned_realloc(void *memblock, size_t size,
                                          size_t align) {
  return _aligned_realloc_base(memblock, size, align);
}

ALLOC_CALL void *__cdecl _aligned_recalloc(void *memblock, size_t count,
                                           size_t size, size_t align) {
  return _aligned_recalloc_base(memblock, count * size, align);
}

FREE_CALL void __cdecl _aligned_free(void *memblock) {
  _aligned_free_base(memblock);
}

// aligned offset base
ALLOC_CALL void *__cdecl _aligned_offset_malloc_base(size_t size, size_t align,
                                                     size_t offset) {
  return NULL;
}

ALLOC_CALL void *__cdecl _aligned_offset_realloc_base(void *memblock,
                                                      size_t size, size_t align,
                                                      size_t offset) {
  return NULL;
}

ALLOC_CALL void *__cdecl _aligned_offset_recalloc_base(void *memblock,
                                                       size_t size,
                                                       size_t align,
                                                       size_t offset) {
  return NULL;
}

// aligned offset
ALLOC_CALL void *__cdecl _aligned_offset_malloc(size_t size, size_t align,
                                                size_t offset) {
  return _aligned_offset_malloc_base(size, align, offset);
}

ALLOC_CALL void *__cdecl _aligned_offset_realloc(void *memblock, size_t size,
                                                 size_t align, size_t offset) {
  return _aligned_offset_realloc_base(memblock, size, align, offset);
}

ALLOC_CALL void *__cdecl _aligned_offset_recalloc(void *memblock, size_t count,
                                                  size_t size, size_t align,
                                                  size_t offset) {
  return _aligned_offset_recalloc_base(memblock, count * size, align, offset);
}

#endif

#endif

}  // end extern "C"

// Override some the _CRT debugging allocation methods in MSVC

#ifdef OS_WIN

extern "C" {

int _CrtDumpMemoryLeaks() { return 0; }

_CRT_DUMP_CLIENT _CrtSetDumpClient(_CRT_DUMP_CLIENT dumpClient) { return NULL; }

int _CrtSetDbgFlag(int nNewFlag) {
  return g_pMemAlloc->CrtSetDbgFlag(nNewFlag);
}

// 64-bit port.
static int s_crtDbgFlag = _CRTDBG_ALLOC_MEM_DF;
int *__cdecl __p__crtDbgFlag(void) { return &s_crtDbgFlag; }

static long s_crtBreakAlloc = 0; /* Break on this allocation */
long *__cdecl __p__crtBreakAlloc(void) { return &s_crtBreakAlloc; }

void __cdecl _CrtSetDbgBlockType(void *pMem, int nBlockUse) { DebuggerBreak(); }

_CRT_ALLOC_HOOK __cdecl _CrtSetAllocHook(_CRT_ALLOC_HOOK pfnNewHook) {
  DebuggerBreak();
  return NULL;
}

long __cdecl _CrtSetBreakAlloc(long lNewBreakAlloc) {
  return g_pMemAlloc->CrtSetBreakAlloc(lNewBreakAlloc);
}

int __cdecl _CrtIsValidHeapPointer(const void *pMem) {
  return g_pMemAlloc->CrtIsValidHeapPointer(pMem);
}

int __cdecl _CrtIsValidPointer(const void *pMem, unsigned int size,
                               int access) {
  return g_pMemAlloc->CrtIsValidPointer(pMem, size, access);
}

int __cdecl _CrtCheckMemory() {
  // TODO(d.rattman): Remove this when we re-implement the heap
  return g_pMemAlloc->CrtCheckMemory();
}

int __cdecl _CrtIsMemoryBlock(const void *pMem, unsigned int nSize,
                              long *plRequestNumber, char **ppFileName,
                              int *pnLine) {
  DebuggerBreak();
  return 1;
}

int __cdecl _CrtMemDifference(_CrtMemState *pState,
                              const _CrtMemState *oldState,
                              const _CrtMemState *newState) {
  DebuggerBreak();
  return FALSE;
}

void __cdecl _CrtMemDumpStatistics(const _CrtMemState *pState) {
  DebuggerBreak();
}

void __cdecl _CrtMemCheckpoint(_CrtMemState *pState) {
  // TODO(d.rattman): Remove this when we re-implement the heap
  g_pMemAlloc->CrtMemCheckpoint(pState);
}

void __cdecl _CrtMemDumpAllObjectsSince(const _CrtMemState *pState) {
  DebuggerBreak();
}

void __cdecl _CrtDoForAllClientObjects(void (*pfn)(void *, void *),
                                       void *pContext) {
  DebuggerBreak();
}

// Methods in dbgrpt.cpp

long _crtAssertBusy = -1;

int __cdecl _CrtSetReportMode(int nReportType, int nReportMode) {
  return g_pMemAlloc->CrtSetReportMode(nReportType, nReportMode);
}

_HFILE __cdecl _CrtSetReportFile(int nRptType, _HFILE hFile) {
  return (_HFILE)g_pMemAlloc->CrtSetReportFile(nRptType, hFile);
}

_CRT_REPORT_HOOK __cdecl _CrtSetReportHook(_CRT_REPORT_HOOK pfnNewHook) {
  return (_CRT_REPORT_HOOK)g_pMemAlloc->CrtSetReportHook(pfnNewHook);
}

int __cdecl _CrtDbgReport(int nRptType, const char *szFile, int nLine,
                          const char *szModule, const char *szFormat, ...) {
  static char output[1024];
  va_list args;
  if (szFormat) {
    va_start(args, szFormat);
    _vsnprintf_s(output, SOURCE_ARRAYSIZE(output) - 1, szFormat, args);
    va_end(args);
  } else {
    output[0] = '\0';
  }

  return g_pMemAlloc->CrtDbgReport(nRptType, szFile, nLine, szModule, output);
}

// Configure VS so that it will record crash dumps on pure-call violations
// and invalid parameter handlers.
// If you manage to call a pure-virtual function (easily done if you
// indirectly / call a pure-virtual function from the base-class constructor or
// destructor) / or if you invoke the invalid parameter handler (printf(NULL);
// is one way) / then no crash dump will be created. / This crash redirects the
// handlers for these two events so that crash dumps / are created.
//
// The ErrorHandlerRegistrar object must be in memoverride.cc so that it will
// be placed in every DLL and EXE. This is required because each DLL and EXE
// gets its own copy of the C run-time and these overrides are set on a
// per-CRT / basis.
/*
// This sample code will cause pure-call and invalid_parameter violations and
// was used for testing:
 class Base
{
 public:
 virtual void PureFunction() = 0;

 Base()
{
 NonPureFunction();
}

 void NonPureFunction()
{
 PureFunction();
}
};

 class Derived : public Base
{
 public:
 void PureFunction() OVERRIDE
{
}
};

 void PureCallViolation()
{
 Derived derived;
}

 void InvalidParameterViolation()
{
 printf( NULL );
}
*/

// Disable compiler optimizations. If we don't do this then VC++
// generates code that confuses the Visual Studio debugger and causes it to
// display completely random call stacks.That makes the minidumps excruciatingly
// hard to understand.
#pragma optimize("", off)

// Write a minidump file, unless running under the debugger in which
// case break into the debugger. The "int dummy" parameter is so that the
// callers can be unique so that the linker won't use its opt:icf optimization
// to collapse them together.This makes reading the call stack easier.
[[noreturn]] void __cdecl WriteMiniDumpOrBreak(int dummy,
                                               const wchar_t *pchName) {
  if (Plat_IsInDebugSession()) {
    __debugbreak();
    // Continue at your peril...
  } else {
    WriteMiniDump();
    // Call Plat_ExitProcess so we don't continue in a bad state.
    TerminateProcess(GetCurrentProcess(), 0);
  }
}

void __cdecl VPureCall() {
  WriteMiniDumpOrBreak(0, L"Pure virtual function call");
}

void VInvalidParameterHandler(const wchar_t *expression,
                              const wchar_t *function, const wchar_t *file,
                              unsigned int line, uintptr_t pReserved) {
  wchar_t parameter_info[1024];
  swprintf_s(parameter_info, L"Invalid parameter: %s (func %s) (%u) (expr %s)",
             file, function, line, expression);

  WriteMiniDumpOrBreak(1, parameter_info);
}

// Restore compiler optimizations.
#pragma optimize("", on)

// Helper class for registering error callbacks. See above for details.
class ErrorHandlerRegistrar {
 public:
  ErrorHandlerRegistrar();
} s_ErrorHandlerRegistration;

ErrorHandlerRegistrar::ErrorHandlerRegistrar() {
  _set_purecall_handler(VPureCall);
  _set_invalid_parameter_handler(VInvalidParameterHandler);
}

#if defined(_DEBUG) || defined(USE_MEM_DEBUG)

int __cdecl __crtMessageWindowW(int nRptType, const wchar_t *szFile,
                                const wchar_t *szLine, const wchar_t *szModule,
                                const wchar_t *szUserMessage) {
  Assert(0);
  return 0;
}

int __cdecl _CrtDbgReportV(int nRptType, const wchar_t *szFile, int nLine,
                           const wchar_t *szModule, const wchar_t *szFormat,
                           va_list arglist) {
  Assert(0);
  return 0;
}

int __cdecl _CrtDbgReportW(int nRptType, const wchar_t *szFile, int nLine,
                           const wchar_t *szModule, const wchar_t *szFormat,
                           ...) {
  Assert(0);
  return 0;
}

int __cdecl _VCrtDbgReportA(_In_ int _ReportType, _In_opt_ void *_ReturnAddress,
                            _In_opt_z_ char const *_FileName,
                            _In_ int _LineNumber,
                            _In_opt_z_ char const *_ModuleName,
                            _In_opt_z_ char const *_Format, va_list _ArgList) {
  Assert(0);
  return 0;
}

int __cdecl _CrtSetReportHook2(int mode, _CRT_REPORT_HOOK pfnNewHook) {
  _CrtSetReportHook(pfnNewHook);
  return 0;
}

#endif /* defined( _DEBUG ) || defined( USE_MEM_DEBUG ) */

extern "C" int __crtDebugCheckCount = FALSE;

extern "C" int __cdecl _CrtSetCheckCount(int fCheckCount) {
  int oldCheckCount = __crtDebugCheckCount;
  return oldCheckCount;
}

extern "C" int __cdecl _CrtGetCheckCount() { return __crtDebugCheckCount; }

// aligned offset debug
extern "C" void *__cdecl _aligned_offset_recalloc_dbg(
    void *memblock, size_t count, size_t size, size_t align, size_t offset,
    const char *f_name, int line_n) {
  void *pMem = ReallocUnattributed(memblock, size * count);
  memset(pMem, 0, size * count);
  return pMem;
}

extern "C" void *__cdecl _aligned_recalloc_dbg(void *memblock, size_t count,
                                               size_t size, size_t align,
                                               const char *f_name, int line_n) {
  return _aligned_offset_recalloc_dbg(memblock, count, size, align, 0, f_name,
                                      line_n);
}

extern "C" void *__cdecl _recalloc_dbg(void *memblock, size_t count,
                                       size_t size, int nBlockUse,
                                       const char *szFileName, int nLine) {
  return _aligned_offset_recalloc_dbg(memblock, count, size, 0, 0, szFileName,
                                      nLine);
}

_CRT_REPORT_HOOK __cdecl _CrtGetReportHook() { return NULL; }
int __cdecl _CrtReportBlockType(const void *pUserData) { return 0; }

}  // end extern "C"
#endif  // OS_WIN

// Most files include this file, so when it's used it adds an extra .ValveDbg
// section, to help identify debug binaries.
#ifdef OS_WIN
#ifndef NDEBUG  // _DEBUG
#pragma data_seg("ValveDBG")
volatile const char *DBG = "*** DEBUG STUB ***";
#endif
#endif

#endif

// Extras added prevent dbgheap.obj from being included - DAL
#ifdef OS_WIN

extern "C" {
size_t __crtDebugFillThreshold = 0;

extern "C" void *__cdecl _heap_alloc_base(size_t size) {
  assert(0);
  return NULL;
}

void *__cdecl _heap_alloc_dbg(size_t nSize, int nBlockUse,
                              const char *szFileName, int nLine) {
  return _heap_alloc(nSize);
}

void __cdecl _free_nolock(void *pUserData) {
  // I don't think the second param is used in memoverride
  _free_dbg(pUserData, 0);
}

void __cdecl _free_dbg_nolock(void *pUserData, int nBlockUse) {
  _free_dbg(pUserData, 0);
}

_CRT_ALLOC_HOOK __cdecl _CrtGetAllocHook() {
  assert(0);
  return NULL;
}

_CRT_DUMP_CLIENT __cdecl _CrtGetDumpClient() {
  assert(0);
  return NULL;
}

void *__cdecl _aligned_malloc_dbg(size_t size, size_t align, const char *f_name,
                                  int line_n) {
  return _aligned_malloc(size, align);
}

void *__cdecl _aligned_realloc_dbg(void *memblock, size_t size, size_t align,
                                   const char *f_name, int line_n) {
  return _aligned_realloc(memblock, size, align);
}

void *__cdecl _aligned_offset_malloc_dbg(size_t size, size_t align,
                                         size_t offset, const char *f_name,
                                         int line_n) {
  return _aligned_offset_malloc(size, align, offset);
}

void *__cdecl _aligned_offset_realloc_dbg(void *memblock, size_t size,
                                          size_t align, size_t offset,
                                          const char *f_name, int line_n) {
  return _aligned_offset_realloc(memblock, size, align, offset);
}

void __cdecl _aligned_free_dbg(void *memblock) { _aligned_free(memblock); }

#ifndef _DEBUG
size_t __cdecl _CrtSetDebugFillThreshold(size_t _NewDebugFillThreshold) {
  assert(0);
  return 0;
}
#endif

//===========================================
// NEW!!! 64-bit

#ifndef _DEBUG
char *__cdecl _strdup(const char *string) {
  int nSize = (int)strlen(string) + 1;
  // Check for integer underflow.
  if (nSize <= 0) return NULL;
  char *pCopy = (char *)AllocUnattributed(nSize);
  if (pCopy) memcpy(pCopy, string, nSize);
  return pCopy;
}
#endif

wchar_t *__cdecl _wcsdup_dbg(const wchar_t *string, int nBlockUse,
                             const char *szFileName, int nLine) {
  Assert(0);
  return 0;
}

wchar_t *__cdecl _wcsdup(const wchar_t *string) {
  Assert(0);
  return 0;
}
}  // end extern "C"

#endif  // !STEAM && !NO_MALLOC_OVERRIDE

#endif  // OS_WIN
