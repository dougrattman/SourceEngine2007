// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_COMPILER_SPECIFIC_MACROSES_H_
#define SOURCE_TIER0_INCLUDE_COMPILER_SPECIFIC_MACROSES_H_

#include "build/include/build_config.h"

// This can be used to ensure the size of pointers to members when declaring
// a pointer type for a class that has only been forward declared
#ifdef COMPILER_MSVC
#define SINGLE_INHERITANCE __single_inheritance
#define MULTIPLE_INHERITANCE __multiple_inheritance
#define NO_VTABLE __declspec(novtable)
#else
#define SINGLE_INHERITANCE
#define MULTIPLE_INHERITANCE
#define NO_VTABLE
#endif  // COMPILER_MSVC

#define abstract_class class NO_VTABLE

// Macroses to disable compiler warnings in scope.
#ifdef COMPILER_MSVC
#define MSVC_BEGIN_WARNING_OVERRIDE_SCOPE() __pragma(warning(push))
#define MSVC_DISABLE_WARNING(warning_level) \
  __pragma(warning(disable : warning_level))
#define MSVC_END_WARNING_OVERRIDE_SCOPE() __pragma(warning(pop))

#define MSVC_SCOPED_DISABLE_WARNING(warning_level, scope) \
                                                          \
  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()                     \
                                                          \
  MSVC_DISABLE_WARNING(warning_level)                     \
                                                          \
  scope MSVC_END_WARNING_OVERRIDE_SCOPE()
#else
#define MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
#define MSVC_DISABLE_WARNING(warning_level)
#define MSVC_END_WARNING_OVERRIDE_SCOPE()

#define MSVC_SCOPED_DISABLE_WARNING(warning_level, scope) scope
#endif  // COMPILER_MSVC

#endif  // SOURCE_TIER0_INCLUDE_COMPILER_SPECIFIC_MACROSES_H_
