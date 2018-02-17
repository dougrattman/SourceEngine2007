// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "vallocator.h"
#include <malloc.h>
#include "tier0/include/basetypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

VStdAllocator g_StdAllocator;

void *VStdAllocator::Alloc(unsigned long size) {
  if (size) {
    void *ret = malloc(size);
    return ret;
  } else
    return 0;
}

void VStdAllocator::Free(void *ptr) { free(ptr); }

#endif  // !_STATIC_LINKED || _SHARED_LIB
