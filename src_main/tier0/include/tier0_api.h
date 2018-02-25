// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_TIER0_API_H_
#define SOURCE_TIER0_INCLUDE_TIER0_API_H_

#include "build/include/build_config.h"

#ifndef TIER0_DLL_EXPORT
#ifdef OS_WIN
#define SOURCE_TIER0_API extern "C" __declspec(dllimport)
#define SOURCE_TIER0_API_OVERLOAD extern __declspec(dllimport)
#define SOURCE_TIER0_API_CLASS __declspec(dllimport)
#elif defined OS_POSIX
#define SOURCE_TIER0_API extern "C"
#define SOURCE_TIER0_API_OVERLOAD extern
#define SOURCE_TIER0_API_CLASS
#else
#error Please add support for your platform in tier0/include/tier0_api.h
#endif
#else
#ifdef OS_WIN
#define SOURCE_TIER0_API extern "C" __declspec(dllexport)
#define SOURCE_TIER0_API_OVERLOAD extern __declspec(dllexport)
#define SOURCE_TIER0_API_CLASS __declspec(dllexport)
#elif defined OS_POSIX
#define SOURCE_TIER0_API extern "C"
#define SOURCE_TIER0_API_OVERLOAD extern
#define SOURCE_TIER0_API_CLASS
#else
#error Please add support for your platform in tier0/include/tier0_api.h
#endif
#endif

#endif  // !SOURCE_TIER0_INCLUDE_TIER0_API_H_
