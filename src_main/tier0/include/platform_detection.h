// Copyright © 1996-2017, Valve Corporation, All rights reserved.

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

// Deprecating, infavor of IsX360() which will revert to IsXbox()
// after confidence of xbox 1 code flush
#define IsXbox() false

#ifdef OS_WIN

#define IsLinux() false
#define IsPC() true
#define IsConsole() false
#define IsX360() false
#define IsPS3() false
#define IS_WINDOWS_PC

#elif defined(OS_POSIX)

#define IsPC() true
#define IsConsole() false
#define IsX360() false
#define IsPS3() false
#define IsLinux() true

#else

#error Unknown platform.

#endif

#endif  // SOURCE_TIER0_INCLUDE_PLATFORM_DETECTION_H_
