// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_PLATFORM_H_
#define SOURCE_TIER0_INCLUDE_PLATFORM_H_

#include "build/include/build_config.h"

#include <malloc.h>
#include <new.h>

#ifdef OS_WIN
#include <intrin.h>
#elif defined OS_POSIX
#include <alloca.h>
#endif  // OS_WIN

#include <cstddef>      // offsetof
#include <cstdlib>      // bswap
#include <cstring>      // memset
#include <type_traits>  // remove_cv_t

#define NO_STEAM

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/platform_detection.h"
#include "tier0/include/tier0_api.h"
#include "tier0/include/wchartypes.h"

#include "tier0/include/valve_off.h"

#ifdef OS_WIN
// NOTE: Starting in Windows 10, version 1607, MAX_PATH limitations have been
// removed from common Win32 file and directory functions. However, you must
// opt-in to the new behavior.
//
// The Windows API has many functions that also have Unicode versions to permit
// an extended-length path for a maximum total path length of 32,767 characters.
// This type of path is composed of components separated by backslashes, each up
// to the value returned in the lpMaximumComponentLength parameter of the
// GetVolumeInformation function (this value is commonly 255 characters). To
// specify an extended-length path, use the "\\?\" prefix. For example,
// "\\?\D:\very long path".
//
// NOTE: The maximum path of 32, 767 characters is approximate, because the
// "\\?\" prefix may be expanded to a longer string by the system at run time,
// and this expansion applies to the total length.
//
// See https://msdn.microsoft.com/en-us/library/aa365247.aspx#MAXPATH
#define SOURCE_MAX_PATH _MAX_PATH
#elif defined OS_POSIX
#include <climits>
// According to POSIX.1-2001 a buffer of size PATH_MAX suffices, but PATH_MAX
// need not be a defined constant,  and may have to be obtained using
// pathconf(3). And asking pathconf(3) does not really help, since, on the
// one hand POSIX warns that the result of pathconf(3) may be huge and
// unsuitable for mallocing memory, and on the other hand pathconf(3) may return
// -1 to signify that PATH_MAX is not bounded.
//
// See http://man7.org/linux/man-pages/man3/realpath.3.html
#define SOURCE_MAX_PATH PATH_MAX
#endif  // OS_WIN

// Break into debugger, when attached.
#define DebuggerBreakIfDebugging() \
  if (!Plat_IsInDebugSession())    \
    ;                              \
  else                             \
    DebuggerBreak()

// Marks the codepath from here until the next branch entry point as
// unreachable, and asserts if any attempt is made to execute it.
#define SOURCE_UNREACHABLE() \
  {                          \
    Assert(0);               \
    SOURCE_HINT(0);          \
  }

#ifdef OS_WIN
// Alloca defined for this platform.
#define stackalloc(_size) _alloca(AlignValue(_size, 16))
#define stackfree(_p) 0
#define securestackalloc(_size) _malloca(AlignValue(_size, 16))
#define securestackfree(_p) _freea(_p)
#elif defined OS_POSIX
// Alloca defined for this platform.
#define stackalloc(_size) _alloca(AlignValue(_size, 16))
#define stackfree(_p) 0
#endif  // OS_WIN

// FP exception handling.
#ifdef COMPILER_MSVC
inline void SetupFPUControlWord() {
#ifndef ARCH_CPU_X86_64
  // use local to get and store control word.
  u16 tmpCtrlW;
  __asm
  {
    fnstcw word ptr[tmpCtrlW]  // get current control word
    and [tmpCtrlW], 0FCC0h  // Keep infinity control + rounding control
    or [tmpCtrlW], 023Fh  // set to 53-bit, mask only inexact, underflow
    fldcw word ptr[tmpCtrlW]  // put new control word in FPU
  }
#endif  // ARCH_CPU_X86_64
}
#else   // !COMPILER_MSVC
inline void SetupFPUControlWord() {
  __volatile u16 __cw;
  __asm __volatile("fnstcw %0" : "=m"(__cw));
  __cw = __cw & 0x0FCC0;  // keep infinity control, keep rounding mode
  __cw = __cw | 0x023F;   // set 53-bit, no exceptions
  __asm __volatile("fldcw %0" : : "m"(__cw));
}
#endif  // COMPILER_MSVC

// Standard functions for handling endian-ness

// Basic swaps

template <typename T>
inline T WordSwapC(T w) {
  u16 temp = ((*((u16 *)&w) & 0xff00) >> 8);
  temp |= ((*((u16 *)&w) & 0x00ff) << 8);
  return *((T *)&temp);
}

template <typename T>
inline T DWordSwapC(T dw) {
  u32 temp = *((u32 *)&dw) >> 24;
  temp |= ((*((u32 *)&dw) & 0x00FF0000) >> 8);
  temp |= ((*((u32 *)&dw) & 0x0000FF00) << 8);
  temp |= ((*((u32 *)&dw) & 0x000000FF) << 24);
  return *((T *)&temp);
}

// Fast swaps

#ifdef COMPILER_MSVC
#define WordSwap WordSwapAsm
#define DWordSwap DWordSwapAsm

template <typename T>
inline T WordSwapAsm(T w) {
  static_assert(sizeof(T) == sizeof(u16));
  static_assert(sizeof(T) == 2);
  return _byteswap_ushort(w);
}

template <typename T>
inline T DWordSwapAsm(T dw) {
  static_assert(sizeof(T) == sizeof(u32));
  static_assert(sizeof(T) == 4);
  return _byteswap_ulong(dw);
}
#else  // !COMPILER_MSVC
#define WordSwap WordSwapC
#define DWordSwap DWordSwapC
#endif  // COMPILER_MSVC

#if defined(ARCH_CPU_LITTLE_ENDIAN)
#define LITTLE_ENDIAN 1
#endif

// If a swapped f32 passes through the fpu, the bytes may get changed.
// Prevent this by swapping floats as DWORDs.
#define SafeSwapFloat(pOut, pIn) (*((u32 *)pOut) = DWordSwap(*((u32 *)pIn)))

#if defined(LITTLE_ENDIAN)
#define BigShort(val) WordSwap(val)
#define BigWord(val) WordSwap(val)
#define BigLong(val) DWordSwap(val)
#define BigDWord(val) DWordSwap(val)
#define LittleShort(val) (val)
#define LittleWord(val) (val)
#define LittleLong(val) (val)
#define LittleDWord(val) (val)
#define SwapShort(val) BigShort(val)
#define SwapWord(val) BigWord(val)
#define SwapLong(val) BigLong(val)
#define SwapDWord(val) BigDWord(val)

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat(pOut, pIn) SafeSwapFloat(pOut, pIn)
#define LittleFloat(pOut, pIn) (*pOut = *pIn)
#define SwapFloat(pOut, pIn) BigFloat(pOut, pIn)
#else   // !LITTLE_ENDIAN
// @Note (toml 05-02-02): this technique expects the compiler to
// optimize the expression and eliminate the other path. On any new
// platform/compiler this should be tested.
inline short BigShort(short val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? WordSwap(val) : val;
}
inline u16 BigWord(u16 val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? WordSwap(val) : val;
}
inline long BigLong(long val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? DWordSwap(val) : val;
}
inline u32 BigDWord(u32 val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? DWordSwap(val) : val;
}
inline short LittleShort(short val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? val : WordSwap(val);
}
inline u16 LittleWord(u16 val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? val : WordSwap(val);
}
inline long LittleLong(long val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? val : DWordSwap(val);
}
inline u32 LittleDWord(u32 val) {
  i32 test = 1;
  return (*(ch *)&test == 1) ? val : DWordSwap(val);
}
inline short SwapShort(short val) { return WordSwap(val); }
inline u16 SwapWord(u16 val) { return WordSwap(val); }
inline long SwapLong(long val) { return DWordSwap(val); }
inline u32 SwapDWord(u32 val) { return DWordSwap(val); }

// Pass floats by pointer for swapping to avoid truncation in the fpu
inline void BigFloat(f32 *pOut, const f32 *pIn) {
  i32 test = 1;
  (*(ch *)&test == 1) ? SafeSwapFloat(pOut, pIn) : (*pOut = *pIn);
}
inline void LittleFloat(f32 *pOut, const f32 *pIn) {
  i32 test = 1;
  (*(ch *)&test == 1) ? (*pOut = *pIn) : SafeSwapFloat(pOut, pIn);
}
inline void SwapFloat(f32 *pOut, const f32 *pIn) { SafeSwapFloat(pOut, pIn); }
#endif  // LITTLE_ENDIAN

inline unsigned long LoadLittleDWord(unsigned long *base, usize dwordIndex) {
  return LittleDWord(base[dwordIndex]);
}

inline void StoreLittleDWord(unsigned long *base, usize dwordIndex,
                             unsigned long dword) {
  base[dwordIndex] = LittleDWord(dword);
}

// Returns time in seconds since the module was loaded.
SOURCE_TIER0_API f64 Plat_FloatTime();

// Time in milliseconds.
SOURCE_TIER0_API unsigned long Plat_MSTime();

// Query performance timer frequency.
SOURCE_TIER0_API u64 Plat_PerformanceFrequency();

// Processor Information.
struct CPUInformation {
  // Size of this structure, for forward compatibility.
  i32 m_Size;

  bool m_bRDTSC : 1,  // Is RDTSC supported?
      m_bCMOV : 1,    // Is CMOV supported?
      m_bFCMOV : 1,   // Is FCMOV supported?
      m_bSSE : 1,     // Is SSE supported?
      m_bSSE2 : 1,    // Is SSE2 Supported?
      m_b3DNow : 1,   // Is 3DNow! Supported?
      m_bMMX : 1,     // Is MMX supported?
      m_bHT : 1;      // Is HyperThreading supported?

  // Number of logical processors.
  u8 m_nLogicalProcessors;
  // Number of physical processors.
  u8 m_nPhysicalProcessors;

  // Cycles per second.
  i64 m_Speed;

  // CPU vendor.
  ch *m_szProcessorID;
};

// Query CPU information.
SOURCE_TIER0_API const CPUInformation &GetCPUInformation();

// Thread related functions

// Registers the current thread with Tier0's thread management system.
// This should be called on every thread created in the game.
SOURCE_TIER0_API unsigned long Plat_RegisterThread(
    const ch *pName = "Source Thread");

// Registers the current thread as the primary thread.
SOURCE_TIER0_API unsigned long Plat_RegisterPrimaryThread();

#ifdef COMPILER_MSVC
// VC-specific. Sets the thread's name so it has a friendly name in the
// debugger. This should generally only be handled by Plat_RegisterThread
// and Plat_RegisterPrimaryThread.
SOURCE_TIER0_API void Plat_SetThreadName(unsigned long dwThreadID,
                                         const ch *pName);
#endif  // COMPILER_MSVC

// These would be private if it were possible to export private variables
// from a .DLL. They need to be variables because they are checked by
// inline functions at performance critical places.
SOURCE_TIER0_API unsigned long Plat_PrimaryThreadID;

// Returns the ID of the currently executing thread.
SOURCE_TIER0_API unsigned long Plat_GetCurrentThreadID();

// Returns the ID of the primary thread.
inline unsigned long Plat_GetPrimaryThreadID() { return Plat_PrimaryThreadID; }

// Returns true if the current thread is the primary thread.
inline bool Plat_IsPrimaryThread() {
  return Plat_GetPrimaryThreadID() == Plat_GetCurrentThreadID();
}

// Get current process command line.
SOURCE_TIER0_API const ch *Plat_GetCommandLine();

#ifndef OS_WIN
// Helper function for OS's that don't have a GetCommandLine() API.
SOURCE_TIER0_API void Plat_SetCommandLine(const ch *cmdLine);
#endif

// Logs file and line to simple.log.
SOURCE_TIER0_API bool Plat_SimpleLog(const ch *file, i32 line);

#ifdef OS_WIN
// Is debugger attached?
SOURCE_TIER0_API bool Plat_IsInDebugSession();
// Log string to debugger output.
SOURCE_TIER0_API void Plat_DebugString(const ch *);
#else  // !OS_WIN
       // Is debugger attached?
#define Plat_IsInDebugSession() (false)
// Log string to debugger output.
#define Plat_DebugString(s) ((void)0)
#endif  // OS_WIN

// Intel vtune profiler access.
SOURCE_TIER0_API bool vtune(bool resume);

#ifdef OS_WIN
// XBOX Components valid in PC compilation space.
// Custom windows messages for Xbox input.
#define WM_XREMOTECOMMAND (WM_USER + 100)
#define WM_XCONTROLLER_KEY (WM_USER + 101)
#endif  // OS_WIN

// Methods to invoke the constructor, copy constructor, and destructor.
template <typename T>
inline void Construct(T *memory) {
  ::new (memory) T;
}

template <typename T>
inline void CopyConstruct(T *memory, T const &source) {
  ::new (memory) T(source);
}

template <typename T>
inline void Destruct(T *memory) {
  memory->~T();

#ifndef NDEBUG
  memset(memory, 0xDD, sizeof(T));
#endif
}

// A platform-independent way for a contained class to get a pointer to
// its owner. If you know a class is exclusively used in the context of
// some "outer" class, this is a much more space efficient way to get at
// the outer class than having the inner class store a pointer to it.
//
// class COuter {
//  class CInner { // NOTE: This does not need to be a nested class to
//  work
//    void PrintAddressOfOuter() {
// 	    printf("Outer is at %p.\n", GET_OUTER(COuter, m_Inner));
//    }
//  };
//
//  CInner m_Inner;
//  friend class CInner;
// };
#define GET_OUTER(outer_type, outer_member)                                  \
  (reinterpret_cast<outer_type *>(                                           \
      reinterpret_cast<u8 *>(                                                \
          const_cast<std::remove_cv_t<std::remove_pointer_t<decltype(this)>> \
                         *>(this)) -                                         \
      offsetof(outer_type, outer_member)))

// NOTE: Platforms that correctly support templated functions can handle
// portions as templated functions rather than wrapped functions.
//
// Helps automate the process of creating an array of function templates
// that are all specialized by a single integer. This sort of thing is
// often useful in optimization work.
//
// For example, using TEMPLATE_FUNCTION_TABLE, this:
//
// TEMPLATE_FUNCTION_TABLE(i32, Function, (i32 blah, i32 blah), 10) {
//   return argument * argument;
// }
//
// is equivalent to the following:
//
// NOTE: The function has to be wrapped in a class due to code generation
// bugs involved with directly specializing a function based on a
// constant.
//
// template<i32 argument>
// class FunctionWrapper {
//  public:
//   i32 Function(i32 blah, i32 blah) {
//     return argument * argument;
//   }
// };
//
// using FunctionType = i32 (*)(i32 blah, i32 blah);
//
// struct FunctionName {
//  public:
//   enum { count = 10 };
//   FunctionType functions[10];
// };
//
// FunctionType FunctionName::functions[] = {
//  FunctionWrapper<0>::Function,
//  FunctionWrapper<1>::Function,
//  FunctionWrapper<2>::Function,
//  FunctionWrapper<3>::Function,
//  FunctionWrapper<4>::Function,
//  FunctionWrapper<5>::Function,
//  FunctionWrapper<6>::Function,
//  FunctionWrapper<7>::Function,
//  FunctionWrapper<8>::Function,
//  FunctionWrapper<9>::Function
// };
#define TEMPLATE_FUNCTION_TABLE(RETURN_TYPE, NAME, ARGS, COUNT)        \
  using __Type_##NAME = RETURN_TYPE(SOURCE_FASTCALL *) ARGS;           \
                                                                       \
  template <const i32 nArgument>                                       \
  struct __Function_##NAME {                                           \
    static RETURN_TYPE SOURCE_FASTCALL Run ARGS;                       \
  };                                                                   \
                                                                       \
  template <const i32 i>                                               \
  struct __MetaLooper_##NAME : __MetaLooper_##NAME<i - 1> {            \
    __Type_##NAME func;                                                \
    inline __MetaLooper_##NAME() { func = __Function_##NAME<i>::Run; } \
  };                                                                   \
                                                                       \
  template <>                                                          \
  struct __MetaLooper_##NAME<0> {                                      \
    __Type_##NAME func;                                                \
    inline __MetaLooper_##NAME() { func = __Function_##NAME<0>::Run; } \
  };                                                                   \
                                                                       \
  class NAME {                                                         \
   private:                                                            \
    static const __MetaLooper_##NAME<COUNT> m;                         \
                                                                       \
   public:                                                             \
    enum { count = COUNT };                                            \
    static const __Type_##NAME *functions;                             \
  };                                                                   \
                                                                       \
  const __MetaLooper_##NAME<COUNT> NAME::m;                            \
  const __Type_##NAME *NAME::functions = (__Type_##NAME *)&m;          \
                                                                       \
  template <const i32 nArgument>                                       \
  RETURN_TYPE SOURCE_FASTCALL __Function_##NAME<nArgument>::Run ARGS

#if defined(_INC_WINDOWS) && defined(OS_WIN)
template <typename FUNCPTR_TYPE>
class CDynamicFunction {
 public:
  CDynamicFunction(const ch *module, const ch *name,
                   FUNCPTR_TYPE pfnFallback = nullptr)
      : module_{::LoadLibrary(module)},
        func_{module_ ? reinterpret_cast<FUNCPTR_TYPE>(
                            ::GetProcAddress(module_, name))
                      : pfnFallback} {}

  ~CDynamicFunction() {
    if (module_) {
      FreeLibrary(module_);
      module_ = nullptr;
      func_ = nullptr;
    }
  }

  operator bool() const { return func_ != nullptr; }
  bool operator!() const { return !static_cast<bool>(*this); }
  operator FUNCPTR_TYPE() const { return func_; }

 private:
  const HMODULE module_;
  const FUNCPTR_TYPE func_;
};
#endif

#include "tier0/include/valve_on.h"

#endif  // SOURCE_TIER0_INCLUDE_PLATFORM_H_
