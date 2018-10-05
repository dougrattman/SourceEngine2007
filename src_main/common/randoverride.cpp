// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include <cstdlib>
#include "vstdlib/random.h"

#include "tier0/include/memdbgon.h"

#ifdef OS_POSIX
#define __cdecl
#endif

void __cdecl srand(unsigned int) {}
int __cdecl rand() { return RandomInt(0, 0x7fff); }

#endif  // !_STATIC_LINKED || _SHARED_LIB
