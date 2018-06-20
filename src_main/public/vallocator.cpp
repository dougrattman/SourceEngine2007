// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)
#include "vallocator.h"

#include <malloc.h>
#include "tier0/include/basetypes.h"
#include "tier0/include/platform.h"

#include "tier0/include/memdbgon.h"

VStdAllocator g_StdAllocator;

void *VStdAllocator::Alloc(usize size) {
  return size ? heap_alloc<u8>(size) : nullptr;
}

void VStdAllocator::Free(void *ptr) { heap_free(ptr); }

#endif  // !_STATIC_LINKED || _SHARED_LIB
