// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/include/threadtools.h"

#ifdef OS_WIN
#include <process.h>
#elif OS_POSIX
typedef i32 (*PTHREAD_START_ROUTINE)(void *lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/time.h>
#include <exception>
#define GetLastError() errno
typedef void *LPVOID;
#endif

#include <memory.h>
#include <memory>
#include "tier0/include/vcrmode.h"

// Must be last header...
#include "tier0/include/memdbgon.h"

#define THREADS_DEBUG 1

// Need to ensure initialized before other clients call in for main thread ID
#ifdef OS_WIN
MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4073)  // warning C4074: initializers put in lib
                            // reserved initialization area
#pragma init_seg(lib)
MSVC_END_WARNING_OVERRIDE_SCOPE()
#endif

#ifdef OS_WIN
COMPILE_TIME_ASSERT(TT_SIZEOF_CRITICALSECTION == sizeof(CRITICAL_SECTION));
COMPILE_TIME_ASSERT(TT_INFINITE == INFINITE);
#endif


// Simple thread functions.
// Because _beginthreadex uses stdcall, we need to convert to cdecl

struct ThreadProcInfo_t {
  ThreadProcInfo_t(ThreadFunc_t pfnThread, void *pParam)
      : pfnThread(pfnThread), pParam(pParam) {}

  ThreadFunc_t pfnThread;
  void *pParam;
};

//---------------------------------------------------------

static u32 __stdcall ThreadProcConvert(void *pParam) {
  ThreadProcInfo_t info = *((ThreadProcInfo_t *)pParam);
  delete ((ThreadProcInfo_t *)pParam);
  return (*info.pfnThread)(info.pParam);
}

//---------------------------------------------------------

ThreadHandle_t CreateSimpleThread(ThreadFunc_t pfnThread, void *pParam,
                                  ThreadId_t *pID, u32 stackSize) {
#ifdef OS_WIN
  ThreadId_t idIgnored;
  if (!pID) pID = &idIgnored;
  return (ThreadHandle_t)VCRHook_CreateThread(
      nullptr, stackSize, (LPTHREAD_START_ROUTINE)ThreadProcConvert,
      new ThreadProcInfo_t(pfnThread, pParam), 0, pID);
#elif OS_POSIX
  pthread_t tid;
  pthread_create(&tid, nullptr, ThreadProcConvert,
                 new ThreadProcInfo_t(pfnThread, pParam));
  if (pID) *pID = (ThreadId_t)tid;
  return tid;
#endif
}

ThreadHandle_t CreateSimpleThread(ThreadFunc_t pfnThread, void *pParam,
                                  u32 stackSize) {
  return CreateSimpleThread(pfnThread, pParam, nullptr, stackSize);
}

bool ReleaseThreadHandle(ThreadHandle_t hThread) {
#ifdef OS_WIN
  return (CloseHandle(hThread) != 0);
#else
  return true;
#endif
}


//
// Wrappers for other simple threading operations
//


void ThreadSleep(u32 duration) {
#ifdef OS_WIN
  Sleep(duration);
#elif OS_POSIX
  usleep(duration * 1000);
#endif
}



#ifndef ThreadGetCurrentId
u32 ThreadGetCurrentId() {
#ifdef OS_WIN
  return GetCurrentThreadId();
#elif OS_POSIX
  return pthread_self();
#endif
}
#endif


ThreadHandle_t ThreadGetCurrentHandle() {
#ifdef OS_WIN
  return (ThreadHandle_t)GetCurrentThread();
#elif OS_POSIX
  return (ThreadHandle_t)pthread_self();
#endif
}



i32 ThreadGetPriority(ThreadHandle_t hThread) {
#ifdef OS_WIN
  if (!hThread) {
    return ::GetThreadPriority(GetCurrentThread());
  }
  return ::GetThreadPriority((HANDLE)hThread);
#else
  return 0;
#endif
}



bool ThreadSetPriority(ThreadHandle_t hThread, i32 priority) {
  if (!hThread) {
    hThread = ThreadGetCurrentHandle();
  }

#ifdef OS_WIN
  return SetThreadPriority(hThread, priority) != FALSE;
#elif OS_POSIX
  struct sched_param thread_param;
  thread_param.sched_priority = priority;
  pthread_setschedparam(hThread, SCHED_RR, &thread_param);
  return true;
#endif
}



void ThreadSetAffinity(ThreadHandle_t hThread, uintptr_t nAffinityMask) {
  if (!hThread) {
    hThread = ThreadGetCurrentHandle();
  }

#ifdef OS_WIN
  SetThreadAffinityMask(hThread, nAffinityMask);
#elif OS_POSIX
// 	cpu_set_t cpuSet;
// 	CPU_ZERO( cpuSet );
// 	for( i32 i = 0 ; i < 32; i++ )
// 	  if ( nAffinityMask & ( 1 << i ) )
// 	    CPU_SET( cpuSet, i );
// 	sched_setaffinity( hThread, sizeof( cpuSet ), &cpuSet );
#endif
}



u32 InitMainThread() {
  ThreadSetDebugName("MainThrd");
#ifdef OS_WIN
  return ThreadGetCurrentId();
#elif OS_POSIX
  return pthread_self();
#endif
}

u32 g_ThreadMainThreadID = InitMainThread();

bool ThreadInMainThread() {
  return (ThreadGetCurrentId() == g_ThreadMainThreadID);
}


void DeclareCurrentThreadIsMainThread() {
  g_ThreadMainThreadID = ThreadGetCurrentId();
}

bool ThreadJoin(ThreadHandle_t hThread, u32 timeout) {
  if (!hThread) {
    return false;
  }

#ifdef OS_WIN
  DWORD wait_result = VCRHook_WaitForSingleObject((HANDLE)hThread, timeout);
  if (wait_result == WAIT_TIMEOUT) return false;
  if (wait_result != WAIT_OBJECT_0 &&
      (wait_result != WAIT_FAILED && GetLastError() != 0)) {
    Assert(0);
    return false;
  }
#elif OS_POSIX
  if (pthread_join((pthread_t)hThread, nullptr) != 0) return false;
    //	m_threadId = 0;
#endif
  return true;
}


// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
void ThreadSetDebugName(ThreadId_t id, const ch *pszName) {
#ifdef OS_WIN
#define MS_VC_EXCEPTION 0x406d1388
  if (Plat_IsInDebugSession()) {
#pragma pack(push, 8)
    typedef struct tagTHREADNAME_INFO {
      DWORD dwType;      // must be 0x1000
      LPCSTR szName;     // pointer to name (in same addr space)
      DWORD dwThreadID;  // thread ID (-1 caller thread)
      DWORD dwFlags;     // reserved for future use, most be zero
    } THREADNAME_INFO;
#pragma pack(pop)

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = pszName;
    info.dwThreadID = id;
    info.dwFlags = 0;

    __try {
      RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                     reinterpret_cast<ULONG_PTR *>(&info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
  }
#endif
}



COMPILE_TIME_ASSERT(TW_FAILED == WAIT_FAILED);
COMPILE_TIME_ASSERT(TW_TIMEOUT == WAIT_TIMEOUT);
COMPILE_TIME_ASSERT(WAIT_OBJECT_0 == 0);

#ifdef OS_WIN
i32 ThreadWaitForObjects(i32 nEvents, const HANDLE *pHandles, bool bWaitAll,
                         u32 timeout) {
  return VCRHook_WaitForMultipleObjects(nEvents, pHandles, bWaitAll, timeout);
}
#endif


// Used to thread LoadLibrary on the 360

static ThreadedLoadLibraryFunc_t s_ThreadedLoadLibraryFunc = 0;
SOURCE_TIER0_API void SetThreadedLoadLibraryFunc(ThreadedLoadLibraryFunc_t func) {
  s_ThreadedLoadLibraryFunc = func;
}

SOURCE_TIER0_API ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc() {
  return s_ThreadedLoadLibraryFunc;
}


//


CThreadSyncObject::CThreadSyncObject()
#ifdef OS_WIN
    : m_hSyncObject(nullptr)
#elif OS_POSIX
    : m_bInitalized(false)
#endif
{
}

//---------------------------------------------------------

CThreadSyncObject::~CThreadSyncObject() {
#ifdef OS_WIN
  if (m_hSyncObject) {
    if (!CloseHandle(m_hSyncObject)) {
      Assert(0);
    }
  }
#elif OS_POSIX
  if (m_bInitalized) {
    pthread_cond_destroy(&m_Condition);
    pthread_mutex_destroy(&m_Mutex);
    m_bInitalized = false;
  }
#endif
}

//---------------------------------------------------------

bool CThreadSyncObject::operator!() const {
#ifdef OS_WIN
  return !m_hSyncObject;
#elif OS_POSIX
  return !m_bInitalized;
#endif
}

//---------------------------------------------------------

void CThreadSyncObject::AssertUseable() {
#ifdef THREADS_DEBUG
#ifdef OS_WIN
  AssertMsg(m_hSyncObject, "Thread synchronization object is unuseable");
#elif OS_POSIX
  AssertMsg(m_bInitalized, "Thread synchronization object is unuseable");
#endif
#endif
}

//---------------------------------------------------------

bool CThreadSyncObject::Wait(u32 dwTimeout) {
#ifdef THREADS_DEBUG
  AssertUseable();
#endif
#ifdef OS_WIN
  return (VCRHook_WaitForSingleObject(m_hSyncObject, dwTimeout) ==
          WAIT_OBJECT_0);
#elif OS_POSIX
  pthread_mutex_lock(&m_Mutex);
  bool bRet = false;
  if (m_cSet > 0) {
    bRet = true;
  } else {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    volatile struct timespec tm;

    volatile u64 nSec = (u64)tv.tv_usec * 1000 + (u64)dwTimeout * 1000000;
    tm.tv_sec = tv.tv_sec + nSec / 1000000000;
    tm.tv_nsec = nSec % 1000000000;

    volatile i32 ret = 0;

    do {
      ret = pthread_cond_timedwait(&m_Condition, &m_Mutex, &tm);
    } while (ret == EINTR);

    bRet = (ret == 0);
  }
  if (!m_bManualReset) m_cSet = 0;
  pthread_mutex_unlock(&m_Mutex);
  return bRet;
#endif
}


//


CThreadEvent::CThreadEvent(bool bManualReset) {
#ifdef OS_WIN
  m_hSyncObject = CreateEvent(nullptr, bManualReset, FALSE, nullptr);
  AssertMsg1(m_hSyncObject, "Failed to create event (error 0x%x)",
             GetLastError());
#elif OS_POSIX
  pthread_mutexattr_t Attr;
  pthread_mutexattr_init(&Attr);
  pthread_mutex_init(&m_Mutex, &Attr);
  pthread_mutexattr_destroy(&Attr);
  pthread_cond_init(&m_Condition, nullptr);
  m_bInitalized = true;
  m_cSet = 0;
  m_bManualReset = bManualReset;
#else
#error "Implement me"
#endif
}

//


//---------------------------------------------------------

bool CThreadEvent::Set() {
  AssertUseable();
#ifdef OS_WIN
  return (SetEvent(m_hSyncObject) != 0);
#elif OS_POSIX
  pthread_mutex_lock(&m_Mutex);
  m_cSet = 1;
  i32 ret = pthread_cond_broadcast(&m_Condition);
  pthread_mutex_unlock(&m_Mutex);
  return ret == 0;
#endif
}

//---------------------------------------------------------

bool CThreadEvent::Reset() {
#ifdef THREADS_DEBUG
  AssertUseable();
#endif
#ifdef OS_WIN
  return (ResetEvent(m_hSyncObject) != 0);
#elif OS_POSIX
  pthread_mutex_lock(&m_Mutex);
  m_cSet = 0;
  pthread_mutex_unlock(&m_Mutex);
  return true;
#endif
}

//---------------------------------------------------------

bool CThreadEvent::Check() {
#ifdef THREADS_DEBUG
  AssertUseable();
#endif
  return Wait(0);
}

bool CThreadEvent::Wait(u32 dwTimeout) {
  return CThreadSyncObject::Wait(dwTimeout);
}

#ifdef OS_WIN

//
// CThreadSemaphore
//
// To get linux implementation, try
// http://www-128.ibm.com/developerworks/eserver/library/es-win32linux-sem.html
//


CThreadSemaphore::CThreadSemaphore(long initialValue, long maxValue) {
  if (maxValue) {
    AssertMsg(maxValue > 0, "Invalid max value for semaphore");
    AssertMsg(initialValue >= 0 && initialValue <= maxValue,
              "Invalid initial value for semaphore");

    m_hSyncObject = CreateSemaphore(nullptr, initialValue, maxValue, nullptr);

    AssertMsg1(m_hSyncObject, "Failed to create semaphore (error 0x%x)",
               GetLastError());
  } else {
    m_hSyncObject = nullptr;
  }
}

//---------------------------------------------------------

bool CThreadSemaphore::Release(long releaseCount, long *pPreviousCount) {
#ifdef THRDTOOL_DEBUG
  AssertUseable();
#endif
  return (ReleaseSemaphore(m_hSyncObject, releaseCount, pPreviousCount) != 0);
}


//


_Acquires_lock_(this->m_hSyncObject) CThreadFullMutex::CThreadFullMutex(
    bool bEstablishInitialOwnership, const ch *pszName) {
  m_hSyncObject = CreateMutex(nullptr, bEstablishInitialOwnership, pszName);

  AssertMsg1(m_hSyncObject, "Failed to create mutex (error 0x%x)",
             GetLastError());
}

//---------------------------------------------------------

_Releases_lock_(this->m_hSyncObject) bool CThreadFullMutex::Release() {
#ifdef THRDTOOL_DEBUG
  AssertUseable();
#endif
  return (ReleaseMutex(m_hSyncObject) != 0);
}

#endif


//


CThreadLocalBase::CThreadLocalBase() {
#ifdef OS_WIN
  m_index = TlsAlloc();
  AssertMsg(m_index != 0xFFFFFFFF, "Bad thread local");
  if (m_index == 0xFFFFFFFF) Error("Out of thread local storage!\n");
#elif OS_POSIX
  if (pthread_key_create(&m_index, nullptr) != 0)
    Error("Out of thread local storage!\n");
#endif
}

//---------------------------------------------------------

CThreadLocalBase::~CThreadLocalBase() {
#ifdef OS_WIN
  if (m_index != 0xFFFFFFFF) TlsFree(m_index);
  m_index = 0xFFFFFFFF;
#elif OS_POSIX
  pthread_key_delete(m_index);
#endif
}

//---------------------------------------------------------

void *CThreadLocalBase::Get() const {
#ifdef OS_WIN
  if (m_index != 0xFFFFFFFF) return TlsGetValue(m_index);
  AssertMsg(0, "Bad thread local");
  return nullptr;
#elif OS_POSIX
  void *value = pthread_getspecific(m_index);
  return value;
#endif
}

//---------------------------------------------------------

void CThreadLocalBase::Set(void *value) {
#ifdef OS_WIN
  if (m_index != 0xFFFFFFFF)
    TlsSetValue(m_index, value);
  else
    AssertMsg(0, "Bad thread local");
#elif OS_POSIX
  if (pthread_setspecific(m_index, value) != 0)
    AssertMsg(0, "Bad thread local");
#endif
}





#ifdef OS_WIN
#define TO_INTERLOCK_PARAM(p) (p)
#define TO_INTERLOCK_PTR_PARAM(p) (p)

#ifndef USE_INTRINSIC_INTERLOCKED
long ThreadInterlockedIncrement(long volatile *pDest) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedIncrement(TO_INTERLOCK_PARAM(pDest));
}

long ThreadInterlockedDecrement(long volatile *pDest) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedDecrement(TO_INTERLOCK_PARAM(pDest));
}

long ThreadInterlockedExchange(long volatile *pDest, long value) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedExchange(TO_INTERLOCK_PARAM(pDest), value);
}

long ThreadInterlockedExchangeAdd(long volatile *pDest, long value) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedExchangeAdd(TO_INTERLOCK_PARAM(pDest), value);
}

long ThreadInterlockedCompareExchange(long volatile *pDest, long value,
                                      long comperand) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedCompareExchange(TO_INTERLOCK_PARAM(pDest), value,
                                    comperand);
}

bool ThreadInterlockedAssignIf(long volatile *pDest, long value,
                               long comperand) {
  Assert((usize)pDest % 4 == 0);

#if !defined(ARCH_CPU_X86_64)
  __asm
  {
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
  }
#else
  return (InterlockedCompareExchange(TO_INTERLOCK_PARAM(pDest), value,
                                     comperand) == comperand);
#endif
}

#endif

#if !defined(USE_INTRINSIC_INTERLOCKED) || defined(ARCH_CPU_X86_64)
void *ThreadInterlockedExchangePointer(void *volatile *pDest, void *value) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedExchangePointer(TO_INTERLOCK_PARAM(pDest), value);
}

void *ThreadInterlockedCompareExchangePointer(void *volatile *pDest,
                                              void *value, void *comperand) {
  Assert((usize)pDest % 4 == 0);
  return InterlockedCompareExchangePointer(TO_INTERLOCK_PTR_PARAM(pDest), value,
                                           comperand);
}

bool ThreadInterlockedAssignPointerIf(void *volatile *pDest, void *value,
                                      void *comperand) {
  Assert((usize)pDest % 4 == 0);
#if !defined(ARCH_CPU_X86_64)
  __asm
  {
		mov	eax,comperand
		mov	ecx,pDest
		mov edx,value
		lock cmpxchg [ecx],edx 
		mov eax,0
		setz al
  }
#else
  return (InterlockedCompareExchangePointer(TO_INTERLOCK_PTR_PARAM(pDest),
                                            value, comperand) == comperand);
#endif
}
#endif

i64 ThreadInterlockedCompareExchange64(i64 volatile *pDest, i64 value,
                                       i64 comperand) {
  Assert((usize)pDest % 8 == 0);

#if defined(ARCH_CPU_X86_64)
  return InterlockedCompareExchange64(pDest, value, comperand);
#else
  __asm
  {
		lea esi,comperand;
		lea edi,value;

		mov eax,[esi];
		mov edx,4[esi];
		mov ebx,[edi];
		mov ecx,4[edi];
		mov esi,pDest;
		lock CMPXCHG8B [esi];
  }
#endif
}

bool ThreadInterlockedAssignIf64(volatile i64 *pDest, i64 value,
                                 i64 comperand) {
  Assert((usize)pDest % 8 == 0);

#if defined(ARCH_CPU_X86_64)
  return ThreadInterlockedCompareExchange64(pDest, value, comperand) ==
         comperand;
#else
  __asm
  {
    lea esi, comperand;
    lea edi, value;

    mov eax, [esi];
    mov edx, 4[esi];
    mov ebx, [edi];
    mov ecx, 4[edi];
    mov esi, pDest;
    lock CMPXCHG8B[esi];
    mov eax, 0;
    setz al;
  }
#endif
}

i64 ThreadInterlockedIncrement64(i64 volatile *pDest) {
  Assert((usize)pDest % 8 == 0);

  i64 Old;

  do {
    Old = *pDest;
  } while (ThreadInterlockedCompareExchange64(pDest, Old + 1, Old) != Old);

  return Old + 1;
}

i64 ThreadInterlockedDecrement64(i64 volatile *pDest) {
  Assert((usize)pDest % 8 == 0);
  i64 Old;

  do {
    Old = *pDest;
  } while (ThreadInterlockedCompareExchange64(pDest, Old - 1, Old) != Old);

  return Old - 1;
}

i64 ThreadInterlockedExchange64(i64 volatile *pDest, i64 value) {
  Assert((usize)pDest % 8 == 0);
  i64 Old;

  do {
    Old = *pDest;
  } while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

  return Old;
}

i64 ThreadInterlockedExchangeAdd64(i64 volatile *pDest, i64 value) {
  Assert((usize)pDest % 8 == 0);
  i64 Old;

  do {
    Old = *pDest;
  } while (ThreadInterlockedCompareExchange64(pDest, Old + value, Old) != Old);

  return Old;
}

#else
// This will perform horribly, what's the Linux alternative?
// atomic_set(), atomic_add() et al should work for i386 arch
// TODO: implement these if needed
CThreadMutex g_InterlockedMutex;

long ThreadInterlockedIncrement(long volatile *pDest) {
  AUTO_LOCK(g_InterlockedMutex);
  return ++(*pDest);
}

long ThreadInterlockedDecrement(long volatile *pDest) {
  AUTO_LOCK(g_InterlockedMutex);
  return --(*pDest);
}

long ThreadInterlockedExchange(long volatile *pDest, long value) {
  AUTO_LOCK(g_InterlockedMutex);
  long retVal = *pDest;
  *pDest = value;
  return retVal;
}

void *ThreadInterlockedExchangePointer(void *volatile *pDest, void *value) {
  AUTO_LOCK(g_InterlockedMutex);
  void *retVal = *pDest;
  *pDest = value;
  return retVal;
}

long ThreadInterlockedExchangeAdd(long volatile *pDest, long value) {
  AUTO_LOCK(g_InterlockedMutex);
  long retVal = *pDest;
  *pDest += value;
  return retVal;
}

long ThreadInterlockedCompareExchange(long volatile *pDest, long value,
                                      long comperand) {
  AUTO_LOCK(g_InterlockedMutex);
  long retVal = *pDest;
  if (*pDest == comperand) *pDest = value;
  return retVal;
}

void *ThreadInterlockedCompareExchangePointer(void *volatile *pDest,
                                              void *value, void *comperand) {
  AUTO_LOCK(g_InterlockedMutex);
  void *retVal = *pDest;
  if (*pDest == comperand) *pDest = value;
  return retVal;
}

i64 ThreadInterlockedCompareExchange64(i64 volatile *pDest, i64 value,
                                       i64 comperand) {
  Assert((usize)pDest % 8 == 0);
  AUTO_LOCK(g_InterlockedMutex);
  i64 retVal = *pDest;
  if (*pDest == comperand) *pDest = value;
  return retVal;
}

i64 ThreadInterlockedExchange64(i64 volatile *pDest, i64 value) {
  Assert((usize)pDest % 8 == 0);
  i64 Old;

  do {
    Old = *pDest;
  } while (ThreadInterlockedCompareExchange64(pDest, value, Old) != Old);

  return Old;
}

bool ThreadInterlockedAssignIf64(volatile i64 *pDest, i64 value,
                                 i64 comperand) {
  Assert((usize)pDest % 8 == 0);
  return (ThreadInterlockedCompareExchange64(pDest, value, comperand) ==
          comperand);
}

bool ThreadInterlockedAssignIf(long volatile *pDest, long value,
                               long comperand) {
  Assert((usize)pDest % 4 == 0);
  return (ThreadInterlockedCompareExchange(pDest, value, comperand) ==
          comperand);
}

#endif



#if defined(OS_WIN) && defined(THREAD_PROFILER)
void ThreadNotifySyncNoop(void *p) {}

#define MAP_THREAD_PROFILER_CALL(from, to)                                     \
  void from(void *p) {                                                         \
    static CDynamicFunction<void (*)(void *)> dynFunc("libittnotify.dll", #to, \
                                                      ThreadNotifySyncNoop);   \
    (*dynFunc)(p);                                                             \
  }

MAP_THREAD_PROFILER_CALL(ThreadNotifySyncPrepare, __itt_notify_sync_prepare);
MAP_THREAD_PROFILER_CALL(ThreadNotifySyncCancel, __itt_notify_sync_cancel);
MAP_THREAD_PROFILER_CALL(ThreadNotifySyncAcquired, __itt_notify_sync_acquired);
MAP_THREAD_PROFILER_CALL(ThreadNotifySyncReleasing,
                         __itt_notify_sync_releasing);

#endif


//
// CThreadMutex
//


#ifndef OS_POSIX
CThreadMutex::CThreadMutex() {
#ifdef THREAD_MUTEX_TRACING_ENABLED
  memset(&m_CriticalSection, 0, sizeof(m_CriticalSection));
#endif
  InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION *)&m_CriticalSection,
                                        4000);
#ifdef THREAD_MUTEX_TRACING_SUPPORTED
  // These need to be initialized unconditionally in case mixing release & debug
  // object modules Lock and unlock may be emitted as COMDATs, in which case may
  // get spurious output
  m_currentOwnerID = m_lockCount = 0;
  m_bTrace = false;
#endif
}

CThreadMutex::~CThreadMutex() {
  DeleteCriticalSection((CRITICAL_SECTION *)&m_CriticalSection);
}
#endif  // !linux

bool CThreadMutex::TryLock() {
#if defined(OS_WIN)
#ifdef THREAD_MUTEX_TRACING_ENABLED
  u32 thisThreadID = ThreadGetCurrentId();
  if (m_bTrace && m_currentOwnerID && (m_currentOwnerID != thisThreadID))
    Msg("Thread %u about to try-wait for lock %x owned by %u\n",
        ThreadGetCurrentId(), (CRITICAL_SECTION *)&m_CriticalSection,
        m_currentOwnerID);
#endif
  if (TryEnterCriticalSection((CRITICAL_SECTION *)&m_CriticalSection) !=
      FALSE) {
#ifdef THREAD_MUTEX_TRACING_ENABLED
    if (m_lockCount == 0) {
      // we now own it for the first time. Set owner information
      m_currentOwnerID = thisThreadID;
      if (m_bTrace)
        Msg("Thread %u now owns lock 0x%x\n", m_currentOwnerID,
            (CRITICAL_SECTION *)&m_CriticalSection);
    }
    m_lockCount++;
#endif
    return true;
  }
  return false;
#elif defined(OS_POSIX)
  return pthread_mutex_trylock(&m_Mutex) == 0;
#else
#error "Implement me!"
  return true;
#endif
}


//
// CThreadFastMutex
//


#ifndef OS_POSIX
void CThreadFastMutex::Lock(const u32 threadId, u32 nSpinSleepTime) volatile {
  i32 i;
  if (nSpinSleepTime != TT_INFINITE) {
    for (i = 1000; i != 0; --i) {
      if (TryLock(threadId)) {
        return;
      }
      ThreadPause();
    }

#ifdef OS_WIN
    if (!nSpinSleepTime &&
        GetThreadPriority(GetCurrentThread()) > THREAD_PRIORITY_NORMAL) {
      nSpinSleepTime = 1;
    } else
#endif

        if (nSpinSleepTime) {
      for (i = 4000; i != 0; --i) {
        if (TryLock(threadId)) {
          return;
        }

        ThreadPause();
        ThreadSleep(0);
      }
    }

    for (;
         ;)  // coded as for instead of while to make easy to breakpoint success
    {
      if (TryLock(threadId)) {
        return;
      }

      ThreadPause();
      ThreadSleep(nSpinSleepTime);
    }
  } else {
    for (;
         ;)  // coded as for instead of while to make easy to breakpoint success
    {
      if (TryLock(threadId)) {
        return;
      }

      ThreadPause();
    }
  }
}
#endif  // !linux


//
// CThreadRWLock
//


void CThreadRWLock::WaitForRead() {
  m_nPendingReaders++;

  do {
    m_mutex.Unlock();
    m_CanRead.Wait();
    m_mutex.Lock();
  } while (m_nWriters);

  m_nPendingReaders--;
}

void CThreadRWLock::LockForWrite() {
  m_mutex.Lock();
  bool bWait = (m_nWriters != 0 || m_nActiveReaders != 0);
  m_nWriters++;
  m_CanRead.Reset();
  m_mutex.Unlock();

  if (bWait) {
    m_CanWrite.Wait();
  }
}

void CThreadRWLock::UnlockWrite() {
  m_mutex.Lock();
  m_nWriters--;
  if (m_nWriters == 0) {
    if (m_nPendingReaders) {
      m_CanRead.Set();
    }
  } else {
    m_CanWrite.Set();
  }
  m_mutex.Unlock();
}


//
// CThreadSpinRWLock
//


void CThreadSpinRWLock::SpinLockForWrite(const u32 threadId) {
  i32 i;

  for (i = 1000; i != 0; --i) {
    if (TryLockForWrite(threadId)) {
      return;
    }
    ThreadPause();
  }

  for (i = 20000; i != 0; --i) {
    if (TryLockForWrite(threadId)) {
      return;
    }

    ThreadPause();
    ThreadSleep(0);
  }

  for (;;)  // coded as for instead of while to make easy to breakpoint success
  {
    if (TryLockForWrite(threadId)) {
      return;
    }

    ThreadPause();
    ThreadSleep(1);
  }
}

void CThreadSpinRWLock::LockForRead() {
  i32 i;

  // In order to grab a read lock, the number of readers must not change and no
  // thread can own the write lock
  LockInfo_t oldValue;
  LockInfo_t newValue;

  oldValue.m_nReaders = m_lockInfo.m_nReaders;
  oldValue.m_writerId = 0;
  newValue.m_nReaders = oldValue.m_nReaders + 1;
  newValue.m_writerId = 0;

  if (m_nWriters == 0 && AssignIf(newValue, oldValue)) return;
  ThreadPause();
  oldValue.m_nReaders = m_lockInfo.m_nReaders;
  newValue.m_nReaders = oldValue.m_nReaders + 1;

  for (i = 1000; i != 0; --i) {
    if (m_nWriters == 0 && AssignIf(newValue, oldValue)) return;
    ThreadPause();
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders + 1;
  }

  for (i = 20000; i != 0; --i) {
    if (m_nWriters == 0 && AssignIf(newValue, oldValue)) return;
    ThreadPause();
    ThreadSleep(0);
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders + 1;
  }

  for (;;)  // coded as for instead of while to make easy to breakpoint success
  {
    if (m_nWriters == 0 && AssignIf(newValue, oldValue)) return;
    ThreadPause();
    ThreadSleep(1);
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders + 1;
  }
}

void CThreadSpinRWLock::UnlockRead() {
  i32 i;

  Assert(m_lockInfo.m_nReaders > 0 && m_lockInfo.m_writerId == 0);
  LockInfo_t oldValue;
  LockInfo_t newValue;

  oldValue.m_nReaders = m_lockInfo.m_nReaders;
  oldValue.m_writerId = 0;
  newValue.m_nReaders = oldValue.m_nReaders - 1;
  newValue.m_writerId = 0;

  if (AssignIf(newValue, oldValue)) return;
  ThreadPause();
  oldValue.m_nReaders = m_lockInfo.m_nReaders;
  newValue.m_nReaders = oldValue.m_nReaders - 1;

  for (i = 500; i != 0; --i) {
    if (AssignIf(newValue, oldValue)) return;
    ThreadPause();
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders - 1;
  }

  for (i = 20000; i != 0; --i) {
    if (AssignIf(newValue, oldValue)) return;
    ThreadPause();
    ThreadSleep(0);
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders - 1;
  }

  for (;;)  // coded as for instead of while to make easy to breakpoint success
  {
    if (AssignIf(newValue, oldValue)) return;
    ThreadPause();
    ThreadSleep(1);
    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    newValue.m_nReaders = oldValue.m_nReaders - 1;
  }
}

void CThreadSpinRWLock::UnlockWrite() {
  Assert(m_lockInfo.m_writerId == ThreadGetCurrentId() &&
         m_lockInfo.m_nReaders == 0);
  static const LockInfo_t newValue = {0, 0};

  ThreadInterlockedExchange64((i64 *)&m_lockInfo, *((i64 *)&newValue));
  --m_nWriters;
}


//
// CThread
//


CThreadLocalPtr<CThread> g_pCurThread;

//---------------------------------------------------------

CThread::CThread()
    :
#ifdef OS_WIN
      m_hThread(nullptr),
#endif
      m_threadId{0},
      m_result{0},
      m_pStackBase{nullptr},
      m_flags{0} {
  m_szName[0] = '\0';
}

//---------------------------------------------------------

CThread::~CThread() {
#ifdef OS_WIN
  if (m_hThread)
#elif OS_POSIX
  if (m_threadId)
#endif
  {
    if (IsAlive()) {
      Msg("Illegal termination of worker thread! Threads must negotiate an end "
          "to the thread before the CThread object is destroyed.\n");
#ifdef OS_WIN

      DoNewAssertDialog(__FILE__, __LINE__,
                        "Illegal termination of worker thread! Threads must "
                        "negotiate an end to the thread before the CThread "
                        "object is destroyed.\n");
#endif
      if (GetCurrentCThread() == this) {
        Stop();  // BUGBUG: Alfred - this doesn't make sense, this destructor
                 // fires from the hosting thread not the thread itself!!
      }
    }
  }
}

//---------------------------------------------------------

const ch *CThread::GetName() {
  AUTO_LOCK(m_Lock);
  if (!m_szName[0]) {
#ifdef OS_WIN
    _snprintf(m_szName, sizeof(m_szName) - 1, "Thread(%p/%p)", this, m_hThread);
#elif OS_POSIX
    _snprintf(m_szName, sizeof(m_szName) - 1, "Thread(%0x%x/0x%x)", this,
              m_threadId);
#endif
    m_szName[sizeof(m_szName) - 1] = 0;
  }
  return m_szName;
}

//---------------------------------------------------------

void CThread::SetName(const ch *pszName) {
  AUTO_LOCK(m_Lock);
  strncpy(m_szName, pszName, sizeof(m_szName) - 1);
  m_szName[sizeof(m_szName) - 1] = 0;
}

//---------------------------------------------------------

bool CThread::Start(u32 nBytesStack) {
  AUTO_LOCK(m_Lock);

  if (IsAlive()) {
    AssertMsg(0, "Tried to create a thread that has already been created!");
    return false;
  }

  bool bInitSuccess = false;

#ifdef OS_WIN
  HANDLE hThread;
  CThreadEvent createComplete;
  ThreadInit_t init = {this, &createComplete, &bInitSuccess};
  m_hThread = hThread = (HANDLE)VCRHook_CreateThread(
      nullptr, nBytesStack, (LPTHREAD_START_ROUTINE)GetThreadProc(),
      new ThreadInit_t(init), 0, &m_threadId);
  if (!hThread) {
    AssertMsg1(0, "Failed to create thread (error 0x%x)", GetLastError());
    return false;
  }
#elif OS_POSIX
  ThreadInit_t init = {this, &bInitSuccess};
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, std::max(nBytesStack, 1024 * 1024));
  if (pthread_create(&m_threadId, &attr, GetThreadProc(),
                     new ThreadInit_t(init)) != 0) {
    AssertMsg1(0, "Failed to create thread (error 0x%x)", GetLastError());
    return false;
  }
  bInitSuccess = true;
#endif

#ifdef OS_WIN
  if (!WaitForCreateComplete(&createComplete)) {
    Msg("Thread failed to initialize\n");
    CloseHandle(m_hThread);
    m_hThread = nullptr;
    return false;
  }
#endif

  if (!bInitSuccess) {
    Msg("Thread failed to initialize\n");
#ifdef OS_WIN
    CloseHandle(m_hThread);
    m_hThread = nullptr;
#elif OS_POSIX
    m_threadId = 0;
#endif
    return false;
  }

#ifdef OS_WIN
  if (!m_hThread) {
    Msg("Thread exited immediately\n");
  }
#endif

#ifdef OS_WIN
  return !!m_hThread;
#elif OS_POSIX
  return !!m_threadId;
#endif
}

//---------------------------------------------------------
//
// Return true if the thread exists. false otherwise
//

bool CThread::IsAlive() {
  DWORD dwExitCode;

  return (
#ifdef OS_WIN
      m_hThread && GetExitCodeThread(m_hThread, &dwExitCode) &&
      dwExitCode == STILL_ACTIVE
#elif OS_POSIX
      m_threadId
#endif
  );
}

//---------------------------------------------------------

bool CThread::Join(u32 timeout) {
#ifdef OS_WIN
  if (m_hThread)
#elif OS_POSIX
  if (m_threadId)
#endif
  {
    AssertMsg(GetCurrentCThread() != this, "Thread cannot be joined with self");

#ifdef OS_WIN
    return ThreadJoin((ThreadHandle_t)m_hThread, timeout);
#elif OS_POSIX
    return ThreadJoin((ThreadHandle_t)m_threadId, timeout);
#endif
  }
  return true;
}

//---------------------------------------------------------

#ifdef OS_WIN

HANDLE CThread::GetThreadHandle() { return m_hThread; }

//---------------------------------------------------------

u32 CThread::GetThreadId() { return m_threadId; }

#endif

//---------------------------------------------------------

i32 CThread::GetResult() { return m_result; }

//---------------------------------------------------------
//
// Forcibly, abnormally, but relatively cleanly stop the thread
//

void CThread::Stop(i32 exitCode) {
  if (!IsAlive()) return;

  if (GetCurrentCThread() == this) {
    m_result = exitCode;
    if (!(m_flags & SUPPORT_STOP_PROTOCOL)) {
      OnExit();
      g_pCurThread = nullptr;

#ifdef OS_WIN
      CloseHandle(m_hThread);
      m_hThread = nullptr;
#endif
      m_threadId = 0;
    }
    throw exitCode;
  } else
    AssertMsg(0, "Only thread can stop self: Use a higher-level protocol");
}

//---------------------------------------------------------

i32 CThread::GetPriority() const {
#ifdef OS_WIN
  return GetThreadPriority(m_hThread);
#elif OS_POSIX
  struct sched_param thread_param;
  i32 policy;
  pthread_getschedparam(m_threadId, &policy, &thread_param);
  return thread_param.sched_priority;
#endif
}

//---------------------------------------------------------

bool CThread::SetPriority(i32 priority) {
#ifdef OS_WIN
  return ThreadSetPriority((ThreadHandle_t)m_hThread, priority);
#elif OS_POSIX
  return ThreadSetPriority((ThreadHandle_t)m_threadId, priority);
#endif
}

//---------------------------------------------------------

u32 CThread::Suspend() {
#ifdef OS_WIN
  return (SuspendThread(m_hThread) != 0);  //-V720
#elif OS_POSIX
  Assert(0);
  return 0;
#endif
}

//---------------------------------------------------------

u32 CThread::Resume() {
#ifdef OS_WIN
  return (ResumeThread(m_hThread) != 0);
#elif OS_POSIX
  Assert(0);
  return 0;
#endif
}

//---------------------------------------------------------

bool CThread::Terminate(i32 exitCode) {
#ifdef OS_WIN
  // I hope you know what you're doing!
  if (!TerminateThread(m_hThread, exitCode)) return false;
  CloseHandle(m_hThread);
  m_hThread = nullptr;
  m_threadId = 0;
#elif OS_POSIX
  pthread_kill(m_threadId, SIGKILL);
  m_threadId = 0;
#endif

  return true;
}

//---------------------------------------------------------
//
// Get the Thread object that represents the current thread, if any.
// Can return nullptr if the current thread was not created using
// CThread
//

CThread *CThread::GetCurrentCThread() { return g_pCurThread; }

//---------------------------------------------------------
//
// Offer a context switch. Under Win32, equivalent to Sleep(0)
//

void CThread::Yield() {
#ifdef OS_WIN
  ::Sleep(0);
#elif OS_POSIX
  pthread_yield();
#endif
}

//---------------------------------------------------------
//
// This method causes the current thread to yield and not to be
// scheduled for further execution until a certain amount of real
// time has elapsed, more or less.
//

void CThread::Sleep(u32 duration) {
#ifdef OS_WIN
  ::Sleep(duration);
#elif OS_POSIX
  usleep(duration * 1000);
#endif
}

//---------------------------------------------------------

bool CThread::Init() { return true; }

//---------------------------------------------------------

void CThread::OnExit() {}

//---------------------------------------------------------
#ifdef OS_WIN
bool CThread::WaitForCreateComplete(CThreadEvent *pEvent) {
  // Force serialized thread creation...
  if (!pEvent->Wait(60000)) {
    AssertMsg(0,
              "Probably deadlock or failure waiting for thread to initialize.");
    return false;
  }
  return true;
}
#endif

//---------------------------------------------------------

CThread::ThreadProc_t CThread::GetThreadProc() { return ThreadProc; }

//---------------------------------------------------------

u32 __stdcall CThread::ThreadProc(LPVOID pv) {
#ifdef OS_POSIX
  ThreadInit_t *pInit = (ThreadInit_t *)pv;
#else
  std::shared_ptr<ThreadInit_t> pInit((ThreadInit_t *)pv);
#endif

  CThread *pThread = pInit->pThread;

  g_pCurThread = pThread;
  g_pCurThread->m_pStackBase = AlignValue(&pThread, 4096);

  pThread->m_result = -1;

  try {
    *(pInit->pfInitSuccess) = pThread->Init();
  } catch (...) {
    *(pInit->pfInitSuccess) = false;
#ifdef OS_WIN
    pInit->pInitCompleteEvent->Set();
#endif
    throw;
  }

  bool bInitSuccess = *(pInit->pfInitSuccess);
#ifdef OS_WIN
  pInit->pInitCompleteEvent->Set();
#endif
  if (!bInitSuccess) return 0;

  if (!Plat_IsInDebugSession() && (pThread->m_flags & SUPPORT_STOP_PROTOCOL)) {
    try {
      pThread->m_result = pThread->Run();
    } catch (...) {
    }
  } else {
    pThread->m_result = pThread->Run();
  }

  pThread->OnExit();
  g_pCurThread = nullptr;

#ifdef OS_WIN
  AUTO_LOCK(pThread->m_Lock);
  CloseHandle(pThread->m_hThread);
  pThread->m_hThread = nullptr;
#endif
  pThread->m_threadId = 0;

  return pInit->pThread->m_result;
}


//

#ifdef OS_WIN
CWorkerThread::CWorkerThread()
    : m_EventSend(true),      // must be manual-reset for PeekCall()
      m_EventComplete(true),  // must be manual-reset to handle multiple wait
                              // with thread properly
      m_Param(0),
      m_ReturnVal(0) {}

//---------------------------------------------------------

i32 CWorkerThread::CallWorker(u32 dw, u32 timeout,
                              bool fBoostWorkerPriorityToMaster) {
  return Call(dw, timeout, fBoostWorkerPriorityToMaster);
}

//---------------------------------------------------------

i32 CWorkerThread::CallMaster(u32 dw, u32 timeout) {
  return Call(dw, timeout, false);
}

//---------------------------------------------------------

HANDLE CWorkerThread::GetCallHandle() { return m_EventSend; }

//---------------------------------------------------------

u32 CWorkerThread::GetCallParam() const { return m_Param; }

//---------------------------------------------------------

i32 CWorkerThread::BoostPriority() {
  i32 iInitialPriority = GetPriority();
  const i32 iNewPriority = ::GetThreadPriority(GetCurrentThread());
  if (iNewPriority > iInitialPriority) SetPriority(iNewPriority);
  return iInitialPriority;
}

//---------------------------------------------------------

static u32 __stdcall DefaultWaitFunc(u32 nHandles, const HANDLE *pHandles,
                                     i32 bWaitAll, u32 timeout) {
  return VCRHook_WaitForMultipleObjects(nHandles, (const void **)pHandles,
                                        bWaitAll, timeout);
}

i32 CWorkerThread::Call(u32 dwParam, u32 timeout, bool fBoostPriority,
                        WaitFunc_t pfnWait) {
  AssertMsg(!m_EventSend.Check(),
            "Cannot perform call if there's an existing call pending");

  AUTO_LOCK(m_Lock);

  if (!IsAlive()) return WTCR_FAIL;

  i32 iInitialPriority = 0;
  if (fBoostPriority) {
    iInitialPriority = BoostPriority();
  }

  // set the parameter, signal the worker thread, wait for the completion to be
  // signaled
  m_Param = dwParam;

  m_EventComplete.Reset();
  m_EventSend.Set();

  WaitForReply(timeout, pfnWait);

  if (fBoostPriority) SetPriority(iInitialPriority);

  return m_ReturnVal;
}

//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------
i32 CWorkerThread::WaitForReply(u32 timeout) {
  return WaitForReply(timeout, nullptr);
}

i32 CWorkerThread::WaitForReply(u32 timeout, WaitFunc_t pfnWait) {
  if (!pfnWait) {
    pfnWait = DefaultWaitFunc;
  }

  HANDLE waits[] = {GetThreadHandle(), m_EventComplete};

  u32 result;
  bool bInDebugger = Plat_IsInDebugSession();

  do {
    // Make sure the thread handle hasn't been closed
    if (!GetThreadHandle()) {
      result = WAIT_OBJECT_0 + 1;
      break;
    }

    result = (*pfnWait)((sizeof(waits) / sizeof(waits[0])), waits, FALSE,
                        (timeout != TT_INFINITE) ? timeout : 30000);

    AssertMsg(timeout != TT_INFINITE || result != WAIT_TIMEOUT,
              "Possible hung thread, call to thread timed out");

  } while (bInDebugger && (timeout == TT_INFINITE && result == WAIT_TIMEOUT));

  if (result != WAIT_OBJECT_0 + 1) {
    if (result == WAIT_TIMEOUT)
      m_ReturnVal = WTCR_TIMEOUT;
    else if (result == WAIT_OBJECT_0) {
      DevMsg(2, "Thread failed to respond, probably exited\n");
      m_EventSend.Reset();
      m_ReturnVal = WTCR_TIMEOUT;
    } else {
      m_EventSend.Reset();
      m_ReturnVal = WTCR_THREAD_GONE;
    }
  }

  return m_ReturnVal;
}

//---------------------------------------------------------
//
// Wait for a request from the client
//
//---------------------------------------------------------

bool CWorkerThread::WaitForCall(u32 *pResult) {
  return WaitForCall(TT_INFINITE, pResult);
}

//---------------------------------------------------------

bool CWorkerThread::WaitForCall(u32 dwTimeout, u32 *pResult) {
  bool returnVal = m_EventSend.Wait(dwTimeout);
  if (pResult) *pResult = m_Param;
  return returnVal;
}

//---------------------------------------------------------
//
// is there a request?
//

bool CWorkerThread::PeekCall(u32 *pParam) {
  if (!m_EventSend.Check()) {
    return false;
  } else {
    if (pParam) {
      *pParam = m_Param;
    }
    return true;
  }
}

//---------------------------------------------------------
//
// Reply to the request
//

void CWorkerThread::Reply(u32 dw) {
  m_Param = 0;
  m_ReturnVal = dw;

  // The request is now complete so PeekCall() should fail from
  // now on
  //
  // This event should be reset BEFORE we signal the client
  m_EventSend.Reset();

  // Tell the client we're finished
  m_EventComplete.Set();
}
#endif  // OS_WIN
