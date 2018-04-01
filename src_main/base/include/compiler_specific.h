// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_COMPILER_SPECIFIC_H_
#define BASE_INCLUDE_COMPILER_SPECIFIC_H_

#include "build/include/build_config.h"

#ifdef COMPILER_MSVC

// For source annotations.
#include <sal.h>

// Begins MSVC warning override scope.
#define MSVC_BEGIN_WARNING_OVERRIDE_SCOPE() __pragma(warning(push))

// Disable MSVC warning |warning_level|.
#define MSVC_DISABLE_WARNING(warning_level) \
  __pragma(warning(disable : warning_level))

// Ends MSVC warning override scope.
#define MSVC_END_WARNING_OVERRIDE_SCOPE() __pragma(warning(pop))

// Disable MSVC warning |warning_level| for code |code|.
#define MSVC_SCOPED_DISABLE_WARNING(warning_level, code) \
  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()                    \
  MSVC_DISABLE_WARNING(warning_level)                    \
  code MSVC_END_WARNING_OVERRIDE_SCOPE()

// Begins scope with disabling some turned off by default MSVC warnings which
// can be turned on in case /Wall compiler flag was used.  MSVC can't disable
// warnings in system headers only for now. See
// https://docs.microsoft.com/en-us/cpp/preprocessor/compiler-warnings-that-are-off-by-default
#define MSVC_BEGIN_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE() \
  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()                        \
  MSVC_DISABLE_WARNING(4365)                                 \
  MSVC_DISABLE_WARNING(4514)                                 \
  MSVC_DISABLE_WARNING(4571)                                 \
  MSVC_DISABLE_WARNING(4625)                                 \
  MSVC_DISABLE_WARNING(4626)                                 \
  MSVC_DISABLE_WARNING(4710)                                 \
  MSVC_DISABLE_WARNING(4774)                                 \
  MSVC_DISABLE_WARNING(4820)                                 \
  MSVC_DISABLE_WARNING(5026)                                 \
  MSVC_DISABLE_WARNING(5027)

// Ends scope with disabling some turned off by default MSVC warnings which can
// be turned on in case /Wall compiler flag was used.
#define MSVC_END_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE() \
  MSVC_END_WARNING_OVERRIDE_SCOPE()

#else  // !COMPILER_MSVC

#define MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
#define MSVC_DISABLE_WARNING(warning_level)
#define MSVC_END_WARNING_OVERRIDE_SCOPE()
#define MSVC_SCOPED_DISABLE_WARNING(warning_level, code) code
#define MSVC_BEGIN_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE()
#define MSVC_END_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE()

#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
// This can be used to ensure the size of pointers to members when declaring
// a pointer type for a class that has only been forward declared.
// See https://docs.microsoft.com/en-us/cpp/cpp/inheritance-keywords
#define MSVC_SINGLE_INHERITANCE __single_inheritance
#define MSVC_MULTIPLE_INHERITANCE __multiple_inheritance
#else  // !COMPILER_MSVC
#define MSVC_SINGLE_INHERITANCE
#define MSVC_MULTIPLE_INHERITANCE
#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
// This form of __declspec can be applied to any class declaration, but
// should only be applied to pure interface classes, that is, classes that
// will never be instantiated on their own.  The __declspec stops the compiler
// from generating code to initialize the vfptr in the constructor(s) and
// destructor of the class.  In many cases, this removes the only references
// to the vtable that are associated with the class and, thus, the linker
// will remove it.  Using this form of __declspec can result in a significant
// reduction in code size.
//
// If you attempt to instantiate a class marked with novtable and then access
// a class member, you will receive an access violation (AV).
//
// See https://docs.microsoft.com/en-us/cpp/cpp/novtable
#define MSVC_NOVTABLE __declspec(novtable)
#else  // !COMPILER_MSVC
#define MSVC_NOVTABLE
#endif  // COMPILER_MSVC

// Defines class with (potentially) no virtual table.  Useful for pure virtual
// classes aka interfaces in Java/C#.
#define the_interface class MSVC_NOVTABLE

#ifdef COMPILER_MSVC
// For functions declared with the naked attribute, the compiler generates code
// without prolog and epilog code. You can use this feature to write your own
// prolog/epilog code sequences using inline assembler code.  Naked functions
// are particularly useful in writing virtual device drivers.  Note that the
// naked attribute is only valid on x86 and ARM, and is not available on x64.
//
// See https://docs.microsoft.com/en-us/cpp/cpp/naked-cpp
#define MSVC_NAKED __declspec(naked)
#else  // !COMPILER_MSVC
#define MSVC_NAKED
#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
// __restrict keyword indicates that a symbol is not aliased in the current
// scope. See https://docs.microsoft.com/en-us/cpp/cpp/extension-restrict
#define SOURCE_RESTRICT __restrict

// When applied to a function declaration or definition that returns a pointer
// type, restrict tells the compiler that the function returns an object that is
// not aliased, that is, referenced by any other pointers.  This allows the
// compiler to perform additional optimizations.
//
// See https://docs.microsoft.com/en-us/cpp/cpp/restrict
#define SOURCE_RESTRICT_FUNCTION __declspec(restrict)
#elif defined COMPILER_GCC || defined COMPILER_CLANG
// See https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html
#define SOURCE_RESTRICT __restrict__
#define SOURCE_RESTRICT_FUNCTION __restrict__
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add restrict keyword support for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
#define SOURCE_FORCEINLINE __forceinline
#elif defined COMPILER_GCC || defined COMPILER_CLANG
#define SOURCE_FORCEINLINE __attribute__((always_inline))
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add force inline support for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
#define SOURCE_STDCALL __stdcall
#define SOURCE_FASTCALL __fastcall
#elif defined COMPILER_GCC || defined COMPILER_CLANG
#define SOURCE_STDCALL __attribute__((stdcall))
#define SOURCE_FASTCALL __attribute__((fastcall))
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add calling conventions support for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

// Export C functions for external declarations that call the appropriate C++
// methods.
#ifndef EXPORT
#ifdef COMPILER_MSVC
#define EXPORT __declspec(dllexport)
#elif defined COMPILER_GCC || defined COMPILER_CLANG
#define EXPORT __attribute__((visibility("default")))
#endif  // COMPILER_MSVC
#endif  // EXPORT

// Linux had a few areas where it didn't construct objects in the same order
// that Windows does.  So when CVProfile::CVProfile() would access g_pMemAlloc,
// it would crash because the allocator wasn't initalized yet.
#if defined COMPILER_GCC || defined COMPILER_CLANG
#define CONSTRUCT_EARLY __attribute__((init_priority(101)))
#else  // !(COMPILER_GCC || COMPILER_CLANG)
#define CONSTRUCT_EARLY
#endif  // COMPILER_GCC || COMPILER_CLANG

#ifdef COMPILER_MSVC
// Used to step into the debugger.
#define DebuggerBreak() __debugbreak()
#elif defined COMPILER_GCC || defined COMPILER_CLANG
// Used to step into the debugger.
#define DebuggerBreak() __asm__ volatile("int $0x03")
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add debugger breakpoint support for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

#ifdef COMPILER_MSVC
// Used for dll exporting and importing.
#define SOURCE_API_EXPORT extern "C" __declspec(dllexport)
#define SOURCE_API_IMPORT extern "C" __declspec(dllimport)

// Can't use extern "C" when DLL exporting a global.
#define SOURCE_API_GLOBAL_EXPORT extern __declspec(dllexport)
#define SOURCE_API_GLOBAL_IMPORT extern __declspec(dllimport)

// Can't use extern "C" when DLL exporting a class.
#define SOURCE_API_CLASS_EXPORT __declspec(dllexport)
#define SOURCE_API_CLASS_IMPORT __declspec(dllimport)
#elif defined COMPILER_GCC || defined COMPILER_CLANG
// Used for shared library exporting and importing.
#define SOURCE_API_EXPORT extern "C" __attribute__((visibility("default")))
#define SOURCE_API_IMPORT extern "C"

// Can't use extern "C" when shared library exporting a global.
#define SOURCE_API_GLOBAL_EXPORT extern __attribute__((visibility("default")))
#define SOURCE_API_GLOBAL_IMPORT extern

// Can't use extern "C" when shared library exporting a class.
#define SOURCE_API_CLASS_EXPORT __attribute__((visibility("default")))
#define SOURCE_API_CLASS_IMPORT
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add support of symbols export/import for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

// Pass hints to the compiler to prevent it from generating unnessecary / stupid
// code in certain situations.  Several compilers other than MSVC also have an
// equivilent construct.
//
// Essentially the 'the_hint' is that the condition specified is assumed to be
// true at that point in the compilation.  If '0' is passed, then the compiler
// assumes that any subsequent code in the same 'basic block' is unreachable,
// and thus usually removed.
#ifdef COMPILER_MSVC
#define SOURCE_HINT(the_hint) __assume((the_hint))
#else  // !COMPILER_MSVC
#define SOURCE_HINT(the_hint) 0
#endif  // COMPILER_MSVC

// Allow compiler to predict branches. See
// https://sourceware.org/glibc/wiki/Style_and_Conventions#Branch_Prediction
#ifdef COMPILER_MSVC
#define SOURCE_LIKELY(cond) cond
#define SOURCE_UNLIKELY(cond) cond
#elif defined COMPILER_GCC || defined COMPILER_CLANG
#define SOURCE_LIKELY(cond) __glibc_likely(cond)
#define SOURCE_UNLIKELY(cond) __glibc_unlikely(cond)
#else  // !(COMPILER_MSVC || COMPILER_GCC || COMPILER_CLANG)
#error Please, add support of branch prediction for your compiler in base/include/compiler_specific.h
#endif  // COMPILER_MSVC

#endif  // BASE_INCLUDE_COMPILER_SPECIFIC_H_
