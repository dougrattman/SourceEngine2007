// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Memory allocation!

#include "pch_tier0.h"

#include "tier0/mem.h"

#include <malloc.h>
#include <limits>
#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvExpand _expand
#endif

#undef max

namespace {
static constexpr size_t MAX_STACK_DEPTH = 64;
static uint8_t *s_stack = nullptr;
static size_t s_stack_depth[MAX_STACK_DEPTH];
static size_t s_depth = std::numeric_limits<size_t>::max();
static size_t s_stack_size = 0;
static size_t s_stack_alloc_size = 0;
}  // namespace

// Other DLL-exported methods for particular kinds of memory.
void *MemAllocScratch(size_t memory_size) {
  // Minimally allocate 1M scratch.
  if (s_stack_alloc_size < s_stack_size + memory_size) {
    s_stack_alloc_size = s_stack_size + memory_size;
    if (s_stack_alloc_size < 1024 * 1024) {
      s_stack_alloc_size = 1024 * 1024;
    }

    if (s_stack) {
      auto *new_stack = (uint8_t *)PvRealloc(s_stack, s_stack_alloc_size);
      if (!new_stack) {
        Assert(0);
        return nullptr;
      }
      s_stack = new_stack;
    } else {
      s_stack = (uint8_t *)PvAlloc(s_stack_alloc_size);
    }
  }

  if (s_stack == nullptr) return nullptr;

  const size_t base_stack_size = s_stack_size;
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

#ifdef _LINUX
void ZeroMemory(void *mem, size_t length) { memset(mem, 0x0, length); }
#endif
