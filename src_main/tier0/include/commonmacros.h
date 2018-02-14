// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: This should contain ONLY general purpose macros that are appropriate
// for use in engine/launcher/all tools

#ifndef SOURCE_TIER0_INCLUDE_COMMONMACROS_H_
#define SOURCE_TIER0_INCLUDE_COMMONMACROS_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

// Makes a 4-byte "packed ID" i32 out of 4 chacters.
template <typename R = u32>
constexpr R MAKEID(const ch& d, const ch& c, const ch& b, const ch& a) {
  return (static_cast<R>(a) << 24) | (static_cast<R>(b) << 16) |
         (static_cast<R>(c) << 8) | (static_cast<R>(d));
}
// Compares a string with a 4-byte packed ID constant.
template <typename T, typename I = u32>
constexpr bool STRING_MATCHES_ID(const T& p, const I id) {
  return *reinterpret_cast<I*>(p) == id;
}
#define ID_TO_STRING(id, p)                                        \
  ((p)[3] = (((id) >> 24) & 0xFF), (p)[2] = (((id) >> 16) & 0xFF), \
   (p)[1] = (((id) >> 8) & 0xFF), (p)[0] = (((id) >> 0) & 0xFF))

#define SETBITS(iBitVector, bits) ((iBitVector) |= (bits))
#define CLEARBITS(iBitVector, bits) ((iBitVector) &= ~(bits))
#define FBitSet(iBitVector, bit) ((iBitVector) & (bit))

#define UID_PREFIX generated_id_
#define UID_CAT1(a, c) a##c
#define UID_CAT2(a, c) UID_CAT1(a, c)
#define EXPAND_CONCAT(a, c) UID_CAT1(a, c)

#ifdef COMPILER_MSVC
#define UNIQUE_ID UID_CAT2(UID_PREFIX, __COUNTER__)
#else
#define UNIQUE_ID UID_CAT2(UID_PREFIX, __LINE__)
#endif

#ifndef NOTE_UNUSED
// For pesky compiler / lint warnings.
#define NOTE_UNUSED(x) (x = x)
#endif

#define ExecuteNTimes(times, x)    \
  {                                \
    static i32 __executeCount = 0; \
    if (__executeCount < times) {  \
      x;                           \
      ++__executeCount;            \
    }                              \
  }
#define ExecuteOnce(x) ExecuteNTimes(1, x)

template <typename T>
constexpr inline bool IsPowerOfTwo(T value) {
  return (value & (value - 1)) == 0;
}

// Pad a number so it lies on an N byte boundary. So PAD_NUMBER(0,4) is 0 and
// PAD_NUMBER(1,4) is 4.
#define PAD_NUMBER(number, boundary) \
  (((number) + ((boundary)-1)) / (boundary)) * (boundary)

// Wraps the integer in quotes, allowing us to form constant strings with
// it.
#define CONST_INTEGER_AS_STRING(x) #x

//__LINE__ can only be converted to an actual number by going through this,
// otherwise the output is literally "__LINE__".
#define __HACK_LINE_AS_STRING__(x) CONST_INTEGER_AS_STRING(x)

// Gives you the line number in constant string form.
#define __LINE__AS_STRING __HACK_LINE_AS_STRING__(__LINE__)

#ifdef RTL_NUMBER_OF_V1
#undef RTL_NUMBER_OF_V1
#endif

// Return the number of elements in a statically sized array.
//   DWORD Buffer[100];
//   RTL_NUMBER_OF(Buffer) == 100
// This is also popularly known as: NUMBER_OF, ARRSIZE, _countof, NELEM, etc.
#define RTL_NUMBER_OF_V1(A) (sizeof(A) / sizeof((A)[0]))

#if defined(__cplusplus) && !defined(MIDL_PASS) && !defined(RC_INVOKED) && \
    !defined(_PREFAST_) && (_MSC_FULL_VER >= 13009466) &&                  \
    !defined(SORTPP_PASS)

// From crtdefs.h
#if !defined(UNALIGNED)
#if defined(_M_IA64) || defined(_M_AMD64)
#define UNALIGNED __unaligned
#else
#define UNALIGNED
#endif
#endif

// RtlpNumberOf is a function that takes a reference to an array of N Ts.
//
// typedef T array_of_T[N];
// typedef array_of_T &reference_to_array_of_T;
//
// RtlpNumberOf returns a pointer to an array of N chs.
// We could return a reference instead of a pointer but older compilers do not
// accept that.
//
// typedef ch array_of_ch[N];
// typedef array_of_ch *pointer_to_array_of_ch;
//
// sizeof(array_of_ch) == N
// sizeof(*pointer_to_array_of_ch) == N
//
// pointer_to_array_of_ch RtlpNumberOf(reference_to_array_of_T);
//
// We never even call RtlpNumberOf, we just take the size of dereferencing its
// return type. We do not even implement RtlpNumberOf, we just decare it.
//
// Attempts to pass pointers instead of arrays to this macro result in compile
// time errors. That is the point.

// Templates cannot be declared to have 'C' linkage
extern "C++" template <typename T, usize N>
ch (*RtlpNumberOf(UNALIGNED T (&)[N]))[N];

#ifdef RTL_NUMBER_OF_V2
#undef RTL_NUMBER_OF_V2
#endif

#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)))

// This does not work with:
//
// void Foo()
// {
//    struct { i32 x; } y[2];
//    RTL_NUMBER_OF_V2(y); // illegal use of anonymous local type in template
//    instantiation
// }
//
// You must instead do:
//
// struct Foo1 { i32 x; };
//
// void Foo()
// {
//    Foo1 y[2];
//    RTL_NUMBER_OF_V2(y); // ok
// }
//
// OR
//
// void Foo()
// {
//    struct { i32 x; } y[2];
//    RTL_NUMBER_OF_V1(y); // ok
// }
//
// OR
//
// void Foo()
// {
//    struct { i32 x; } y[2];
//    _ARRAYSIZE(y); // ok
// }

#else
#define RTL_NUMBER_OF_V2(A) RTL_NUMBER_OF_V1(A)
#endif

// Using ARRAYSIZE implementation from winnt.h:
#ifdef ARRAYSIZE
#undef ARRAYSIZE
#endif

// ARRAYSIZE is more readable version of RTL_NUMBER_OF_V2
// _ARRAYSIZE is a version useful for anonymous types.
#define ARRAYSIZE(A) RTL_NUMBER_OF_V2(A)

#ifdef _ARRAYSIZE
#undef _ARRAYSIZE
#endif

#define _ARRAYSIZE(A) RTL_NUMBER_OF_V1(A)

#endif  // SOURCE_TIER0_INCLUDE_COMMONMACROS_H_
