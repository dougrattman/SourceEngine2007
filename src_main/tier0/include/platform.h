// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_PLATFORM_H_
#define SOURCE_TIER0_INCLUDE_PLATFORM_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#define NO_STEAM

#include <malloc.h>
#include <new.h>

#ifdef OS_WIN
#include <intrin.h>
#endif

#ifdef OS_POSIX
#include <alloca.h>
#endif

#include <cstdlib>  // bswap instrinsic-like functions.
#include <cstring>  // need this for memset

#include "tier0/include/basetypes.h"
#include "tier0/include/calling_conventions.h"
#include "tier0/include/compiler_specific_macroses.h"
#include "tier0/include/platform_detection.h"
#include "tier0/include/valve_off.h"
#include "tier0/include/wchartypes.h"

// portability / compiler settings
#ifdef OS_WIN
#if defined(_M_IX86)
#define __i386__ 1
#endif
#elif OS_POSIX
typedef u32 DWORD;
typedef u16 WORD;
typedef void *HINSTANCE;
#define _MAX_PATH PATH_MAX
#endif  // defined(OS_WIN)

// Defines MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

//  need macro for constant expression
#define ALIGN_VALUE(val, alignment) (((val) + (alignment)-1) & ~((alignment)-1))

// Used to step into the debugger
#ifdef OS_WIN
#define DebuggerBreak() __debugbreak()
#else
#define DebuggerBreak() \
  {}
#endif

#define DebuggerBreakIfDebugging() \
  if (!Plat_IsInDebugSession())    \
    ;                              \
  else                             \
    DebuggerBreak()

// C functions for external declarations that call the appropriate C++ methods
#ifndef EXPORT
#ifdef OS_WIN
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
#endif

#if defined __i386__ && !defined __linux__
#define id386 1
#else
#define id386 0
#endif  // __i386__

// decls for aligning data
#ifdef OS_WIN
#define DECL_ALIGN(x) __declspec(align(x))

#elif OS_POSIX
#define DECL_ALIGN(x) __attribute__((aligned(x)))
#elif
#define DECL_ALIGN(x) /* */
#endif

#define ALIGN8 DECL_ALIGN(8)
#define ALIGN16 DECL_ALIGN(16)
#define ALIGN32 DECL_ALIGN(32)
#define ALIGN128 DECL_ALIGN(128)

// Linux had a few areas where it didn't construct objects in the same order
// that Windows does. So when CVProfile::CVProfile() would access g_pMemAlloc,
// it would crash because the allocator wasn't initalized yet.
#ifdef OS_POSIX
#define CONSTRUCT_EARLY __attribute__((init_priority(101)))
#else
#define CONSTRUCT_EARLY
#endif

#ifdef OS_WIN
#define SELECTANY __declspec(selectany)
#elif OS_POSIX
#define SELECTANY __attribute__((weak))
#else
#define SELECTANY static
#endif

#ifdef OS_WIN

// Used for dll exporting and importing
#define DLL_EXPORT extern "C" __declspec(dllexport)
#define DLL_IMPORT extern "C" __declspec(dllimport)

// Can't use extern "C" when DLL exporting a class
#define DLL_CLASS_EXPORT __declspec(dllexport)
#define DLL_CLASS_IMPORT __declspec(dllimport)

// Can't use extern "C" when DLL exporting a global
#define DLL_GLOBAL_EXPORT extern __declspec(dllexport)
#define DLL_GLOBAL_IMPORT extern __declspec(dllimport)

#elif defined OS_POSIX
// Used for dll exporting and importing
#define DLL_EXPORT extern "C"
#define DLL_IMPORT extern "C"

// Can't use extern "C" when DLL exporting a class
#define DLL_CLASS_EXPORT
#define DLL_CLASS_IMPORT

// Can't use extern "C" when DLL exporting a global
#define DLL_GLOBAL_EXPORT extern
#define DLL_GLOBAL_IMPORT extern

#else
#error "Unsupported Platform."
#endif

// Force a function call site -not- to inlined. (useful for profiling)
#define DONT_INLINE(a) (((i32)(a) + 1) ? (a) : (a))

// Pass hints to the compiler to prevent it from generating unnessecary / stupid
// code in certain situations.  Several compilers other than MSVC also have an
// equivilent construct.
//
// Essentially the 'Hint' is that the condition specified is assumed to be true
// at that point in the compilation.  If '0' is passed, then the compiler
// assumes that any subsequent code in the same 'basic block' is unreachable,
// and thus usually removed.
#ifdef COMPILER_MSVC
#define HINT(THE_HINT) __assume((THE_HINT))
#else
#define HINT(THE_HINT) 0
#endif

// Marks the codepath from here until the next branch entry point as
// unreachable, and asserts if any attempt is made to execute it.
#define UNREACHABLE() \
  {                   \
    Assert(0);        \
    HINT(0);          \
  }

// In cases where no default is present or appropriate, this causes MSVC to
// generate as little code as possible, and throw an assertion in debug.
#define NO_DEFAULT \
  default:         \
    UNREACHABLE();

#ifdef OS_WIN
// Alloca defined for this platform
#define stackalloc(_size) _alloca(ALIGN_VALUE(_size, 16))
#define stackfree(_p) 0
#define securestackalloc(_size) _malloca(ALIGN_VALUE(_size, 16))
#define securestackfree(_p) _freea(_p)
#elif OS_POSIX
// Alloca defined for this platform
#define stackalloc(_size) _alloca(ALIGN_VALUE(_size, 16))
#define stackfree(_p) 0
#endif

#ifdef OS_WIN
#define RESTRICT __restrict
#define RESTRICT_FUNC __declspec(restrict)
#else
#define RESTRICT
#define RESTRICT_FUNC
#endif

//-----------------------------------------------------------------------------
// fsel
//-----------------------------------------------------------------------------
static FORCEINLINE f32 fsel(f32 fComparand, f32 fValGE, f32 fLT) {
  return fComparand >= 0 ? fValGE : fLT;
}
static FORCEINLINE f64 fsel(f64 fComparand, f64 fValGE, f64 fLT) {
  return fComparand >= 0 ? fValGE : fLT;
}

//-----------------------------------------------------------------------------
// FP exception handling
//-----------------------------------------------------------------------------

#if defined(COMPILER_MSVC)
inline void SetupFPUControlWord() {
#ifndef ARCH_CPU_X86_64
  // use local to get and store control word
  u16 tmpCtrlW;
  __asm
  {
    fnstcw word ptr[tmpCtrlW] /* get current control word */
    and [tmpCtrlW], 0FCC0h /* Keep infinity control + rounding control */
    or [tmpCtrlW], 023Fh /* set to 53-bit, mask only inexact, underflow */
    fldcw word ptr[tmpCtrlW] /* put new control word in FPU */
  }
#endif
}
#else
inline void SetupFPUControlWord() {
  __volatile u16 __cw;
  __asm __volatile("fnstcw %0" : "=m"(__cw));
  __cw = __cw & 0x0FCC0;  // keep infinity control, keep rounding mode
  __cw = __cw | 0x023F;   // set 53-bit, no exceptions
  __asm __volatile("fldcw %0" : : "m"(__cw));
}
#endif  // COMPILER_MSVC

//-----------------------------------------------------------------------------
// Purpose: Standard functions for handling endian-ness
//-----------------------------------------------------------------------------

//-------------------------------------
// Basic swaps
//-------------------------------------

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

//-------------------------------------
// Fast swaps
//-------------------------------------

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
#else
#define WordSwap WordSwapC
#define DWordSwap DWordSwapC
#endif

//-------------------------------------
// The typically used methods.
//-------------------------------------

#if defined(__i386__)
#define LITTLE_ENDIAN 1
#endif

#if defined(_SGI_SOURCE)
#define BIG_ENDIAN 1
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

#elif defined(BIG_ENDIAN)

#define BigShort(val) (val)
#define BigWord(val) (val)
#define BigLong(val) (val)
#define BigDWord(val) (val)
#define LittleShort(val) WordSwap(val)
#define LittleWord(val) WordSwap(val)
#define LittleLong(val) DWordSwap(val)
#define LittleDWord(val) DWordSwap(val)
#define SwapShort(val) LittleShort(val)
#define SwapWord(val) LittleWord(val)
#define SwapLong(val) LittleLong(val)
#define SwapDWord(val) LittleDWord(val)

// Pass floats by pointer for swapping to avoid truncation in the fpu
#define BigFloat(pOut, pIn) (*pOut = *pIn)
#define LittleFloat(pOut, pIn) SafeSwapFloat(pOut, pIn)
#define SwapFloat(pOut, pIn) LittleFloat(pOut, pIn)

#else

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

#endif

inline unsigned long LoadLittleDWord(unsigned long *base, usize dwordIndex) {
  return LittleDWord(base[dwordIndex]);
}

inline void StoreLittleDWord(unsigned long *base, usize dwordIndex,
                             unsigned long dword) {
  base[dwordIndex] = LittleDWord(dword);
}

#ifndef STATIC_TIER0

#ifdef TIER0_DLL_EXPORT
#define PLATFORM_INTERFACE DLL_EXPORT
#define PLATFORM_OVERLOAD DLL_GLOBAL_EXPORT
#define PLATFORM_CLASS DLL_CLASS_EXPORT
#else
#define PLATFORM_INTERFACE DLL_IMPORT
#define PLATFORM_OVERLOAD DLL_GLOBAL_IMPORT
#define PLATFORM_CLASS DLL_CLASS_IMPORT
#endif

#else  // BUILD_AS_DLL

#define PLATFORM_INTERFACE extern
#define PLATFORM_OVERLOAD
#define PLATFORM_CLASS

#endif  // BUILD_AS_DLL

// Returns time in seconds since the module was loaded.
PLATFORM_INTERFACE f64 Plat_FloatTime();
// Time in milliseconds.
PLATFORM_INTERFACE unsigned long Plat_MSTime();
// Query timer frequency.
PLATFORM_INTERFACE u64 Plat_PerformanceFrequency();

// b/w compatibility
#define Sys_FloatTime Plat_FloatTime

// Processor Information:
struct CPUInformation {
  i32 m_Size;  // Size of this structure, for forward compatibility.

  bool m_bRDTSC : 1,  // Is RDTSC supported?
      m_bCMOV : 1,    // Is CMOV supported?
      m_bFCMOV : 1,   // Is FCMOV supported?
      m_bSSE : 1,     // Is SSE supported?
      m_bSSE2 : 1,    // Is SSE2 Supported?
      m_b3DNow : 1,   // Is 3DNow! Supported?
      m_bMMX : 1,     // Is MMX supported?
      m_bHT : 1;      // Is HyperThreading supported?

  u8 m_nLogicalProcessors;   // Number op logical processors.
  u8 m_nPhysicalProcessors;  // Number of physical processors

  i64 m_Speed;  // In cycles per second.

  ch *m_szProcessorID;  // Processor vendor Identification.
};

PLATFORM_INTERFACE const CPUInformation &GetCPUInformation();

PLATFORM_INTERFACE void GetCurrentDate(i32 *pDay, i32 *pMonth, i32 *pYear);

// Thread related functions
// Registers the current thread with Tier0's thread management system.
// This should be called on every thread created in the game.
PLATFORM_INTERFACE unsigned long Plat_RegisterThread(
    const ch *pName = "Source Thread");

// Registers the current thread as the primary thread.
PLATFORM_INTERFACE unsigned long Plat_RegisterPrimaryThread();

// VC-specific. Sets the thread's name so it has a friendly name in the
// debugger. This should generally only be handled by Plat_RegisterThread and
// Plat_RegisterPrimaryThread
PLATFORM_INTERFACE void Plat_SetThreadName(unsigned long dwThreadID,
                                           const ch *pName);

// These would be private if it were possible to export private variables from a
// .DLL. They need to be variables because they are checked by inline functions
// at performance critical places.
PLATFORM_INTERFACE unsigned long Plat_PrimaryThreadID;

// Returns the ID of the currently executing thread.
PLATFORM_INTERFACE unsigned long Plat_GetCurrentThreadID();

// Returns the ID of the primary thread.
inline unsigned long Plat_GetPrimaryThreadID() { return Plat_PrimaryThreadID; }

// Returns true if the current thread is the primary thread.
inline bool Plat_IsPrimaryThread() {
  // return true;
  return (Plat_GetPrimaryThreadID() == Plat_GetCurrentThreadID());
}

//-----------------------------------------------------------------------------
// Process related functions
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE const ch *Plat_GetCommandLine();
#ifndef OS_WIN
// helper function for OS's that don't have a ::GetCommandLine() call
PLATFORM_INTERFACE void Plat_SetCommandLine(const ch *cmdLine);
#endif
PLATFORM_INTERFACE const ch *Plat_GetCommandLineA();

//-----------------------------------------------------------------------------
// Just logs file and line to simple.log
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE bool Plat_SimpleLog(const ch *file, i32 line);

//-----------------------------------------------------------------------------
// Returns true if debugger attached, false otherwise
//-----------------------------------------------------------------------------
#ifdef OS_WIN
PLATFORM_INTERFACE bool Plat_IsInDebugSession();
PLATFORM_INTERFACE void Plat_DebugString(const ch *);
#else
#define Plat_IsInDebugSession() (false)
#define Plat_DebugString(s) ((void)0)
#endif

//-----------------------------------------------------------------------------
// XBOX Components valid in PC compilation space
//-----------------------------------------------------------------------------

#define XBOX_DVD_SECTORSIZE 2048
#define XBOX_DVD_ECC_SIZE 32768  // driver reads in quantum ECC blocks
#define XBOX_HDD_SECTORSIZE 512

// Custom windows messages for Xbox input
#define WM_XREMOTECOMMAND (WM_USER + 100)
#define WM_XCONTROLLER_KEY (WM_USER + 101)
#define WM_SYS_UI (WM_USER + 102)
#define WM_SYS_SIGNINCHANGED (WM_USER + 103)
#define WM_SYS_STORAGEDEVICESCHANGED (WM_USER + 104)
#define WM_SYS_PROFILESETTINGCHANGED (WM_USER + 105)
#define WM_SYS_MUTELISTCHANGED (WM_USER + 106)
#define WM_SYS_INPUTDEVICESCHANGED (WM_USER + 107)
#define WM_SYS_INPUTDEVICECONFIGCHANGED (WM_USER + 108)
#define WM_LIVE_CONNECTIONCHANGED (WM_USER + 109)
#define WM_LIVE_INVITE_ACCEPTED (WM_USER + 110)
#define WM_LIVE_LINK_STATE_CHANGED (WM_USER + 111)
#define WM_LIVE_CONTENT_INSTALLED (WM_USER + 112)
#define WM_LIVE_MEMBERSHIP_PURCHASED (WM_USER + 113)
#define WM_LIVE_VOICECHAT_AWAY (WM_USER + 114)
#define WM_LIVE_PRESENCE_CHANGED (WM_USER + 115)
#define WM_FRIENDS_PRESENCE_CHANGED (WM_USER + 116)
#define WM_FRIENDS_FRIEND_ADDED (WM_USER + 117)
#define WM_FRIENDS_FRIEND_REMOVED (WM_USER + 118)
#define WM_CUSTOM_GAMEBANNERPRESSED (WM_USER + 119)
#define WM_CUSTOM_ACTIONPRESSED (WM_USER + 120)
#define WM_XMP_STATECHANGED (WM_USER + 121)
#define WM_XMP_PLAYBACKBEHAVIORCHANGED (WM_USER + 122)
#define WM_XMP_PLAYBACKCONTROLLERCHANGED (WM_USER + 123)

inline const ch *GetPlatformExt() { return IsX360() ? ".360" : ""; }

// flat view, 6 hw threads
#define XBOX_PROCESSOR_0 (1 << 0)
#define XBOX_PROCESSOR_1 (1 << 1)
#define XBOX_PROCESSOR_2 (1 << 2)
#define XBOX_PROCESSOR_3 (1 << 3)
#define XBOX_PROCESSOR_4 (1 << 4)
#define XBOX_PROCESSOR_5 (1 << 5)

// core view, 3 cores with 2 hw threads each
#define XBOX_CORE_0_HWTHREAD_0 XBOX_PROCESSOR_0
#define XBOX_CORE_0_HWTHREAD_1 XBOX_PROCESSOR_1
#define XBOX_CORE_1_HWTHREAD_0 XBOX_PROCESSOR_2
#define XBOX_CORE_1_HWTHREAD_1 XBOX_PROCESSOR_3
#define XBOX_CORE_2_HWTHREAD_0 XBOX_PROCESSOR_4
#define XBOX_CORE_2_HWTHREAD_1 XBOX_PROCESSOR_5

// Include additional dependant header components.
#include "tier0/include/fasttimer.h"

// Methods to invoke the constructor, copy constructor, and destructor
template <class T>
inline void Construct(T *pMemory) {
  ::new (pMemory) T;
}

template <class T>
inline void CopyConstruct(T *pMemory, T const &src) {
  ::new (pMemory) T(src);
}

template <class T>
inline void Destruct(T *pMemory) {
  pMemory->~T();

#ifndef NDEBUG
  memset(pMemory, 0xDD, sizeof(T));
#endif
}

//
// GET_OUTER()
//
// A platform-independent way for a contained class to get a pointer to its
// owner. If you know a class is exclusively used in the context of some
// "outer" class, this is a much more space efficient way to get at the outer
// class than having the inner class store a pointer to it.
//
//	class COuter
//	{
//		class CInner // Note: this does not need to be a nested class to
// work
//		{
// void PrintAddressOfOuter()
// {
// 	printf( "Outer is at 0x%x\n", GET_OUTER( COuter, m_Inner ) );
// }
//		};
//
//		CInner m_Inner;
//		friend class CInner;
//	};

#define GET_OUTER(OuterType, OuterMember) \
  ((OuterType *)((u8 *)this - offsetof(OuterType, OuterMember)))

/*	TEMPLATE_FUNCTION_TABLE()

(Note added to platform.h so platforms that correctly support templated
functions can handle portions as templated functions rather than
wrapped functions)

Helps automate the process of creating an array of function
templates that are all specialized by a single integer.
This sort of thing is often useful in optimization work.

For example, using TEMPLATE_FUNCTION_TABLE, this:

TEMPLATE_FUNCTION_TABLE(i32, Function, ( i32 blah, i32 blah ), 10)
{
return argument * argument;
}

is equivilent to the following:

(NOTE: the function has to be wrapped in a class due to code
generation bugs involved with directly specializing a function
based on a constant.)

template<i32 argument>
class FunctionWrapper
{
public:
i32 Function( i32 blah, i32 blah )
{
return argument*argument;
}
}

typedef i32 (*FunctionType)( i32 blah, i32 blah );

class FunctionName
{
public:
enum { count = 10 };
FunctionType functions[10];
};

FunctionType FunctionName::functions[] =
{
FunctionWrapper<0>::Function,
FunctionWrapper<1>::Function,
FunctionWrapper<2>::Function,
FunctionWrapper<3>::Function,
FunctionWrapper<4>::Function,
FunctionWrapper<5>::Function,
FunctionWrapper<6>::Function,
FunctionWrapper<7>::Function,
FunctionWrapper<8>::Function,
FunctionWrapper<9>::Function
};
*/

PLATFORM_INTERFACE bool vtune(bool resume);

#define TEMPLATE_FUNCTION_TABLE(RETURN_TYPE, NAME, ARGS, COUNT)        \
                                                                       \
  typedef RETURN_TYPE(FASTCALL *__Type_##NAME) ARGS;                   \
                                                                       \
  template <const i32 nArgument>                                       \
                                                                       \
  struct __Function_##NAME {                                           \
    static RETURN_TYPE FASTCALL Run ARGS;                              \
  };                                                                   \
                                                                       \
  template <const i32 i>                                               \
                                                                       \
  struct __MetaLooper_##NAME : __MetaLooper_##NAME<i - 1> {            \
    __Type_##NAME func;                                                \
    inline __MetaLooper_##NAME() { func = __Function_##NAME<i>::Run; } \
  };                                                                   \
                                                                       \
  template <>                                                          \
                                                                       \
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
                                                                       \
  const __Type_##NAME *NAME::functions = (__Type_##NAME *)&m;          \
                                                                       \
  template <const i32 nArgument>                                       \
                                                                       \
  RETURN_TYPE FASTCALL __Function_##NAME<nArgument>::Run ARGS

#define LOOP_INTERCHANGE(BOOLEAN, CODE) \
  if ((BOOLEAN)) {                      \
    CODE;                               \
  } else {                              \
    CODE;                               \
  }

#if defined(_INC_WINDOWS) && defined(OS_WIN)
template <typename FUNCPTR_TYPE>
class CDynamicFunction {
 public:
  CDynamicFunction(const ch *pszModule, const ch *pszName,
                   FUNCPTR_TYPE pfnFallback = nullptr) {
    m_pfn = pfnFallback;

    m_module = ::LoadLibrary(pszModule);
    if (m_module) m_pfn = (FUNCPTR_TYPE)::GetProcAddress(m_module, pszName);
  }
  ~CDynamicFunction() {
    if (m_module) {
      FreeLibrary(m_module);
      m_module = nullptr;
      m_pfn = nullptr;
    }
  }

  operator bool() const { return m_pfn != nullptr; }
  bool operator!() const { return !m_pfn; }
  operator FUNCPTR_TYPE() const { return m_pfn; }

 private:
  FUNCPTR_TYPE m_pfn;
  HMODULE m_module;
};
#endif

#include "tier0/include/valve_on.h"

#endif  // SOURCE_TIER0_INCLUDE_PLATFORM_H_
