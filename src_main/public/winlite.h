// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef WINLITE_H
#define WINLITE_H

// Prevent tons of unused windows definitions.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Strict types mode.
#ifndef STRICT
#define STRICT
#endif

#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME

// Windows 10 features.
#define _WIN32_WINNT 0x0A00

// Windows doesn't protect itself from redefinition, but macro is same.
#ifdef RTL_NUMBER_OF_V1
#undef RTL_NUMBER_OF_V1
#endif

#include <Windows.h>

#include <Versionhelpers.h>

#undef Yield

#ifndef USE_WINDOWS_POSTMESSAGE
#undef PostMessage
#endif

#endif  // WINLITE_H
