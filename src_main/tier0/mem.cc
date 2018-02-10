// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#include "pch_tier0.h"

#include "tier0/include/mem.h"

#include <malloc.h>
#include <limits>
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvExpand _expand
#endif

#undef max

namespace {
constexpr usize MAX_STACK_DEPTH{64};
u8 *s_stack{nullptr};
usize s_stack_depth[MAX_STACK_DEPTH];
usize s_depth{std::numeric_limits<usize>::max()};
usize s_stack_size{0};
usize s_stack_alloc_size{0};
}  // namespace

// Other DLL-exported methods for particular kinds of memory.
void *MemAllocScratch(usize memory_size) {
  // Minimally allocate 1M scratch.
  if (s_stack_alloc_size < s_stack_size + memory_size) {
    s_stack_alloc_size = s_stack_size + memory_size;
    if (s_stack_alloc_size < 1024 * 1024) {
      s_stack_alloc_size = 1024 * 1024;
    }

    if (s_stack) {
      auto *new_stack = (u8 *)PvRealloc(s_stack, s_stack_alloc_size);
      if (!new_stack) {
        Assert(0);
        return nullptr;
      }
      s_stack = new_stack;
    } else {
      s_stack = (u8 *)PvAlloc(s_stack_alloc_size);
    }
  }

  if (s_stack == nullptr) return nullptr;

  const usize base_stack_size = s_stack_size;
  s_stack_size += memory_size;
  ++s_depth;

  Assert(s_depth < MAX_STACK_DEPTH);
  s_stack_depth[s_depth] = memory_size;

  return &s_stack[base_stack_size];
}

void MemFreeScratch() {
  s_stack_size -= s_stack_depth[s_depth];
  --s_depth;
}

#ifdef OS_POSIX
void ZeroMemory(void *mem, usize length) { memset(mem, 0x0, length); }
#endif
