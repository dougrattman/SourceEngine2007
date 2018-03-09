// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A collection of utility classes to simplify thread handling, and
// as much as possible contain portability problems. Here avoiding
// including windows.h.

#ifndef SOURCE_TIER0_INCLUDE_THREADTOOLS_H_
#define SOURCE_TIER0_INCLUDE_THREADTOOLS_H_

#include "build/include/build_config.h"

#ifdef OS_WIN
#include <intrin.h>
#endif

#ifdef OS_POSIX
#include <pthread.h>
#include <cerrno>
#endif

#include <climits>

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "base/include/macros.h"
#include "tier0/include/dbg.h"
#include "tier0/include/tier0_api.h"
#include "tier0/include/vcrmode.h"

// #define THREAD_PROFILER 1

#ifndef _RETAIL
#define THREAD_MUTEX_TRACING_SUPPORTED

#if defined(OS_WIN) && !defined(NDEBUG)
#define THREAD_MUTEX_TRACING_ENABLED
#endif
#endif  // !_RETAIL

#ifdef OS_WIN
using HANDLE = void *;  //-V677
#endif

constexpr u32 TT_INFINITE{0xFFFFFFFFui32};  //-V112

#ifndef NO_THREAD_LOCAL
#ifndef THREAD_LOCAL
#define THREAD_LOCAL thread_local
#endif  // THREAD_LOCAL
#endif  // !NO_THREAD_LOCAL

using ThreadId_t = unsigned long;

// Simple thread creation. Differs from VCR mode/CreateThread/_beginthreadex
// in that it accepts a standard C function rather than compiler specific one.
SOURCE_FORWARD_DECLARE_HANDLE(ThreadHandle_t);
using ThreadFunc_t = u32 (*)(void *parameter);

SOURCE_TIER0_API_GLOBAL ThreadHandle_t CreateSimpleThread(ThreadFunc_t,
                                                          void *pParam,
                                                          ThreadId_t *pID,
                                                          u32 stackSize = 0);
SOURCE_TIER0_API ThreadHandle_t CreateSimpleThread(ThreadFunc_t, void *pParam,
                                                   u32 stackSize = 0);
SOURCE_TIER0_API bool ReleaseThreadHandle(ThreadHandle_t);

SOURCE_TIER0_API void ThreadSleep(u32 duration = 0);
SOURCE_TIER0_API u32 ThreadGetCurrentId();
SOURCE_TIER0_API ThreadHandle_t ThreadGetCurrentHandle();
SOURCE_TIER0_API i32 ThreadGetPriority(ThreadHandle_t hThread = nullptr);
SOURCE_TIER0_API bool ThreadSetPriority(ThreadHandle_t hThread, i32 priority);
inline bool ThreadSetPriority(i32 priority) {
  return ThreadSetPriority(nullptr, priority);
}
SOURCE_TIER0_API bool ThreadInMainThread();
SOURCE_TIER0_API void DeclareCurrentThreadIsMainThread();

// NOTE: ThreadedLoadLibraryFunc_t needs to return the sleep time in
// milliseconds or TT_INFINITE
using ThreadedLoadLibraryFunc_t = i32 (*)();
SOURCE_TIER0_API void SetThreadedLoadLibraryFunc(
    ThreadedLoadLibraryFunc_t func);
SOURCE_TIER0_API ThreadedLoadLibraryFunc_t GetThreadedLoadLibraryFunc();

#if defined(OS_WIN) && !defined(ARCH_CPU_X86_64)
extern "C" unsigned long __declspec(dllimport) SOURCE_STDCALL
    GetCurrentThreadId();
#define ThreadGetCurrentId GetCurrentThreadId
#endif

inline void ThreadPause() {
#ifdef OS_WIN
  _mm_pause();
#elif OS_POSIX
  __asm __volatile("pause");
#else
#error "implement me"
#endif
}

SOURCE_TIER0_API bool ThreadJoin(ThreadHandle_t, u32 timeout = TT_INFINITE);

SOURCE_TIER0_API void ThreadSetDebugName(ThreadId_t id, const ch *pszName);
inline void ThreadSetDebugName(const ch *pszName) {
  ThreadSetDebugName((ThreadId_t)-1, pszName);
}

SOURCE_TIER0_API void ThreadSetAffinity(ThreadHandle_t hThread,
                                        uintptr_t nAffinityMask);

enum ThreadWaitResult_t {
  TW_FAILED = 0xffffffff,   // WAIT_FAILED //-V112
  TW_TIMEOUT = 0x00000102,  // WAIT_TIMEOUT
};

#ifdef OS_WIN
SOURCE_TIER0_API i32 ThreadWaitForObjects(i32 nEvents, const HANDLE *pHandles,
                                          bool bWaitAll = true,
                                          u32 timeout = TT_INFINITE);
inline i32 ThreadWaitForObject(HANDLE handle, bool bWaitAll = true,
                               u32 timeout = TT_INFINITE) {
  return ThreadWaitForObjects(1, &handle, bWaitAll, timeout);
}
#endif

// Interlock methods. These perform very fast atomic thread
// safe operations. These are especially relevant in a multi-core setting.

#ifdef OS_WIN
#define NOINLINE
#elif OS_POSIX
#define NOINLINE __attribute__((noinline))
#endif

#ifdef OS_WIN
#define USE_INTRINSIC_INTERLOCKED
#endif

#ifdef USE_INTRINSIC_INTERLOCKED
extern "C" {
long __cdecl _InterlockedIncrement(volatile long *);
long __cdecl _InterlockedDecrement(volatile long *);
long __cdecl _InterlockedExchange(volatile long *, long);
long __cdecl _InterlockedExchangeAdd(volatile long *, long);
long __cdecl _InterlockedCompareExchange(volatile long *, long, long);
}

#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchange)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedCompareExchange64)
#ifdef ARCH_CPU_X86_64
#pragma intrinsic(_InterlockedDecrement64)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_InterlockedExchangeAdd64)
#pragma intrinsic(_InterlockedIncrement64)
#endif

inline long ThreadInterlockedIncrement(long volatile *p) {
  Assert((usize)p % 4 == 0);
  return _InterlockedIncrement(p);
}
inline long ThreadInterlockedDecrement(long volatile *p) {
  Assert((usize)p % 4 == 0);
  return _InterlockedDecrement(p);
}
inline long ThreadInterlockedExchange(long volatile *p, long value) {
  Assert((usize)p % 4 == 0);
  return _InterlockedExchange(p, value);
}
inline long ThreadInterlockedExchangeAdd(long volatile *p, long value) {
  Assert((usize)p % 4 == 0);
  return _InterlockedExchangeAdd(p, value);
}
inline long ThreadInterlockedCompareExchange(long volatile *p, long value,
                                             long comperand) {
  Assert((usize)p % 4 == 0);
  return _InterlockedCompareExchange(p, value, comperand);
}
inline __int64 ThreadInterlockedCompareExchange(__int64 volatile *p,
                                                __int64 value,
                                                __int64 comperand) {
  Assert((usize)p % 8 == 0);
  return _InterlockedCompareExchange64(p, value, comperand);
}
inline bool ThreadInterlockedAssignIf(long volatile *p, long value,
                                      long comperand) {
  Assert((usize)p % 4 == 0);
  return (_InterlockedCompareExchange(p, value, comperand) == comperand);
}
inline bool ThreadInterlockedAssignIf(__int64 volatile *p, __int64 value,
                                      __int64 comperand) {
  Assert((usize)p % 8 == 0);
  return (_InterlockedCompareExchange64(p, value, comperand) == comperand);
}

#ifdef ARCH_CPU_X86_64
inline __int64 ThreadInterlockedIncrement(__int64 *p) {
  Assert((usize)p % 8 == 0);
  return _InterlockedIncrement64(p);
}
inline __int64 ThreadInterlockedDecrement(__int64 *p) {
  Assert((usize)p % 8 == 0);
  return _InterlockedDecrement64(p);
}
inline __int64 ThreadInterlockedExchange(__int64 volatile *p, __int64 value) {
  Assert((usize)p % 8 == 0);
  return _InterlockedExchange64(p, value);
}
inline __int64 ThreadInterlockedExchangeAdd(__int64 volatile *p,
                                            __int64 value) {
  Assert((usize)p % 8 == 0);
  return _InterlockedExchangeAdd64(p, value);
}
#endif

#else
SOURCE_TIER0_API long ThreadInterlockedIncrement(long volatile *) NOINLINE;
SOURCE_TIER0_API long ThreadInterlockedDecrement(long volatile *) NOINLINE;
SOURCE_TIER0_API long ThreadInterlockedExchange(long volatile *,
                                                long value) NOINLINE;
SOURCE_TIER0_API long ThreadInterlockedExchangeAdd(long volatile *,
                                                   long value) NOINLINE;
SOURCE_TIER0_API long ThreadInterlockedCompareExchange(long volatile *,
                                                       long value,
                                                       long comperand) NOINLINE;
SOURCE_TIER0_API bool ThreadInterlockedAssignIf(long volatile *, long value,
                                                long comperand) NOINLINE;
#endif

inline u32 ThreadInterlockedExchangeSubtract(long volatile *p, long value) {
  return ThreadInterlockedExchangeAdd((long volatile *)p, -value);
}

#if defined(USE_INTRINSIC_INTERLOCKED) && !defined(ARCH_CPU_X86_64)
#define TIPTR()
inline void *ThreadInterlockedExchangePointer(void *volatile *p, void *value) {
  return (void *)_InterlockedExchange(reinterpret_cast<long volatile *>(p),
                                      reinterpret_cast<long>(value));
}
inline void *ThreadInterlockedCompareExchangePointer(void *volatile *p,
                                                     void *value,
                                                     void *comperand) {
  return (void *)_InterlockedCompareExchange(
      reinterpret_cast<long volatile *>(p), reinterpret_cast<long>(value),
      reinterpret_cast<long>(comperand));
}
inline bool ThreadInterlockedAssignPointerIf(void *volatile *p, void *value,
                                             void *comperand) {
  return (_InterlockedCompareExchange(reinterpret_cast<long volatile *>(p),
                                      reinterpret_cast<long>(value),
                                      reinterpret_cast<long>(comperand)) ==
          reinterpret_cast<long>(comperand));
}
#else
SOURCE_TIER0_API void *ThreadInterlockedExchangePointer(void *volatile *,
                                                        void *value) NOINLINE;
SOURCE_TIER0_API void *ThreadInterlockedCompareExchangePointer(
    void *volatile *, void *value, void *comperand) NOINLINE;
SOURCE_TIER0_API bool ThreadInterlockedAssignPointerIf(
    void *volatile *, void *value, void *comperand) NOINLINE;
#endif

inline void const *ThreadInterlockedExchangePointerToConst(
    void const *volatile *p, void const *value) {
  return ThreadInterlockedExchangePointer(const_cast<void *volatile *>(p),
                                          const_cast<void *>(value));
}
inline void const *ThreadInterlockedCompareExchangePointerToConst(
    void const *volatile *p, void const *value, void const *comperand) {
  return ThreadInterlockedCompareExchangePointer(
      const_cast<void *volatile *>(p), const_cast<void *>(value),
      const_cast<void *>(comperand));
}
inline bool ThreadInterlockedAssignPointerToConstIf(void const *volatile *p,
                                                    void const *value,
                                                    void const *comperand) {
  return ThreadInterlockedAssignPointerIf(const_cast<void *volatile *>(p),
                                          const_cast<void *>(value),
                                          const_cast<void *>(comperand));
}

SOURCE_TIER0_API i64 ThreadInterlockedIncrement64(i64 volatile *) NOINLINE;
SOURCE_TIER0_API i64 ThreadInterlockedDecrement64(i64 volatile *) NOINLINE;
SOURCE_TIER0_API i64 ThreadInterlockedCompareExchange64(i64 volatile *,
                                                        i64 value,
                                                        i64 comperand) NOINLINE;
SOURCE_TIER0_API i64 ThreadInterlockedExchange64(i64 volatile *,
                                                 i64 value) NOINLINE;
SOURCE_TIER0_API i64 ThreadInterlockedExchangeAdd64(i64 volatile *,
                                                    i64 value) NOINLINE;
SOURCE_TIER0_API bool ThreadInterlockedAssignIf64(volatile i64 *pDest,
                                                  i64 value,
                                                  i64 comperand) NOINLINE;

inline u32 ThreadInterlockedExchangeSubtract(u32 volatile *p, u32 value) {
  return ThreadInterlockedExchangeAdd((long volatile *)p, value);
}
inline u32 ThreadInterlockedIncrement(u32 volatile *p) {
  return ThreadInterlockedIncrement((long volatile *)p);
}
inline u32 ThreadInterlockedDecrement(u32 volatile *p) {
  return ThreadInterlockedDecrement((long volatile *)p);
}
inline u32 ThreadInterlockedExchange(u32 volatile *p, u32 value) {
  return ThreadInterlockedExchange((long volatile *)p, value);
}
inline u32 ThreadInterlockedExchangeAdd(u32 volatile *p, u32 value) {
  return ThreadInterlockedExchangeAdd((long volatile *)p, value);
}
inline u32 ThreadInterlockedCompareExchange(u32 volatile *p, u32 value,
                                            u32 comperand) {
  return ThreadInterlockedCompareExchange((long volatile *)p, value, comperand);
}
inline bool ThreadInterlockedAssignIf(u32 volatile *p, u32 value,
                                      u32 comperand) {
  return ThreadInterlockedAssignIf((long volatile *)p, value, comperand);
}

inline i32 ThreadInterlockedExchangeSubtract(i32 volatile *p, i32 value) {
  return ThreadInterlockedExchangeAdd((long volatile *)p, -value);
}
inline i32 ThreadInterlockedIncrement(i32 volatile *p) {
  return ThreadInterlockedIncrement((long volatile *)p);
}
inline i32 ThreadInterlockedDecrement(i32 volatile *p) {
  return ThreadInterlockedDecrement((long volatile *)p);
}
inline i32 ThreadInterlockedExchange(i32 volatile *p, i32 value) {
  return ThreadInterlockedExchange((long volatile *)p, value);
}
inline i32 ThreadInterlockedExchangeAdd(i32 volatile *p, i32 value) {
  return ThreadInterlockedExchangeAdd((long volatile *)p, value);
}
inline i32 ThreadInterlockedCompareExchange(i32 volatile *p, i32 value,
                                            i32 comperand) {
  return ThreadInterlockedCompareExchange((long volatile *)p, value, comperand);
}
inline bool ThreadInterlockedAssignIf(i32 volatile *p, i32 value,
                                      i32 comperand) {
  return ThreadInterlockedAssignIf((long volatile *)p, value, comperand);
}

// Access to VTune thread profiling
#if defined(OS_WIN) && defined(THREAD_PROFILER)
SOURCE_TIER0_API void ThreadNotifySyncPrepare(void *p);
SOURCE_TIER0_API void ThreadNotifySyncCancel(void *p);
SOURCE_TIER0_API void ThreadNotifySyncAcquired(void *p);
SOURCE_TIER0_API void ThreadNotifySyncReleasing(void *p);
#else
#define ThreadNotifySyncPrepare(p) ((void)0)
#define ThreadNotifySyncCancel(p) ((void)0)
#define ThreadNotifySyncAcquired(p) ((void)0)
#define ThreadNotifySyncReleasing(p) ((void)0)
#endif

// Encapsulation of a thread local datum (needed because THREAD_LOCAL doesn't
// work in a DLL loaded with LoadLibrary()

#ifndef __AFXTLS_H__  // not compatible with some Windows headers
#ifndef NO_THREAD_LOCAL

class SOURCE_TIER0_API_CLASS CThreadLocalBase {
 public:
  CThreadLocalBase();
  ~CThreadLocalBase();

  void *Get() const;
  void Set(void *);

 private:
#ifdef OS_WIN
  u32 m_index;
#elif OS_POSIX
  pthread_key_t m_index;
#endif
};

#ifndef __AFXTLS_H__

template <class T>
class CThreadLocal : public CThreadLocalBase {
  static_assert(sizeof(T) <= sizeof(void *));

 public:
  CThreadLocal() {}

  T Get() const { return reinterpret_cast<T>(CThreadLocalBase::Get()); }

  void Set(T val) { CThreadLocalBase::Set(reinterpret_cast<void *>(val)); }
};

#endif

template <class T = i32>
class CThreadLocalInt : public CThreadLocal<T> {
 public:
  operator const T() const { return this->Get(); }
  i32 operator=(T i) {
    this->Set(i);
    return i;
  }

  T operator++() {
    T i = this->Get();
    this->Set(++i);
    return i;
  }
  T operator++(i32) {
    T i = this->Get();
    this->Set(i + 1);
    return i;
  }

  T operator--() {
    T i = this->Get();
    this->Set(--i);
    return i;
  }
  T operator--(i32) {
    T i = this->Get();
    this->Set(i - 1);
    return i;
  }
};

template <class T>
class CThreadLocalPtr : private CThreadLocalBase {
 public:
  CThreadLocalPtr() {}

  operator const void *() const { return (T *)Get(); }
  operator void *() { return (T *)Get(); }

  operator const T *() const { return (T *)Get(); }
  operator const T *() { return (T *)Get(); }
  operator T *() { return (T *)Get(); }

  i32 operator=(i32 i) {
    AssertMsg(i == 0, "Only nullptr allowed on integer assign");
    Set(nullptr);
    return 0;
  }
  T *operator=(T *p) {
    Set(p);
    return p;
  }

  bool operator!() const { return (!Get()); }
  bool operator!=(i32 i) const {
    AssertMsg(i == 0, "Only nullptr allowed on integer compare");
    return (Get() != nullptr);
  }
  bool operator==(i32 i) const {
    AssertMsg(i == 0, "Only nullptr allowed on integer compare");
    return (Get() == nullptr);
  }
  bool operator==(const void *p) const { return (Get() == p); }
  bool operator==(const std::nullptr_t p) const { return !Get(); }
  bool operator!=(const void *p) const { return (Get() != p); }
  bool operator!=(const std::nullptr_t p) const { return Get(); }
  bool operator==(const T *p) const { return operator==((void *)p); }
  bool operator!=(const T *p) const { return operator!=((void *)p); }

  T *operator->() { return (T *)Get(); }
  T &operator*() { return *((T *)Get()); }

  const T *operator->() const { return (T *)Get(); }
  const T &operator*() const { return *((T *)Get()); }

  const T &operator[](usize i) const { return *((T *)Get() + i); }
  T &operator[](usize i) { return *((T *)Get() + i); }

 private:
  // Disallowed operations
  CThreadLocalPtr(T *pFrom);
  CThreadLocalPtr(const CThreadLocalPtr<T> &from);
  T **operator&();
  T *const *operator&() const;
  void operator=(const CThreadLocalPtr<T> &from);
  bool operator==(const CThreadLocalPtr<T> &p) const;
  bool operator!=(const CThreadLocalPtr<T> &p) const;
};

#endif  // NO_THREAD_LOCAL
#endif  // !__AFXTLS_H__

// A super-fast thread-safe integer A simple class encapsulating the notion of
// an atomic integer used across threads that uses the built in and faster
// "interlocked" functionality rather than a full-blown mutex. Useful for simple
// things like reference counts, etc.

template <typename T>
class CInterlockedIntT {
 public:
  CInterlockedIntT() : m_value(0) { static_assert(sizeof(T) == sizeof(long)); }
  CInterlockedIntT(T value) : m_value(value) {}

  operator T() const { return m_value; }

  bool operator!() const { return (m_value == 0); }
  bool operator==(T rhs) const { return (m_value == rhs); }
  bool operator!=(T rhs) const { return (m_value != rhs); }

  T operator++() { return (T)ThreadInterlockedIncrement((long *)&m_value); }
  T operator++(i32) { return operator++() - 1; }

  T operator--() { return (T)ThreadInterlockedDecrement((long *)&m_value); }
  T operator--(i32) { return operator--() + 1; }

  bool AssignIf(T conditionValue, T newValue) {
    return ThreadInterlockedAssignIf((long *)&m_value, (long)newValue,
                                     (long)conditionValue);
  }

  T operator=(T newValue) {
    ThreadInterlockedExchange((long *)&m_value, newValue);
    return m_value;
  }

  void operator+=(T add) {
    ThreadInterlockedExchangeAdd((long *)&m_value, (long)add);
  }
  void operator-=(T subtract) { operator+=(-subtract); }
  void operator*=(T multiplier) {
    T original, result;
    do {
      original = m_value;
      result = original * multiplier;
    } while (!AssignIf(original, result));
  }
  void operator/=(T divisor) {
    T original, result;
    do {
      original = m_value;
      result = original / divisor;
    } while (!AssignIf(original, result));
  }

  T operator+(T rhs) const { return m_value + rhs; }
  T operator-(T rhs) const { return m_value - rhs; }

 private:
  volatile T m_value;
};

using CInterlockedInt = CInterlockedIntT<i32>;
using CInterlockedUInt = CInterlockedIntT<u32>;

template <typename T>
class CInterlockedPtr {
#ifndef ARCH_CPU_X86_64
  using pointer_type = long;
#else
  using pointer_type = __int64;
#endif
 public:
  CInterlockedPtr() : m_value{nullptr} {
    static_assert(sizeof(T *) == sizeof(pointer_type));
  }
  CInterlockedPtr(T *value) : m_value{value} {}

  operator T *() const { return m_value; }

  bool operator!() const { return m_value == nullptr; }
  bool operator==(T *rhs) const { return m_value == rhs; }
  bool operator!=(T *rhs) const { return m_value != rhs; }

  T *operator++() {
    return ((T *)ThreadInterlockedExchangeAdd((volatile pointer_type *)&m_value,
                                              sizeof(T))) +
           1;
  }
  T *operator++(i32) {
    return (T *)ThreadInterlockedExchangeAdd((volatile pointer_type *)&m_value,
                                             sizeof(T));
  }

  T *operator--() {
    return ((T *)ThreadInterlockedExchangeAdd((volatile pointer_type *)&m_value,
                                              -sizeof(T))) -
           1;
  }
  T *operator--(i32) {
    return (T *)ThreadInterlockedExchangeAdd((volatile pointer_type *)&m_value,
                                             -sizeof(T));
  }

  bool AssignIf(T *conditionValue, T *newValue) {
    return ThreadInterlockedAssignPointerToConstIf(
        (void const **)&m_value, (void const *)newValue,
        (void const *)conditionValue);
  }

  T *operator=(T *newValue) {
    ThreadInterlockedExchangePointerToConst((void const **)&m_value,
                                            (void const *)newValue);
    return newValue;
  }

  void operator+=(isize add) {
    ThreadInterlockedExchangeAdd((pointer_type *)&m_value, add * sizeof(T));
  }
  void operator-=(isize subtract) { operator+=(-subtract); }

  T *operator+(isize rhs) const { return m_value + rhs; }
  T *operator+(usize rhs) const { return m_value + rhs; }
  T *operator-(isize rhs) const { return m_value - rhs; }
  T *operator-(usize rhs) const { return m_value - rhs; }
  usize operator-(T *p) const { return m_value - p; }
  usize operator-(const CInterlockedPtr<T> &p) const {
    return m_value - p.m_value;
  }

 private:
  T *volatile m_value;
};

// Platform independent for critical sections management

class SOURCE_TIER0_API_CLASS CThreadMutex {
 public:
  CThreadMutex();
  ~CThreadMutex();

  // Mutex acquisition/release. Const intentionally defeated.
  _Acquires_lock_(this->m_CriticalSection) void Lock();
  _Acquires_lock_(this->m_CriticalSection) void Lock() const {
    (const_cast<CThreadMutex *>(this))->Lock();
  }
  _Releases_lock_(this->m_CriticalSection) void Unlock();
  _Releases_lock_(this->m_CriticalSection) void Unlock() const {
    (const_cast<CThreadMutex *>(this))->Unlock();
  }

  bool TryLock();
  bool TryLock() const { return (const_cast<CThreadMutex *>(this))->TryLock(); }

  // Use this to make deadlocks easier to track by asserting
  // when it is expected that the current thread owns the mutex
  bool AssertOwnedByCurrentThread();

  // Enable tracing to track deadlock problems
  void SetTrace(bool);

 private:
  // Disallow copying
  CThreadMutex(const CThreadMutex &);
  CThreadMutex &operator=(const CThreadMutex &);

#ifdef OS_WIN
  // Efficient solution to breaking the windows.h dependency, invariant is
  // tested.
#ifdef ARCH_CPU_X86_64
#define TT_SIZEOF_CRITICALSECTION 40
#else
#define TT_SIZEOF_CRITICALSECTION 24
#endif  // ARCH_CPU_X86_64
  u8 m_CriticalSection[TT_SIZEOF_CRITICALSECTION];
#elif OS_POSIX
  pthread_mutex_t m_Mutex;
  pthread_mutexattr_t m_Attr;
#else
#error
#endif

#ifdef THREAD_MUTEX_TRACING_SUPPORTED
  // Debugging (always here to allow mixed debug/release builds w/o changing
  // size)
  u32 m_currentOwnerID;
  u16 m_lockCount;
  bool m_bTrace;
#endif
};

// An alternative mutex that is useful for cases when thread contention is
// rare, but a mutex is required. Instances should be declared volatile.
// Sleep of 0 may not be sufficient to keep high priority threads from starving
// lesser threads. This class is not a suitable replacement for a critical
// section if the resource contention is high.

#if defined(OS_WIN) && !defined(THREAD_PROFILER)

class SOURCE_TIER0_API_CLASS CThreadFastMutex {
 public:
  CThreadFastMutex() : m_ownerID(0), m_depth(0) {}

 private:
  SOURCE_FORCEINLINE bool TryLockInline(const u32 threadId) volatile {
    if (threadId != m_ownerID &&
        !ThreadInterlockedAssignIf((volatile long *)&m_ownerID, (long)threadId,
                                   0))
      return false;

    ++m_depth;
    return true;
  }

  bool TryLock(const u32 threadId) volatile { return TryLockInline(threadId); }

  void Lock(const u32 threadId, u32 nSpinSleepTime) volatile;

 public:
  bool TryLock() volatile {
#ifndef NDEBUG
    if (m_depth == INT_MAX) DebuggerBreak();

    if (m_depth < 0) DebuggerBreak();
#endif
    return TryLockInline(ThreadGetCurrentId());
  }

#ifdef NDEBUG
  SOURCE_FORCEINLINE
#endif
  void Lock(u32 nSpinSleepTime = 0) volatile {
    const u32 threadId = ThreadGetCurrentId();

    if (!TryLockInline(threadId)) {
      ThreadPause();
      Lock(threadId, nSpinSleepTime);
    }
#ifndef NDEBUG
    if (m_ownerID != ThreadGetCurrentId()) DebuggerBreak();

    if (m_depth == INT_MAX) DebuggerBreak();

    if (m_depth < 0) DebuggerBreak();
#endif
  }

#ifdef NDEBUG
  SOURCE_FORCEINLINE
#endif
  void Unlock() volatile {
#ifndef NDEBUG
    if (m_ownerID != ThreadGetCurrentId()) DebuggerBreak();

    if (m_depth <= 0) DebuggerBreak();
#endif

    --m_depth;
    if (!m_depth) ThreadInterlockedExchange(&m_ownerID, 0);
  }

  bool TryLock() const volatile {
    return (const_cast<CThreadFastMutex *>(this))->TryLock();
  }
  void Lock(u32 nSpinSleepTime = 1) const volatile {
    (const_cast<CThreadFastMutex *>(this))->Lock(nSpinSleepTime);
  }
  void Unlock() const volatile {
    (const_cast<CThreadFastMutex *>(this))->Unlock();
  }

  // To match regular CThreadMutex:
  bool AssertOwnedByCurrentThread() { return true; }
  void SetTrace(bool) {}

  u32 GetOwnerId() const { return m_ownerID; }
  i32 GetDepth() const { return m_depth; }

 private:
  volatile u32 m_ownerID;
  i32 m_depth;
};

class alignas(128) CAlignedThreadFastMutex : public CThreadFastMutex {
 public:
  CAlignedThreadFastMutex() {  //-V730
    Assert((usize)this % 128 == 0 && sizeof(*this) == 128);
  }

 private:
  u8 pad[128 - sizeof(CThreadFastMutex)];
};

#else
using CThreadFastMutex = CThreadMutex;
#endif

class CThreadNullMutex {
 public:
  static void Lock() {}
  static void Unlock() {}

  static bool TryLock() { return true; }
  static bool AssertOwnedByCurrentThread() { return true; }
  static void SetTrace(bool) {}

  static u32 GetOwnerId() { return 0; }
  static i32 GetDepth() { return 0; }
};

// A mutex decorator class used to control the use of a mutex, to make it
// less expensive when not multithreading

template <class BaseClass, bool *pCondition>
class CThreadConditionalMutex : public BaseClass {
 public:
  void Lock() {
    if (*pCondition) BaseClass::Lock();
  }
  void Lock() const {
    if (*pCondition) BaseClass::Lock();
  }
  void Unlock() {
    if (*pCondition) BaseClass::Unlock();
  }
  void Unlock() const {
    if (*pCondition) BaseClass::Unlock();
  }

  bool TryLock() {
    if (*pCondition)
      return BaseClass::TryLock();
    else
      return true;
  }
  bool TryLock() const {
    if (*pCondition)
      return BaseClass::TryLock();
    else
      return true;
  }
  bool AssertOwnedByCurrentThread() {
    if (*pCondition)
      return BaseClass::AssertOwnedByCurrentThread();
    else
      return true;
  }
  void SetTrace(bool b) {
    if (*pCondition) BaseClass::SetTrace(b);
  }
};

// Mutex decorator that blows up if another thread enters
template <class BaseClass>
class CThreadTerminalMutex : public BaseClass {
 public:
  bool TryLock() {
    if (!BaseClass::TryLock()) {
      DebuggerBreak();
      return false;
    }
    return true;
  }
  bool TryLock() const {
    if (!BaseClass::TryLock()) {
      DebuggerBreak();
      return false;
    }
    return true;
  }
  void Lock() {
    if (!TryLock()) BaseClass::Lock();
  }
  void Lock() const {
    if (!TryLock()) BaseClass::Lock();
  }
};

// Class to Lock a critical section, and unlock it automatically
// when the lock goes out of scope
template <class MUTEX_TYPE = CThreadMutex>
class CAutoLockT {
 public:
  _Acquires_lock_(this->m_lock) SOURCE_FORCEINLINE CAutoLockT(MUTEX_TYPE &lock)
      : m_lock(lock) {
    m_lock.Lock();
  }

  _Acquires_lock_(this->m_lock) SOURCE_FORCEINLINE
      CAutoLockT(const MUTEX_TYPE &lock)
      : m_lock(const_cast<MUTEX_TYPE &>(lock)) {
    m_lock.Lock();
  }

  _Releases_lock_(this->m_lock) SOURCE_FORCEINLINE ~CAutoLockT() {
    m_lock.Unlock();
  }

 private:
  MUTEX_TYPE &m_lock;

  // Disallow copying
  CAutoLockT<MUTEX_TYPE>(const CAutoLockT<MUTEX_TYPE> &) = delete;
  CAutoLockT<MUTEX_TYPE> &operator=(const CAutoLockT<MUTEX_TYPE> &) = delete;
};

using CAutoLock = CAutoLockT<CThreadMutex>;

template <i32 size>
struct CAutoLockTypeDeducer {};
template <>
struct CAutoLockTypeDeducer<sizeof(CThreadMutex)> {
  typedef CThreadMutex Type_t;
};
template <>
struct CAutoLockTypeDeducer<sizeof(CThreadNullMutex)> {
  typedef CThreadNullMutex Type_t;
};
#if defined(OS_WIN) && !defined(THREAD_PROFILER)
template <>
struct CAutoLockTypeDeducer<sizeof(CThreadFastMutex)> {
  typedef CThreadFastMutex Type_t;
};
template <>
struct CAutoLockTypeDeducer<sizeof(CAlignedThreadFastMutex)> {
  typedef CAlignedThreadFastMutex Type_t;
};
#endif

#define AUTO_LOCK_(type, mutex) \
  CAutoLockT<type> SOURCE_UNIQUE_ID(static_cast<const type &>(mutex))

#define AUTO_LOCK(mutex) \
  AUTO_LOCK_(CAutoLockTypeDeducer<sizeof(mutex)>::Type_t, mutex)

#define AUTO_LOCK_FM(mutex) AUTO_LOCK_(CThreadFastMutex, mutex)

#define LOCAL_THREAD_LOCK_(tag)            \
  ;                                        \
  static CThreadFastMutex autoMutex_##tag; \
  AUTO_LOCK(autoMutex_##tag)

#define LOCAL_THREAD_LOCK() LOCAL_THREAD_LOCK_(_)

// Base class for event, semaphore and mutex objects.
class SOURCE_TIER0_API_CLASS CThreadSyncObject {
 public:
  ~CThreadSyncObject();

  // Query if object is useful
  bool operator!() const;

  // Access handle
#ifdef OS_WIN
  operator HANDLE() { return m_hSyncObject; }
#endif

  // Wait for a signal from the object
  bool Wait(u32 dwTimeout = TT_INFINITE);

 protected:
  CThreadSyncObject();
  void AssertUseable();

#ifdef OS_WIN
  HANDLE m_hSyncObject;
#elif OS_POSIX
  pthread_mutex_t m_Mutex;
  pthread_cond_t m_Condition;
  bool m_bInitalized;
  i32 m_cSet;
  bool m_bManualReset;
#else
#error Please, add sync object support for your platform in tier0/include/threadtools.h
#endif

 private:
  CThreadSyncObject(const CThreadSyncObject &);
  CThreadSyncObject &operator=(const CThreadSyncObject &);
};

// Wrapper for unnamed event objects

#ifdef OS_WIN

// CThreadSemaphore
class SOURCE_TIER0_API_CLASS CThreadSemaphore : public CThreadSyncObject {
 public:
  CThreadSemaphore(long initialValue, long maxValue);

  // Increases the count of the semaphore object by a specified
  // amount.  Wait() decreases the count by one on return.
  bool Release(long releaseCount = 1, long *pPreviousCount = nullptr);

 private:
  CThreadSemaphore(const CThreadSemaphore &);
  CThreadSemaphore &operator=(const CThreadSemaphore &);
};

// A mutex suitable for out-of-process, multi-processor usage
class SOURCE_TIER0_API_CLASS CThreadFullMutex : public CThreadSyncObject {
 public:
  _Acquires_lock_(this->m_hSyncObject)
      CThreadFullMutex(bool bEstablishInitialOwnership = false,
                       const ch *pszName = nullptr);

  // Release ownership of the mutex
  _Releases_lock_(this->m_hSyncObject) bool Release();

  // To match regular CThreadMutex:
  void Lock() { Wait(); }
  void Lock(u32 timeout) { Wait(timeout); }
  _Releases_lock_(this->m_hSyncObject) void Unlock() { Release(); }
  bool AssertOwnedByCurrentThread() { return true; }
  void SetTrace(bool) {}

 private:
  CThreadFullMutex(const CThreadFullMutex &);
  CThreadFullMutex &operator=(const CThreadFullMutex &);
};
#endif

class SOURCE_TIER0_API_CLASS CThreadEvent : public CThreadSyncObject {
 public:
  CThreadEvent(bool fManualReset = false);

  // Set the state to signaled
  bool Set();

  // Set the state to nonsignaled
  bool Reset();

  // Check if the event is signaled
  bool Check();

  bool Wait(u32 dwTimeout = TT_INFINITE);

 private:
  CThreadEvent(const CThreadEvent &);
  CThreadEvent &operator=(const CThreadEvent &);
#ifdef OS_POSIX
  CInterlockedInt m_cSet;
#endif
};

// Hard-wired manual event for use in array declarations
class CThreadManualEvent : public CThreadEvent {
 public:
  CThreadManualEvent() : CThreadEvent(true) {}
};

inline i32 ThreadWaitForEvents(i32 nEvents, const CThreadEvent *pEvents,
                               bool bWaitAll = true,
                               u32 timeout = TT_INFINITE) {
#ifdef OS_POSIX
  Assert(0);
  return 0;
#else
  return ThreadWaitForObjects(nEvents, (const HANDLE *)pEvents, bWaitAll,
                              timeout);
#endif
}

// CThreadRWLock
class SOURCE_TIER0_API_CLASS CThreadRWLock {
 public:
  CThreadRWLock();

  void LockForRead();
  void UnlockRead();
  void LockForWrite();
  void UnlockWrite();

  void LockForRead() const { const_cast<CThreadRWLock *>(this)->LockForRead(); }
  void UnlockRead() const { const_cast<CThreadRWLock *>(this)->UnlockRead(); }
  void LockForWrite() const {
    const_cast<CThreadRWLock *>(this)->LockForWrite();
  }
  void UnlockWrite() const { const_cast<CThreadRWLock *>(this)->UnlockWrite(); }

 private:
  void WaitForRead();

  CThreadFastMutex m_mutex;
  CThreadEvent m_CanWrite;
  CThreadEvent m_CanRead;

  i32 m_nWriters;
  i32 m_nActiveReaders;
  i32 m_nPendingReaders;
};

// CThreadSpinRWLock
#define TFRWL_ALIGN alignas(8)

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4324)

class TFRWL_ALIGN SOURCE_TIER0_API_CLASS CThreadSpinRWLock {
 public:
  CThreadSpinRWLock() {
    static_assert(sizeof(LockInfo_t) == sizeof(i64));
    Assert((uintptr_t)this % 8 == 0);
    memset(this, 0, sizeof(*this));
  }

  bool TryLockForWrite() {
    ++m_nWriters;
    if (!TryLockForWrite(ThreadGetCurrentId())) {
      --m_nWriters;
      return false;
    }
    return true;
  }
  bool TryLockForRead() {
    if (m_nWriters != 0) {
      return false;
    }
    // In order to grab a write lock, the number of readers must not change and
    // no thread can own the write
    LockInfo_t oldValue;
    LockInfo_t newValue;

    oldValue.m_nReaders = m_lockInfo.m_nReaders;
    oldValue.m_writerId = 0;
    newValue.m_nReaders = oldValue.m_nReaders + 1;
    newValue.m_writerId = 0;

    return AssignIf(newValue, oldValue);
  }

  void LockForRead();
  void UnlockRead();

  void LockForWrite() {
    const u32 threadId = ThreadGetCurrentId();

    ++m_nWriters;

    if (!TryLockForWrite(threadId)) {
      ThreadPause();
      SpinLockForWrite(threadId);
    }
  }
  void UnlockWrite();

  bool TryLockForWrite() const {
    return const_cast<CThreadSpinRWLock *>(this)->TryLockForWrite();
  }
  bool TryLockForRead() const {
    return const_cast<CThreadSpinRWLock *>(this)->TryLockForRead();
  }
  void LockForRead() const {
    const_cast<CThreadSpinRWLock *>(this)->LockForRead();
  }
  void UnlockRead() const {
    const_cast<CThreadSpinRWLock *>(this)->UnlockRead();
  }
  void LockForWrite() const {
    const_cast<CThreadSpinRWLock *>(this)->LockForWrite();
  }
  void UnlockWrite() const {
    const_cast<CThreadSpinRWLock *>(this)->UnlockWrite();
  }

 private:
  struct LockInfo_t {
    u32 m_writerId;
    i32 m_nReaders;
  };

  bool AssignIf(const LockInfo_t &newValue, const LockInfo_t &comperand) {
    return ThreadInterlockedAssignIf64((i64 *)&m_lockInfo, *((i64 *)&newValue),
                                       *((i64 *)&comperand));
  }
  bool TryLockForWrite(const u32 threadId) {
    // In order to grab a write lock, there can be no readers and no owners of
    // the write lock
    if (m_lockInfo.m_nReaders > 0 ||
        (m_lockInfo.m_writerId && m_lockInfo.m_writerId != threadId)) {
      return false;
    }

    static const LockInfo_t oldValue = {0, 0};
    LockInfo_t newValue = {threadId, 0};

    return AssignIf(newValue, oldValue);
  }

  void SpinLockForWrite(const u32 threadId);

  volatile LockInfo_t m_lockInfo;
  // CInterlockedInt is inline, so linker can access it.
  MSVC_SCOPED_DISABLE_WARNING(4251, CInterlockedInt m_nWriters);
};

MSVC_END_WARNING_OVERRIDE_SCOPE()

// A thread wrapper similar to a Java thread.
class SOURCE_TIER0_API_CLASS CThread {
 public:
  CThread();
  virtual ~CThread();

  const ch *GetName();
  void SetName(const ch *);

  usize CalcStackDepth(void *pStackVariable) {
    return ((u8 *)m_pStackBase - (u8 *)pStackVariable);
  }

  // Functions for the other threads

  // Start thread running  - error if already running
  virtual bool Start(u32 nBytesStack = 0);

  // Returns true if thread has been created and hasn't yet exited
  bool IsAlive();

  // This method causes the current thread to wait until this thread
  // is no longer alive.
  bool Join(u32 timeout = TT_INFINITE);

#ifdef OS_WIN
  // Access the thread handle directly
  HANDLE GetThreadHandle();
  u32 GetThreadId();
#endif

  i32 GetResult();

  // Functions for both this, and maybe, and other threads

  // Forcibly, abnormally, but relatively cleanly stop the thread
  void Stop(i32 exitCode = 0);

  // Get the priority
  i32 GetPriority() const;

  // Set the priority
  bool SetPriority(i32);

  // Suspend a thread
  u32 Suspend();

  // Resume a suspended thread
  u32 Resume();

  // Force hard-termination of thread.  Used for critical failures.
  bool Terminate(i32 exitCode = 0);

  // Global methods

  // Get the Thread object that represents the current thread, if any.
  // Can return nullptr if the current thread was not created using
  // CThread
  static CThread *GetCurrentCThread();

  // Offer a context switch. Under Win32, equivalent to Sleep(0)
#ifdef Yield
#undef Yield
#endif
  static void Yield();

  // This method causes the current thread to yield and not to be
  // scheduled for further execution until a certain amount of real
  // time has elapsed, more or less.
  static void Sleep(u32 duration);

 protected:
  // Optional pre-run call, with ability to fail-create. Note Init()
  // is forced synchronous with Start()
  virtual bool Init();

  // Thread will run this function on startup, must be supplied by
  // derived class, performs the intended action of the thread.
  virtual i32 Run() = 0;

  // Called when the thread exits
  virtual void OnExit();

#ifdef OS_WIN
  // Allow for custom start waiting
  virtual bool WaitForCreateComplete(CThreadEvent *pEvent);
#endif

  // "Virtual static" facility
  typedef u32(SOURCE_STDCALL *ThreadProc_t)(void *);
  virtual ThreadProc_t GetThreadProc();

  CThreadMutex m_Lock;

 private:
  enum Flags { SUPPORT_STOP_PROTOCOL = 1 << 0 };

  // Thread initially runs this. param is actually 'this'. function
  // just gets this and calls ThreadProc
  struct ThreadInit_t {
    CThread *pThread;
#ifdef OS_WIN
    CThreadEvent *pInitCompleteEvent;
#endif
    bool *pfInitSuccess;
  };

  static u32 SOURCE_STDCALL ThreadProc(void *pv);

  // make copy constructor and assignment operator inaccessible
  CThread(const CThread &) = delete;
  CThread &operator=(const CThread &) = delete;

#ifdef OS_WIN
  HANDLE m_hThread;
  ThreadId_t m_threadId;
#elif OS_POSIX
  pthread_t m_threadId;
#endif
  i32 m_result;
  ch m_szName[64];
  void *m_pStackBase;
  u32 m_flags;
};

// Simple thread class encompasses the notion of a worker thread, handing
// synchronized communication.

#ifdef OS_WIN

// These are internal reserved error results from a call attempt
enum WTCallResult_t {
  WTCR_FAIL = -1,
  WTCR_TIMEOUT = -2,
  WTCR_THREAD_GONE = -3,
};

class SOURCE_TIER0_API_CLASS CWorkerThread : public CThread {
 public:
  CWorkerThread();

  // Inter-thread communication
  //
  // Calls in either direction take place on the same "channel."
  // Seperate functions are specified to make identities obvious

  // Master: Signal the thread, and block for a response
  i32 CallWorker(u32, u32 timeout = TT_INFINITE,
                 bool fBoostWorkerPriorityToMaster = true);

  // Worker: Signal the thread, and block for a response
  i32 CallMaster(u32, u32 timeout = TT_INFINITE);

  // Wait for the next request
  bool WaitForCall(u32 dwTimeout, u32 *pResult = nullptr);
  bool WaitForCall(u32 *pResult = nullptr);

  // Is there a request?
  bool PeekCall(u32 *pParam = nullptr);

  // Reply to the request
  void Reply(u32);

  // Wait for a reply in the case when CallWorker() with timeout != TT_INFINITE
  i32 WaitForReply(u32 timeout = TT_INFINITE);

  // If you want to do WaitForMultipleObjects you'll need to include
  // this handle in your wait list or you won't be responsive
  HANDLE GetCallHandle();

  // Find out what the request was
  u32 GetCallParam() const;

  // Boost the worker thread to the master thread, if worker thread is lesser,
  // return old priority
  i32 BoostPriority();

 protected:
  typedef u32(SOURCE_STDCALL *WaitFunc_t)(u32 nHandles, const HANDLE *pHandles,
                                          i32 bWaitAll, u32 timeout);
  i32 Call(u32, u32 timeout, bool fBoost, WaitFunc_t = nullptr);
  i32 WaitForReply(u32 timeout, WaitFunc_t);

 private:
  CWorkerThread(const CWorkerThread &);
  CWorkerThread &operator=(const CWorkerThread &);

#ifdef OS_WIN
  CThreadEvent m_EventSend;
  CThreadEvent m_EventComplete;
#endif

  u32 m_Param;
  i32 m_ReturnVal;
};

#else

using CWorkerThread = CThread;

#endif

// a unidirectional message queue. A queue of type T. Not especially high speed
// since each message is malloced/freed. Note that if your message class has
// destructors/constructors, they MUST be thread safe!
template <typename T>
class CMessageQueue {
  CThreadEvent SignalEvent;  // signals presence of data
  CThreadMutex QueueAccessMutex;

  // the parts protected by the mutex
  struct MsgNode {
    MsgNode *Next;
    T Data;
  };

  MsgNode *Head;
  MsgNode *Tail;

 public:
  CMessageQueue() { Head = Tail = nullptr; }

  // check for a message. not 100% reliable - someone could grab the message
  // first
  bool MessageWaiting() { return (Head != nullptr); }

  void WaitMessage(T *pMsg) {
    for (;;) {
      while (!MessageWaiting()) SignalEvent.Wait();
      QueueAccessMutex.Lock();
      if (!Head) {
        // multiple readers could make this 0
        QueueAccessMutex.Unlock();
        continue;
      }
      *(pMsg) = Head->Data;
      MsgNode *remove_this = Head;
      Head = Head->Next;
      if (!Head)  // if empty, fix tail ptr
        Tail = nullptr;
      QueueAccessMutex.Unlock();
      delete remove_this;
      break;
    }
  }

  void QueueMessage(T const &Msg) {
    MsgNode *new1 = new MsgNode;
    new1->Data = Msg;
    new1->Next = nullptr;
    QueueAccessMutex.Lock();
    if (Tail) {
      Tail->Next = new1;
      Tail = new1;
    } else {
      Head = new1;
      Tail = new1;
    }
    SignalEvent.Set();
    QueueAccessMutex.Unlock();
  }
};

// CThreadMutex. Inlining to reduce overhead and to allow client code
// to decide debug status (tracing)

#ifdef OS_WIN
using RTL_CRITICAL_SECTION = struct _RTL_CRITICAL_SECTION;
using CRITICAL_SECTION = RTL_CRITICAL_SECTION;

extern "C" {
void __declspec(dllimport) SOURCE_STDCALL
    InitializeCriticalSection(_Out_ CRITICAL_SECTION *);
void __declspec(dllimport) SOURCE_STDCALL
    EnterCriticalSection(_Inout_ CRITICAL_SECTION *);
void __declspec(dllimport) SOURCE_STDCALL
    LeaveCriticalSection(_Inout_ CRITICAL_SECTION *);
void __declspec(dllimport) SOURCE_STDCALL
    DeleteCriticalSection(_Inout_ CRITICAL_SECTION *);
};

//---------------------------------------------------------

inline _Acquires_lock_(this->m_CriticalSection) void CThreadMutex::Lock() {
#ifdef THREAD_MUTEX_TRACING_ENABLED
  u32 thisThreadID = ThreadGetCurrentId();
  if (m_bTrace && m_currentOwnerID && (m_currentOwnerID != thisThreadID))
    Msg("Thread %u about to wait for lock %x owned by %u\n",
        ThreadGetCurrentId(), (CRITICAL_SECTION *)&m_CriticalSection,
        m_currentOwnerID);
#endif

  VCRHook_EnterCriticalSection((CRITICAL_SECTION *)&m_CriticalSection);

#ifdef THREAD_MUTEX_TRACING_ENABLED
  if (m_lockCount == 0) {
    // we now own it for the first time.  Set owner information
    m_currentOwnerID = thisThreadID;
    if (m_bTrace)
      Msg("Thread %u now owns lock 0x%x\n", m_currentOwnerID,
          (CRITICAL_SECTION *)&m_CriticalSection);
  }
  m_lockCount++;
#endif
}

//---------------------------------------------------------

inline _Releases_lock_(this->m_CriticalSection) void CThreadMutex::Unlock() {
#ifdef THREAD_MUTEX_TRACING_ENABLED
  AssertMsg(m_lockCount >= 1, "Invalid unlock of thread lock");
  m_lockCount--;
  if (m_lockCount == 0) {
    if (m_bTrace)
      Msg("Thread %u releasing lock 0x%x\n", m_currentOwnerID,
          (CRITICAL_SECTION *)&m_CriticalSection);
    m_currentOwnerID = 0;
  }
#endif
  LeaveCriticalSection((CRITICAL_SECTION *)&m_CriticalSection);
}

inline bool CThreadMutex::AssertOwnedByCurrentThread() {
#ifdef THREAD_MUTEX_TRACING_ENABLED
  if (ThreadGetCurrentId() == m_currentOwnerID) return true;
  AssertMsg3(0, "Expected thread %u as owner of lock 0x%x, but %u owns",
             ThreadGetCurrentId(), (CRITICAL_SECTION *)&m_CriticalSection,
             m_currentOwnerID);
  return false;
#else
  return true;
#endif
}

inline void CThreadMutex::SetTrace([[maybe_unused]] bool bTrace) {
#ifdef THREAD_MUTEX_TRACING_ENABLED
  m_bTrace = bTrace;
#endif
}

#elif OS_POSIX

inline CThreadMutex::CThreadMutex() {
  // enable recursive locks as we need them
  pthread_mutexattr_init(&m_Attr);
  pthread_mutexattr_settype(&m_Attr, PTHREAD_MUTEX_RECURSIVE_NP);
  pthread_mutex_init(&m_Mutex, &m_Attr);
}

inline CThreadMutex::~CThreadMutex() { pthread_mutex_destroy(&m_Mutex); }

inline void CThreadMutex::Lock() { pthread_mutex_lock(&m_Mutex); }

inline void CThreadMutex::Unlock() { pthread_mutex_unlock(&m_Mutex); }

inline bool CThreadMutex::AssertOwnedByCurrentThread() { return true; }

inline void CThreadMutex::SetTrace(bool fTrace) {}

#endif  // OS_POSIX

// CThreadRWLock inline functions
inline CThreadRWLock::CThreadRWLock()
    : m_CanRead(true),
      m_nWriters(0),
      m_nActiveReaders(0),
      m_nPendingReaders(0) {}

inline void CThreadRWLock::LockForRead() {
  m_mutex.Lock();
  if (m_nWriters) {
    WaitForRead();
  }
  m_nActiveReaders++;
  m_mutex.Unlock();
}

inline void CThreadRWLock::UnlockRead() {
  m_mutex.Lock();
  m_nActiveReaders--;
  if (m_nActiveReaders == 0 && m_nWriters != 0) {
    m_CanWrite.Set();
  }
  m_mutex.Unlock();
}

#endif  // SOURCE_TIER0_INCLUDE_THREADTOOLS_H_
