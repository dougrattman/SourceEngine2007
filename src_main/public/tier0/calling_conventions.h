// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_CALLING_CONVENTIONS_H_
#define SOURCE_TIER0_CALLING_CONVENTIONS_H_

#include "build/include/build_config.h"

// Used for standard calling conventions.
#ifdef COMPILER_MSVC
#define STDCALL __stdcall
#define FASTCALL __fastcall
#define FORCEINLINE __forceinline
#define FORCEINLINE_TEMPLATE __forceinline
#else
#define STDCALL
#define FASTCALL
#define FORCEINLINE inline
#define FORCEINLINE_TEMPLATE inline

#define __cdecl
#define __stdcall __attribute__((__stdcall__))
#endif  // COMPILER_MSVC

#endif  // !SOURCE_TIER0_CALLING_CONVENTIONS_H_
