// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef LAUNCHER_SCOPED_MEMORY_LEAK_DUMPER_
#define LAUNCHER_SCOPED_MEMORY_LEAK_DUMPER_

#include "tier0/dbg.h"
#include "tier0/memalloc.h"

class ScopedMemoryLeakDumper {
 public:
  ScopedMemoryLeakDumper(IMemAlloc *const memory_allocator,
                         bool should_dump_memory_leaks)
      : memory_allocator_{memory_allocator},
        should_dump_memory_leaks_{should_dump_memory_leaks} {
    Assert(memory_allocator);
  }

  ~ScopedMemoryLeakDumper() {
    if (should_dump_memory_leaks_) {
      memory_allocator_->DumpStats();
    }
  }

 private:
  IMemAlloc *const memory_allocator_;
  const bool should_dump_memory_leaks_;

  ScopedMemoryLeakDumper(const ScopedMemoryLeakDumper &s) = delete;
  ScopedMemoryLeakDumper &operator=(const ScopedMemoryLeakDumper &s) = delete;
};

#endif  // LAUNCHER_SCOPED_MEMORY_LEAK_DUMPER_
