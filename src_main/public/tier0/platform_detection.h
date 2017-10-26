// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_PLATFORM_DETECTION_H_
#define SOURCE_TIER0_PLATFORM_DETECTION_H_

#ifdef _RETAIL
#define IsRetail() true
#else
#define IsRetail() false
#endif

#ifdef _DEBUG
#define IsRelease() false
#define IsDebug() true
#else
#define IsRelease() true
#define IsDebug() false
#endif

// Deprecating, infavor of IsX360() which will revert to IsXbox()
// after confidence of xbox 1 code flush
#define IsXbox() false

#ifdef _WIN32

#define IsLinux() false
#define IsPC() true
#define IsConsole() false
#define IsX360() false
#define IsPS3() false
#define IS_WINDOWS_PC

#elif defined(_LINUX)

#define IsPC() true
#define IsConsole() false
#define IsX360() false
#define IsPS3() false
#define IsLinux() true

#else

#error Unknown platform.

#endif

#endif  // SOURCE_TIER0_PLATFORM_DETECTION_H_
