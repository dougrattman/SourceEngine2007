// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks, and there will never be two
// contiguous free memblocks.
//
// The rover can be left pointing at a non-empty block
//
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.

#include "zone.h"

#include "base/include/base_types.h"
#include "datacache/idatacache.h"
#include "host.h"
#include "sys_dll.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/memalloc.h"
#include "tier1/memstack.h"
#include "tier1/strtools.h"

namespace {
CMemoryStack zone_memory_stack;

usize GetTargetCacheSize() {
  usize memory_limit = host_parms.memsize - Hunk_Size();
  if (memory_limit < 0x100000u) memory_limit = 0x100000u;

  return memory_limit;
}
}  // namespace

void *Hunk_AllocName(usize size, const char *name, bool is_clear) {
  MEM_ALLOC_CREDIT();

  void *memory = zone_memory_stack.Alloc(size, is_clear);
  if (memory) return memory;

  Error("Engine: Hunk memory allocator overflow, can't alloc %zu bytes.\n",
        size);
  return nullptr;
}

void *Hunk_Alloc(usize size, bool is_clear) {
  MEM_ALLOC_CREDIT();

  return Hunk_AllocName(size, nullptr, is_clear);
}

uintptr_t Hunk_LowMark() { return zone_memory_stack.GetCurrentAllocPoint(); }

void Hunk_FreeToLowMark(uintptr_t mark) {
  Assert(mark < zone_memory_stack.GetSize());
  zone_memory_stack.FreeToAllocPoint(mark);
}

usize Hunk_MallocSize() { return zone_memory_stack.GetSize(); }

usize Hunk_Size() { return zone_memory_stack.GetUsed(); }

void Hunk_Print() {
  Msg("Total used memory:      %5.2f MB (%zu bytes).\n",
      Hunk_Size() / (1024.0f * 1024.0f), Hunk_Size());
  Msg("Total committed memory: %5.2f MB (%zu bytes).\n",
      Hunk_MallocSize() / (1024.0f * 1024.0f), Hunk_MallocSize());
}

void Memory_Init() {
  MEM_ALLOC_CREDIT();

  usize memory_max_bytes{128 * 1024 * 1024};
  ConVarRef mem_min_heapsize{"mem_min_heapsize"};
  const usize minimum_memory_bytes = mem_min_heapsize.GetInt() * 1024 * 1024;

  constexpr usize kMemoryMinCommitBytes{32768};
  constexpr usize kMemInitialCommitBytes{4 * 1024 * 1024};

  while (!zone_memory_stack.Init(memory_max_bytes, kMemoryMinCommitBytes,
                                 kMemInitialCommitBytes)) {
    Warning(
        "Engine: Unable to allocate %zu MB of memory, trying %zu MB instead.\n",
        memory_max_bytes / (1024 * 1024), memory_max_bytes / (2 * 1024 * 1024));

    memory_max_bytes /= 2;

    if (memory_max_bytes < minimum_memory_bytes) {
      Error(
          "Engine: Failed to allocate minimum memory requirement for game (%zu "
          "MB).\n",
          minimum_memory_bytes / (1024 * 1024));
    }
  }

  g_pDataCache->SetSize(GetTargetCacheSize());
}

void Memory_Shutdown() {
  zone_memory_stack.FreeAll();

  // This disconnects the engine data cache
  g_pDataCache->SetSize(0);
}
