// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_WINDOWS_WINDOWS_LIGHT_H_
#define BASE_INCLUDE_WINDOWS_WINDOWS_LIGHT_H_

// Prevent tons of unused windows definitions.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Strict types mode.
#ifndef STRICT
#define STRICT
#endif

// Nobody needs it.
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOMINMAX

// Windows 10 features.
#define _WIN32_WINNT 0x0A00

#include "base/include/compiler_specific.h"

MSVC_BEGIN_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE()
#include <Windows.h>

#include <Versionhelpers.h>
MSVC_END_TURN_OFF_DEFAULT_WARNING_OVERRIDE_SCOPE()

#undef Yield

#ifndef USE_WINDOWS_POSTMESSAGE
#undef PostMessage
#endif

#endif  // BASE_INCLUDE_WINDOWS_WINDOWS_LIGHT_H_
