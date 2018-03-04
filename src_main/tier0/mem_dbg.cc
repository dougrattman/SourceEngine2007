// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

#include <malloc.h>
#include <climits>
#include <cstring>
#include <map>
#include <set>

#include "mem_helpers.h"
#include "tier0/include/compiler_specific_macroses.h"
#include "tier0/include/dbg.h"
#include "tier0/include/memalloc.h"
#include "tier0/include/threadtools.h"

#ifdef OS_WIN
#include <crtdbg.h>
#endif

#if (defined(NDEBUG) && defined(USE_MEM_DEBUG))
#pragma message( \
    "USE_MEM_DEBUG is enabled in a release build. Don't check this in!")
#endif
#if (!defined(NDEBUG) || defined(USE_MEM_DEBUG))

#if defined(OS_WIN) && !defined(ARCH_CPU_X86_64)
// #define USE_STACK_WALK
// or:
// #define USE_STACK_WALK_DETAILED
#endif

#define DebugAlloc malloc
#define DebugFree free

i32 g_DefaultHeapFlags =
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF);

#if defined(_MEMTEST)
static ch s_szStatsMapName[32];
static ch s_szStatsComment[256];
#endif

#if defined(USE_STACK_WALK) || defined(USE_STACK_WALK_DETAILED)
#include <dbghelp.h>

#pragma comment(lib, "Dbghelp.lib")

#pragma auto_inline(off)
__declspec(naked) DWORD GetEIP() {
  __asm
  {
		mov eax, [ebp + 4]
		ret
  }
}

i32 WalkStack(void **ppAddresses, i32 nMaxAddresses, i32 nSkip = 0) {
  HANDLE hProcess = GetCurrentProcess();
  HANDLE hThread = GetCurrentThread();

  STACKFRAME64 frame;

  memset(&frame, 0, sizeof(frame));
  DWORD valEsp, valEbp;
  __asm
  {
		mov [valEsp], esp;
		mov [valEbp], ebp
  }
  frame.AddrPC.Offset = GetEIP();
  frame.AddrStack.Offset = valEsp;
  frame.AddrFrame.Offset = valEbp;
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;

  // Walk the stack.
  i32 nWalked = 0;
  nSkip++;
  while (nMaxAddresses - nWalked > 0) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_I386, hProcess, hThread, &frame,
                     nullptr, nullptr, SymFunctionTableAccess64,
                     SymGetModuleBase64, nullptr)) {
      break;
    }

    if (nSkip == 0) {
      if (frame.AddrFrame.Offset == 0) {
        // End of stack.
        break;
      }

      *ppAddresses++ = (void *)frame.AddrPC.Offset;
      nWalked++;

      if (frame.AddrPC.Offset == frame.AddrReturn.Offset) {
        // Catching a stack loop
        break;
      }
    } else {
      nSkip--;
    }
  }

  if (nMaxAddresses) {
    memset(ppAddresses, 0, (nMaxAddresses - nWalked) * sizeof(*ppAddresses));
  }

  return nWalked;
}

bool GetModuleFromAddress(void *address, ch *pResult) {
  IMAGEHLP_MODULE moduleInfo;

  moduleInfo.SizeOfStruct = sizeof(moduleInfo);

  if (SymGetModuleInfo(GetCurrentProcess(), (DWORD)address, &moduleInfo)) {
    strcpy(pResult, moduleInfo.ModuleName);
    return true;
  }

  return false;
}

bool GetCallerModule(ch *pDest) {
  static bool bInit;
  if (!bInit) {
    PSTR psUserSearchPath = nullptr;
    psUserSearchPath =
        "u:\\data\\game\\bin\\;u:\\data\\game\\episodic\\bin\\;u:"
        "\\data\\game\\hl2\\bin\\;\\\\perforce\\symbols";
    SymInitialize(GetCurrentProcess(), psUserSearchPath, true);
    bInit = true;
  }
  void *pCaller;
  WalkStack(&pCaller, 1, 2);

  return (pCaller != 0 && GetModuleFromAddress(pCaller, pDest));
}

#if defined(USE_STACK_WALK_DETAILED)

//
// Note: StackDescribe function is non-reentrant:
//		Reason:   Stack description is stored in a static buffer.
//		Solution: Passing caller-allocated buffers would allow the
//		function to become reentrant, however the current only client
//(FindOrCreateFilename) 		is synchronized with a heap mutex, after
// retrieving stack description the 		heap memory will be allocated to
// copy the text.
//

ch *StackDescribe(void **ppAddresses, i32 nMaxAddresses) {
  static ch s_chStackDescription[32 * 1024];
  static ch s_chSymbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 1024];

  IMAGEHLP_SYMBOL64 &hlpSymbol = *(IMAGEHLP_SYMBOL64 *)s_chSymbolBuffer;
  hlpSymbol.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
  hlpSymbol.MaxNameLength = 1024;
  DWORD64 hlpSymbolOffset = 0;

  IMAGEHLP_LINE64 hlpLine;
  hlpLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
  DWORD hlpLineOffset = 0;

  s_chStackDescription[0] = 0;
  ch *pchBuffer = s_chStackDescription;

  for (i32 k = 0; k < nMaxAddresses; ++k) {
    if (!ppAddresses[k]) break;

    pchBuffer += strlen(pchBuffer);
    if (SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)ppAddresses[k],
                             &hlpLineOffset, &hlpLine)) {
      ch const *pchFileName = hlpLine.FileName
                                  ? hlpLine.FileName + strlen(hlpLine.FileName)
                                  : nullptr;
      for (usize numSlashesAllowed = 2; pchFileName > hlpLine.FileName;
           --pchFileName) {
        if (*pchFileName == '\\') {
          if (numSlashesAllowed--)
            continue;
          else
            break;
        }
      }
      sprintf(pchBuffer, hlpLineOffset ? "%s:%d+0x%I32X" : "%s:%d", pchFileName,
              hlpLine.LineNumber, hlpLineOffset);
    } else if (SymGetSymFromAddr64(GetCurrentProcess(), (DWORD64)ppAddresses[k],
                                   &hlpSymbolOffset, &hlpSymbol)) {
      sprintf(pchBuffer,
              (hlpSymbolOffset > 0 && !(hlpSymbolOffset >> 63)) ? "%s+0x%I64X"
                                                                : "%s",
              hlpSymbol.Name, hlpSymbolOffset);
    } else {
      sprintf(pchBuffer, "#0x%08p", ppAddresses[k]);
    }

    pchBuffer += strlen(pchBuffer);
    sprintf(pchBuffer, "<--");
  }
  *pchBuffer = 0;

  return s_chStackDescription;
}

#endif  // #if defined( USE_STACK_WALK_DETAILED )

#else

inline i32 WalkStack(void **ppAddresses, i32 nMaxAddresses, i32 nSkip = 0) {
  memset(ppAddresses, 0, nMaxAddresses * sizeof(*ppAddresses));
  return 0;
}
#define GetModuleFromAddress(address, pResult) ((*pResult = 0), 0)
#define GetCallerModule(pDest) false
#endif

// NOTE: This exactly mirrors the dbg header in the MSDEV crt eventually when we
// write our own allocator, we can kill this
struct CrtDbgMemHeader_t {
  u8 m_Reserved[8];
  const ch *m_pFileName;
  i32 m_nLineNumber;
  u8 m_Reserved2[16];
};

struct DbgMemHeader_t
#ifdef NDEBUG
    : CrtDbgMemHeader_t
#endif
{
  usize nLogicalSize;
  u8 reserved[12];  // MS allocator always returns mem aligned on 16 bytes,
                    // which some of our code depends on
};

#ifndef NDEBUG
#define GetCrtDbgMemHeader(pMem) \
  ((CrtDbgMemHeader_t *)((DbgMemHeader_t *)pMem - 1) - 1)
#else
#define GetCrtDbgMemHeader(pMem) ((DbgMemHeader_t *)pMem - 1)
#endif

inline void *InternalMalloc(usize nSize, const ch *pFileName, i32 nLine) {
  DbgMemHeader_t *pInternalMem;
#ifdef NDEBUG
  pInternalMem = (DbgMemHeader_t *)malloc(nSize + sizeof(DbgMemHeader_t));
  if (!pInternalMem) return nullptr;

  pInternalMem->m_pFileName = pFileName;
  pInternalMem->m_nLineNumber = nLine;
#else
  pInternalMem = (DbgMemHeader_t *)_malloc_dbg(nSize + sizeof(DbgMemHeader_t),
                                               _NORMAL_BLOCK, pFileName, nLine);
  if (!pInternalMem) return nullptr;
#endif

  pInternalMem->nLogicalSize = nSize;
  return pInternalMem + 1;
}

inline void *InternalRealloc(void *pMem, usize nNewSize, const ch *pFileName,
                             i32 nLine) {
  if (!pMem) return InternalMalloc(nNewSize, pFileName, nLine);

  DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#ifdef NDEBUG
  pInternalMem = (DbgMemHeader_t *)realloc(pInternalMem,
                                           nNewSize + sizeof(DbgMemHeader_t));
  if (!pInternalMem) return nullptr;

  pInternalMem->m_pFileName = pFileName;
  pInternalMem->m_nLineNumber = nLine;
#else
  pInternalMem = (DbgMemHeader_t *)_realloc_dbg(
      pInternalMem, nNewSize + sizeof(DbgMemHeader_t), _NORMAL_BLOCK, pFileName,
      nLine);
  if (!pInternalMem) return nullptr;
#endif

  pInternalMem->nLogicalSize = nNewSize;
  return pInternalMem + 1;
}

inline void InternalFree(void *pMem) {
  if (!pMem) return;

  DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#ifdef NDEBUG
  free(pInternalMem);
#else
  _free_dbg(pInternalMem, _NORMAL_BLOCK);
#endif
}

inline usize InternalMSize(void *pMem) {
  DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
#ifdef OS_POSIX
  return pInternalMem->nLogicalSize;
#else
#ifdef NDEBUG
  return _msize(pInternalMem) - sizeof(DbgMemHeader_t);
#else
  return _msize_dbg(pInternalMem, _NORMAL_BLOCK) - sizeof(DbgMemHeader_t);
#endif
#endif  // OS_POSIX
}

inline usize InternalLogicalSize(void *pMem) {
  DbgMemHeader_t *pInternalMem = (DbgMemHeader_t *)pMem - 1;
  return pInternalMem->nLogicalSize;
}

#ifdef NDEBUG
#define _CrtDbgReport(nRptType, szFile, nLine, szModule, pMsg) 0
#endif

// Custom allocator protects this module from recursing on operator new
template <class T>
class CNoRecurseAllocator {
 public:
  // type definitions
  typedef T value_type;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef std::size_t size_type;
  typedef std::isize difference_type;

  CNoRecurseAllocator() {}
  CNoRecurseAllocator(const CNoRecurseAllocator &) {}
  template <class U>
  CNoRecurseAllocator(const CNoRecurseAllocator<U> &) {}
  ~CNoRecurseAllocator() {}

  // rebind allocator to type U
  template <class U>
  struct rebind {
    typedef CNoRecurseAllocator<U> other;
  };

  // return address of values
  pointer address(reference value) const { return &value; }

  const_pointer address(const_reference value) const { return &value; }
  size_type max_size() const { return INT_MAX; }

  pointer allocate(size_type num, const void * = 0) {
    return (pointer)DebugAlloc(num * sizeof(T));
  }
  void deallocate(pointer p, size_type num) { DebugFree(p); }
  void construct(pointer p, const T &value) { new ((void *)p) T(value); }
  void destroy(pointer p) { p->~T(); }
};

template <class T1, class T2>
bool operator==(const CNoRecurseAllocator<T1> &,
                const CNoRecurseAllocator<T2> &) {
  return true;
}

template <class T1, class T2>
bool operator!=(const CNoRecurseAllocator<T1> &,
                const CNoRecurseAllocator<T2> &) {
  return false;
}

class CStringLess {
 public:
  bool operator()(const ch *pszLeft, const ch *pszRight) const {
    return stricmp(pszLeft, pszRight) < 0;
  }
};

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4074)  // warning C4074: initializers put in compiler
                            // reserved initialization area
#pragma init_seg(compiler)
MSVC_END_WARNING_OVERRIDE_SCOPE()

// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually

class CDbgMemAlloc : public IMemAlloc {
 public:
  CDbgMemAlloc();
  virtual ~CDbgMemAlloc();

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

  // FIXME: Remove when we have our own allocator
  virtual void *CrtSetReportFile(i32 nRptType, void *hFile);
  virtual void *CrtSetReportHook(void *pfnNewHook);
  virtual i32 CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                           const ch *szModule, const ch *szFormat);

  virtual i32 heapchk();

  virtual bool IsDebugHeap() { return true; }

  virtual i32 GetVersion() { return MEMALLOC_VERSION; }

  virtual void CompactHeap() {}

  virtual MemAllocFailHandler_t SetAllocFailHandler(
      MemAllocFailHandler_t pfnMemAllocFailHandler) {
    return nullptr;
  }  // debug heap doesn't attempt retries

#if defined(_MEMTEST)
  void SetStatsExtraInfo(const ch *pMapName, const ch *pComment) {
    strncpy(s_szStatsMapName, pMapName, sizeof(s_szStatsMapName));
    s_szStatsMapName[sizeof(s_szStatsMapName) - 1] = '\0';

    strncpy(s_szStatsComment, pComment, sizeof(s_szStatsComment));
    s_szStatsComment[sizeof(s_szStatsComment) - 1] = '\0';
  }
#endif

  virtual usize MemoryAllocFailed();
  void SetCRTAllocFailed(usize nMemSize);

  enum {
    BYTE_COUNT_16 = 0,
    BYTE_COUNT_32,
    BYTE_COUNT_128,
    BYTE_COUNT_1024,
    BYTE_COUNT_GREATER,

    NUM_BYTE_COUNT_BUCKETS
  };

 private:
  struct MemInfo_t {
    MemInfo_t() { memset(this, 0, sizeof(*this)); }

    // Size in bytes
    usize m_nCurrentSize;
    usize m_nPeakSize;
    usize m_nTotalSize;
    usize m_nOverheadSize;
    usize m_nPeakOverheadSize;

    // Count in terms of # of allocations
    usize m_nCurrentCount;
    usize m_nPeakCount;
    usize m_nTotalCount;

    // Count in terms of # of allocations of a particular size
    usize m_pCount[NUM_BYTE_COUNT_BUCKETS];

    // Time spent allocating + deallocating	(microseconds)
    u64 m_nTime;
  };

  struct MemInfoKey_t {
    MemInfoKey_t(const ch *pFileName, i32 line)
        : m_pFileName(pFileName), m_nLine(line) {}
    bool operator<(const MemInfoKey_t &key) const {
      i32 iret = stricmp(m_pFileName, key.m_pFileName);
      if (iret < 0) return true;

      if (iret > 0) return false;

      return m_nLine < key.m_nLine;
    }

    const ch *m_pFileName;
    i32 m_nLine;
  };

  // NOTE: Deliberately using STL here because the UTL stuff
  // is a client of this library; want to avoid circular dependency

  // Maps file name to info
  typedef std::map<
      MemInfoKey_t, MemInfo_t, std::less<MemInfoKey_t>,
      CNoRecurseAllocator<std::pair<const MemInfoKey_t, MemInfo_t> > >
      StatMap_t;
  typedef StatMap_t::iterator StatMapIter_t;
  typedef StatMap_t::value_type StatMapEntry_t;

  typedef std::set<const ch *, CStringLess, CNoRecurseAllocator<const ch *> >
      Filenames_t;

  // Heap reporting method
  typedef void (*HeapReportFunc_t)(ch const *pFormat, ...);

 private:
  // Returns the actual debug info
  void GetActualDbgInfo(const ch *&pFileName, i32 &nLine);

  // Finds the file in our map
  MemInfo_t &FindOrCreateEntry(const ch *pFileName, i32 line);
  const ch *FindOrCreateFilename(const ch *pFileName);

  // Updates stats
  void RegisterAllocation(const ch *pFileName, i32 nLine, usize nLogicalSize,
                          usize nActualSize, u32 nTime);
  void RegisterDeallocation(const ch *pFileName, i32 nLine, usize nLogicalSize,
                            usize nActualSize, u32 nTime);

  void RegisterAllocation(MemInfo_t &info, usize nLogicalSize,
                          usize nActualSize, u32 nTime);
  void RegisterDeallocation(MemInfo_t &info, usize nLogicalSize,
                            usize nActualSize, u32 nTime);

  // Gets the allocation file name
  const ch *GetAllocatonFileName(void *pMem);
  i32 GetAllocatonLineNumber(void *pMem);

  // FIXME: specify a spew output func for dumping stats
  // Stat output
  void DumpMemInfo(const ch *pAllocationName, i32 line, const MemInfo_t &info);
  void DumpFileStats();
  void DumpStats();
  void DumpStatsFileBase(ch const *pchFileBase);
  void DumpBlockStats(void *p);

 private:
  StatMap_t m_StatMap;
  MemInfo_t m_GlobalInfo;
  CFastTimer m_Timer;
  bool m_bInitialized;
  Filenames_t m_Filenames;

  HeapReportFunc_t m_OutputFunc;

  static usize s_pCountSizes[NUM_BYTE_COUNT_BUCKETS];
  static const ch *s_pCountHeader[NUM_BYTE_COUNT_BUCKETS];

  usize m_sMemoryAllocFailed;
};

static ch const *g_pszUnknown = "unknown";

const i32 DBG_INFO_STACK_DEPTH = 32;

struct DbgInfoStack_t {
  const ch *m_pFileName;
  i32 m_nLine;
};

CThreadLocalPtr<DbgInfoStack_t> g_DbgInfoStack CONSTRUCT_EARLY;
CThreadLocalInt<> g_nDbgInfoStackDepth CONSTRUCT_EARLY;

// Singleton...

static CDbgMemAlloc s_DbgMemAlloc CONSTRUCT_EARLY;

#ifndef TIER0_VALIDATE_HEAP
IMemAlloc *g_pMemAlloc = &s_DbgMemAlloc;
#else
IMemAlloc *g_pActualAlloc = &s_DbgMemAlloc;
#endif

CThreadMutex g_DbgMemMutex CONSTRUCT_EARLY;

#define HEAP_LOCK() AUTO_LOCK(g_DbgMemMutex)

// Byte count buckets

usize CDbgMemAlloc::s_pCountSizes[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = {
    16, 32, 128, 1024, INT_MAX};

const ch *CDbgMemAlloc::s_pCountHeader[CDbgMemAlloc::NUM_BYTE_COUNT_BUCKETS] = {
    "<=16 byte allocations", "17-32 byte allocations",
    "33-128 byte allocations", "129-1024 byte allocations",
    ">1024 byte allocations"};

// Standard output

static FILE *s_DbgFile;

static void DefaultHeapReportFunc(ch const *pFormat, ...) {
  va_list args;
  va_start(args, pFormat);
  vfprintf(s_DbgFile, pFormat, args);
  va_end(args);
}

// Constructor

CDbgMemAlloc::CDbgMemAlloc() : m_sMemoryAllocFailed((usize)0) {
  CClockSpeedInit::Init();

  m_OutputFunc = DefaultHeapReportFunc;
  m_bInitialized = true;

  if (!IsDebug()) {
    Plat_DebugString(
        "USE_MEM_DEBUG is enabled in a release build. Don't check this in!\n");
  }
}

CDbgMemAlloc::~CDbgMemAlloc() {
  Filenames_t::const_iterator iter = m_Filenames.begin();
  while (iter != m_Filenames.end()) {
    ch *pFileName = (ch *)(*iter);
    free(pFileName);
    iter++;
  }
  m_bInitialized = false;
}

// Release versions

void *CDbgMemAlloc::Alloc(usize nSize) {
  /*
          // NOTE: Uncomment this to find unknown allocations
          const ch *pFileName = g_pszUnknown;
          i32 nLine;
          GetActualDbgInfo( pFileName, nLine );
          if (pFileName == g_pszUnknown)
          {
                  i32 x = 3;
          }
  */
  ch szModule[MAX_PATH];
  if (GetCallerModule(szModule)) {
    return Alloc(nSize, szModule, 0);
  } else {
    return Alloc(nSize, g_pszUnknown, 0);
  }
  //	return malloc( nSize );
}

void *CDbgMemAlloc::Realloc(void *pMem, usize nSize) {
  /*
          // NOTE: Uncomment this to find unknown allocations
          const ch *pFileName = g_pszUnknown;
          i32 nLine;
          GetActualDbgInfo( pFileName, nLine );
          if (pFileName == g_pszUnknown)
          {
                  i32 x = 3;
          }
  */
  // FIXME: Should these gather stats?
  ch szModule[MAX_PATH];
  if (GetCallerModule(szModule)) {
    return Realloc(pMem, nSize, szModule, 0);
  } else {
    return Realloc(pMem, nSize, g_pszUnknown, 0);
  }
  //	return realloc( pMem, nSize );
}

void CDbgMemAlloc::Free(void *pMem) {
  // FIXME: Should these gather stats?
  Free(pMem, g_pszUnknown, 0);
  //	free( pMem );
}

void *CDbgMemAlloc::Expand_NoLongerSupported(void *pMem, usize nSize) {
  return nullptr;
}

// Force file + line information for an allocation

void CDbgMemAlloc::PushAllocDbgInfo(const ch *pFileName, i32 nLine) {
  if (g_DbgInfoStack == nullptr) {
    g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc(sizeof(DbgInfoStack_t) *
                                                  DBG_INFO_STACK_DEPTH);
    g_nDbgInfoStackDepth = -1;
  }

  ++g_nDbgInfoStackDepth;
  Assert(g_nDbgInfoStackDepth < DBG_INFO_STACK_DEPTH);
  g_DbgInfoStack[g_nDbgInfoStackDepth].m_pFileName =
      FindOrCreateFilename(pFileName);
  g_DbgInfoStack[g_nDbgInfoStackDepth].m_nLine = nLine;
}

void CDbgMemAlloc::PopAllocDbgInfo() {
  if (g_DbgInfoStack == nullptr) {
    g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc(sizeof(DbgInfoStack_t) *
                                                  DBG_INFO_STACK_DEPTH);
    g_nDbgInfoStackDepth = -1;
  }

  --g_nDbgInfoStackDepth;
  Assert(g_nDbgInfoStackDepth >= -1);
}

// Returns the actual debug info

void CDbgMemAlloc::GetActualDbgInfo(const ch *&pFileName, i32 &nLine) {
#if defined(USE_STACK_WALK_DETAILED)
  return;
#endif

  if (g_DbgInfoStack == nullptr) {
    g_DbgInfoStack = (DbgInfoStack_t *)DebugAlloc(sizeof(DbgInfoStack_t) *
                                                  DBG_INFO_STACK_DEPTH);
    g_nDbgInfoStackDepth = -1;
  }

  if (g_nDbgInfoStackDepth >= 0 && g_DbgInfoStack[0].m_pFileName) {
    pFileName = g_DbgInfoStack[0].m_pFileName;
    nLine = g_DbgInfoStack[0].m_nLine;
  }
}

//

const ch *CDbgMemAlloc::FindOrCreateFilename(const ch *pFileName) {
  // If we created it for the first time, actually *allocate* the filename
  // memory
  HEAP_LOCK();
  // This is necessary for shutdown conditions: the file name is stored
  // in some piece of memory in a DLL; if that DLL becomes unloaded,
  // we'll have a pointer to crap memory

  if (!pFileName) {
    pFileName = g_pszUnknown;
  }

#if defined(USE_STACK_WALK_DETAILED)
  {
    // Walk the stack to determine what's causing the allocation
    void *arrStackAddresses[10] = {0};
    i32 numStackAddrRetrieved = WalkStack(arrStackAddresses, 10, 0);
    ch *szStack = StackDescribe(arrStackAddresses, numStackAddrRetrieved);
    if (szStack && *szStack) {
      pFileName = szStack;  // Use the stack description for the allocation
    }
  }
#endif  // #if defined( USE_STACK_WALK_DETAILED )

  ch *pszFilenameCopy;
  Filenames_t::const_iterator iter = m_Filenames.find(pFileName);
  if (iter == m_Filenames.end()) {
    usize nLen = strlen(pFileName) + 1;
    pszFilenameCopy = (ch *)DebugAlloc(nLen);
    memcpy(pszFilenameCopy, pFileName, nLen);
    m_Filenames.insert(pszFilenameCopy);
  } else {
    pszFilenameCopy = (ch *)(*iter);
  }

  return pszFilenameCopy;
}

// Finds the file in our map

CDbgMemAlloc::MemInfo_t &CDbgMemAlloc::FindOrCreateEntry(const ch *pFileName,
                                                         i32 line) {
  // Oh how I love crazy STL. retval.first == the StatMapIter_t in the std::pair
  // retval.first->second == the MemInfo_t that's part of the StatMapIter_t
  std::pair<StatMapIter_t, bool> retval;
  retval = m_StatMap.insert(
      StatMapEntry_t(MemInfoKey_t(pFileName, line), MemInfo_t()));
  return retval.first->second;
}

// Updates stats

void CDbgMemAlloc::RegisterAllocation(const ch *pFileName, i32 nLine,
                                      usize nLogicalSize, usize nActualSize,
                                      u32 nTime) {
  HEAP_LOCK();
  RegisterAllocation(m_GlobalInfo, nLogicalSize, nActualSize, nTime);
  RegisterAllocation(FindOrCreateEntry(pFileName, nLine), nLogicalSize,
                     nActualSize, nTime);
}

void CDbgMemAlloc::RegisterDeallocation(const ch *pFileName, i32 nLine,
                                        usize nLogicalSize, usize nActualSize,
                                        u32 nTime) {
  HEAP_LOCK();
  RegisterDeallocation(m_GlobalInfo, nLogicalSize, nActualSize, nTime);
  RegisterDeallocation(FindOrCreateEntry(pFileName, nLine), nLogicalSize,
                       nActualSize, nTime);
}

void CDbgMemAlloc::RegisterAllocation(MemInfo_t &info, usize nLogicalSize,
                                      usize nActualSize, u32 nTime) {
  ++info.m_nCurrentCount;
  ++info.m_nTotalCount;
  if (info.m_nCurrentCount > info.m_nPeakCount) {
    info.m_nPeakCount = info.m_nCurrentCount;
  }

  info.m_nCurrentSize += nLogicalSize;
  info.m_nTotalSize += nLogicalSize;
  if (info.m_nCurrentSize > info.m_nPeakSize) {
    info.m_nPeakSize = info.m_nCurrentSize;
  }

  for (i32 i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i) {
    if (nLogicalSize <= s_pCountSizes[i]) {
      ++info.m_pCount[i];
      break;
    }
  }

  Assert(info.m_nPeakCount >= info.m_nCurrentCount);
  Assert(info.m_nPeakSize >= info.m_nCurrentSize);

  info.m_nOverheadSize += (nActualSize - nLogicalSize);
  if (info.m_nOverheadSize > info.m_nPeakOverheadSize) {
    info.m_nPeakOverheadSize = info.m_nOverheadSize;
  }

  info.m_nTime += nTime;
}

void CDbgMemAlloc::RegisterDeallocation(MemInfo_t &info, usize nLogicalSize,
                                        usize nActualSize, u32 nTime) {
  --info.m_nCurrentCount;
  info.m_nCurrentSize -= nLogicalSize;

  for (i32 i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i) {
    if (nLogicalSize <= s_pCountSizes[i]) {
      --info.m_pCount[i];
      break;
    }
  }

  Assert(info.m_nPeakCount >= info.m_nCurrentCount);
  Assert(info.m_nPeakSize >= info.m_nCurrentSize);

  info.m_nOverheadSize -= (nActualSize - nLogicalSize);
  info.m_nTime += nTime;
}

// Gets the allocation file name

const ch *CDbgMemAlloc::GetAllocatonFileName(void *pMem) {
  if (!pMem) return "";

  CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader(pMem);
  if (pHeader->m_pFileName)
    return pHeader->m_pFileName;
  else
    return g_pszUnknown;
}

// Gets the allocation file name

i32 CDbgMemAlloc::GetAllocatonLineNumber(void *pMem) {
  if (!pMem) return 0;

  CrtDbgMemHeader_t *pHeader = GetCrtDbgMemHeader(pMem);
  return pHeader->m_nLineNumber;
}

// Debug versions of the main allocation methods

void *CDbgMemAlloc::Alloc(usize nSize, const ch *pFileName, i32 nLine) {
  HEAP_LOCK();

  if (!m_bInitialized) return InternalMalloc(nSize, pFileName, nLine);

  if (pFileName != g_pszUnknown) pFileName = FindOrCreateFilename(pFileName);

  GetActualDbgInfo(pFileName, nLine);

  m_Timer.Start();
  void *pMem = InternalMalloc(nSize, pFileName, nLine);
  m_Timer.End();

  ApplyMemoryInitializations(pMem, nSize);

  RegisterAllocation(GetAllocatonFileName(pMem), GetAllocatonLineNumber(pMem),
                     InternalLogicalSize(pMem), InternalMSize(pMem),
                     m_Timer.GetDuration().GetMicroseconds());

  if (!pMem) {
    SetCRTAllocFailed(nSize);
  }
  return pMem;
}

void *CDbgMemAlloc::Realloc(void *pMem, usize nSize, const ch *pFileName,
                            i32 nLine) {
  HEAP_LOCK();

  pFileName = FindOrCreateFilename(pFileName);

  if (!m_bInitialized) return InternalRealloc(pMem, nSize, pFileName, nLine);

  if (pMem != 0) {
    RegisterDeallocation(GetAllocatonFileName(pMem),
                         GetAllocatonLineNumber(pMem),
                         InternalLogicalSize(pMem), InternalMSize(pMem), 0);
  }

  GetActualDbgInfo(pFileName, nLine);

  m_Timer.Start();
  pMem = InternalRealloc(pMem, nSize, pFileName, nLine);
  m_Timer.End();

  RegisterAllocation(GetAllocatonFileName(pMem), GetAllocatonLineNumber(pMem),
                     InternalLogicalSize(pMem), InternalMSize(pMem),
                     m_Timer.GetDuration().GetMicroseconds());
  if (!pMem) {
    SetCRTAllocFailed(nSize);
  }
  return pMem;
}

void CDbgMemAlloc::Free(void *pMem, const ch * /*pFileName*/, i32 nLine) {
  if (!pMem) return;

  HEAP_LOCK();

  if (!m_bInitialized) {
    InternalFree(pMem);
    return;
  }

  usize nOldLogicalSize = InternalLogicalSize(pMem);
  usize nOldSize = InternalMSize(pMem);
  const ch *pOldFileName = GetAllocatonFileName(pMem);
  i32 oldLine = GetAllocatonLineNumber(pMem);

  m_Timer.Start();
  InternalFree(pMem);
  m_Timer.End();

  RegisterDeallocation(pOldFileName, oldLine, nOldLogicalSize, nOldSize,
                       m_Timer.GetDuration().GetMicroseconds());
}

void *CDbgMemAlloc::Expand_NoLongerSupported(void *pMem, usize nSize,
                                             const ch *pFileName, i32 nLine) {
  return nullptr;
}

// Returns size of a particular allocation

usize CDbgMemAlloc::GetSize(void *pMem) {
  HEAP_LOCK();
  return pMem ? InternalMSize(pMem) : CalcHeapUsed();
}

// FIXME: Remove when we make our own heap! Crt stuff we're currently using

long CDbgMemAlloc::CrtSetBreakAlloc(long lNewBreakAlloc) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtSetBreakAlloc(lNewBreakAlloc);
#endif
}

i32 CDbgMemAlloc::CrtSetReportMode(i32 nReportType, i32 nReportMode) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtSetReportMode(nReportType, nReportMode);
#endif
}

i32 CDbgMemAlloc::CrtIsValidHeapPointer(const void *pMem) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtIsValidHeapPointer(pMem);
#endif
}

i32 CDbgMemAlloc::CrtIsValidPointer(const void *pMem, u32 size, i32 access) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtIsValidPointer(pMem, size, access);
#endif
}

#define DBGMEM_CHECKMEMORY 1

i32 CDbgMemAlloc::CrtCheckMemory() {
#ifndef DBGMEM_CHECKMEMORY
  return 1;
#else
  if (!_CrtCheckMemory()) {
    Msg("Memory check failed!\n");
    return 0;
  }
  return 1;
#endif
}

i32 CDbgMemAlloc::CrtSetDbgFlag(i32 nNewFlag) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtSetDbgFlag(nNewFlag);
#endif
}

void CDbgMemAlloc::CrtMemCheckpoint(_CrtMemState *pState) {
#ifdef OS_POSIX
  return 0;
#else
  _CrtMemCheckpoint(pState);
#endif
}

// FIXME: Remove when we have our own allocator
void *CDbgMemAlloc::CrtSetReportFile(i32 nRptType, void *hFile) {
#ifdef OS_POSIX
  return 0;
#else
  return (void *)_CrtSetReportFile(nRptType, (_HFILE)hFile);
#endif
}

void *CDbgMemAlloc::CrtSetReportHook(void *pfnNewHook) {
#ifdef OS_POSIX
  return 0;
#else
  return (void *)_CrtSetReportHook((_CRT_REPORT_HOOK)pfnNewHook);
#endif
}

i32 CDbgMemAlloc::CrtDbgReport(i32 nRptType, const ch *szFile, i32 nLine,
                               const ch *szModule, const ch *pMsg) {
#ifdef OS_POSIX
  return 0;
#else
  return _CrtDbgReport(nRptType, szFile, nLine, szModule, pMsg);
#endif
}

i32 CDbgMemAlloc::heapchk() {
#ifdef OS_POSIX
  return 0;
#else
  return _HEAPOK;
#endif
}

void CDbgMemAlloc::DumpBlockStats(void *p) {
  DbgMemHeader_t *pBlock = (DbgMemHeader_t *)p - 1;
  if (!CrtIsValidHeapPointer(pBlock)) {
    Msg("0x%x is not valid heap pointer\n", p);
    return;
  }

  const ch *pFileName = GetAllocatonFileName(p);
  i32 line = GetAllocatonLineNumber(p);

  Msg("0x%x allocated by %s line %d, %d bytes\n", p, pFileName, line,
      GetSize(p));
}

// Stat output

void CDbgMemAlloc::DumpMemInfo(const ch *pAllocationName, i32 line,
                               const MemInfo_t &info) {
  m_OutputFunc("%s, line %i\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%llu\t%zu\t%zu\t%zu",
               pAllocationName, line, info.m_nCurrentSize / 1024.0,
               info.m_nPeakSize / 1024.0, info.m_nTotalSize / 1024.0,
               info.m_nOverheadSize / 1024.0, info.m_nPeakOverheadSize / 1024.0,
               (info.m_nTime / 1000), info.m_nCurrentCount, info.m_nPeakCount,
               info.m_nTotalCount);

  for (i32 i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i) {
    m_OutputFunc("\t%zu", info.m_pCount[i]);
  }

  m_OutputFunc("\n");
}

// Stat output

void CDbgMemAlloc::DumpFileStats() {
  StatMapIter_t iter = m_StatMap.begin();
  while (iter != m_StatMap.end()) {
    DumpMemInfo(iter->first.m_pFileName, iter->first.m_nLine, iter->second);
    iter++;
  }
}

void CDbgMemAlloc::DumpStatsFileBase(ch const *pchFileBase) {
  HEAP_LOCK();

  ch szFileName[MAX_PATH];
  static i32 s_FileCount = 0;
  if (m_OutputFunc == DefaultHeapReportFunc) {
    const ch *pPath = "";

#if defined(_MEMTEST)
    ch szXboxName[32];
    strcpy(szXboxName, "xbox");
    DWORD numChars = sizeof(szXboxName);
    DmGetXboxName(szXboxName, &numChars);
    ch *pXboxName = strstr(szXboxName, "_360");
    if (pXboxName) {
      *pXboxName = '\0';
    }

    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);
    _snprintf_s(szFileName, sizeof(szFileName),
                "%s%s_%2.2d%2.2d_%2.2d%2.2d%2.2d_%d.txt", pPath,
                s_szStatsMapName, systemTime.wMonth, systemTime.wDay,
                systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
                s_FileCount);
#else
    _snprintf_s(szFileName, sizeof(szFileName), "%s%s%d.txt", pPath,
                pchFileBase, s_FileCount);
#endif

    ++s_FileCount;

    s_DbgFile = fopen(szFileName, "wt");
    if (!s_DbgFile) return;
  }

  m_OutputFunc(
      "Allocation type\tCurrent Size(k)\tPeak Size(k)\tTotal "
      "Allocations(k)\tOverhead Size(k)\tPeak Overhead "
      "Size(k)\tTime(ms)\tCurrent Count\tPeak Count\tTotal Count");

  for (i32 i = 0; i < NUM_BYTE_COUNT_BUCKETS; ++i) {
    m_OutputFunc("\t%s", s_pCountHeader[i]);
  }

  m_OutputFunc("\n");

  DumpMemInfo("Totals", 0, m_GlobalInfo);

  DumpFileStats();

  if (m_OutputFunc == DefaultHeapReportFunc) {
    fclose(s_DbgFile);
  }
}

// Stat output

void CDbgMemAlloc::DumpStats() { DumpStatsFileBase("memstats"); }

void CDbgMemAlloc::SetCRTAllocFailed(usize nSize) {
  m_sMemoryAllocFailed = nSize;
}

usize CDbgMemAlloc::MemoryAllocFailed() { return m_sMemoryAllocFailed; }

#endif  // !NDEBUG

#endif  // !STEAM && !NO_MALLOC_OVERRIDE
