// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_INCLUDE_PLATFORM_DETECTION_H_
#define SOURCE_TIER0_INCLUDE_PLATFORM_DETECTION_H_

#include "build/include/build_config.h"

#ifdef _RETAIL
#define IsRetail() true
#else
#define IsRetail() false
#endif

#ifndef NDEBUG
#define IsRelease() false
#define IsDebug() true
#else
#define IsRelease() true
#define IsDebug() false
#endif

#ifdef OS_WIN
#define IsLinux() false
#define IsPC() true
#define IsConsole() false
#define IsX360() false
#elif defined(OS_POSIX)
#define IsPC() true
#define IsConsole() false
#define IsX360() false
#define IsLinux() true
#else
#error Please, define your platform in tier0/include/platform_detection.h
#endif

#endif  // SOURCE_TIER0_INCLUDE_PLATFORM_DETECTION_H_
