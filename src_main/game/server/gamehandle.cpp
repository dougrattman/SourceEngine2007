// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: returns the module handle of the game dll
//			this is in its own file to protect it from tier0 PROTECTED_THINGS



#if defined(_WIN32)
#include "base/include/windows/windows_light.h"
extern HMODULE win32DLLHandle;
#elif defined(OS_POSIX)
#include <cstdio>
#include "tier0/include/dbg.h"
#endif

 
#include "tier0/include/memdbgon.h"

void *GetGameModuleHandle()
{
#if defined(_WIN32)
	return (void *)win32DLLHandle;
#elif defined(OS_POSIX)
	Assert(0);
	return NULL; // NOT implemented
#else
#error "GetGameModuleHandle() needs to be implemented"
#endif
}

