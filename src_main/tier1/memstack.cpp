// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/memstack.h"

#include "build/include/build_config.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"

#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#endif

#include "base/include/compiler_specific.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier1/utlmap.h"

#include "tier0/include/memdbgon.h"

#ifdef OS_WIN
MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
MSVC_DISABLE_WARNING(4073)
#pragma init_seg(lib)
MSVC_END_WARNING_OVERRIDE_SCOPE()
#endif

MEMALLOC_DEFINE_EXTERNAL_TRACKING(CMemoryStack);

CMemoryStack::CMemoryStack()
    : base_(nullptr),
      next_alloc_(nullptr),
      alloc_limit_(nullptr),
      commit_limit_(nullptr),
      alignment_(16),
#ifdef OS_WIN
      commit_size_(0),
      min_commit_(0),
#endif
      max_size_(0) {
}

CMemoryStack::~CMemoryStack() {
  if (base_) Term();
}

bool CMemoryStack::Init(usize maxSize, usize commitSize, usize initialCommit,
                        usize alignment) {
  Assert(!base_);

  max_size_ = maxSize;
  alignment_ = AlignValue(alignment, 4);

  Assert(alignment_ == alignment);
  Assert(max_size_ > 0);

#ifdef OS_WIN
  if (commitSize != 0) {
    commit_size_ = commitSize;
  }

  SYSTEM_INFO system_info;
  GetNativeSystemInfo(&system_info);
  Assert(!(system_info.dwPageSize & (system_info.dwPageSize - 1)));
  const u32 pageSize = system_info.dwPageSize;

  commit_size_ =
      commit_size_ == 0 ? pageSize : AlignValue(commit_size_, pageSize);
  max_size_ = AlignValue(max_size_, commit_size_);

  Assert(max_size_ % pageSize == 0 && commit_size_ % pageSize == 0 &&
         commit_size_ <= max_size_);

  base_ =
      (u8 *)VirtualAlloc(nullptr, max_size_, VA_RESERVE_FLAGS, PAGE_NOACCESS);
  Assert(base_);

  commit_limit_ = next_alloc_ = base_;

  if (initialCommit) {
    initialCommit = AlignValue(initialCommit, commit_size_);
    Assert(initialCommit < max_size_);

    if (!VirtualAlloc(commit_limit_, initialCommit, VA_COMMIT_FLAGS,
                      PAGE_READWRITE))
      return false;

    min_commit_ = initialCommit;
    commit_limit_ += initialCommit;

    MemAlloc_RegisterExternalAllocation(CMemoryStack, GetBase(), GetSize());
  }
#else
  base_ = MemAlloc_AllocAligned(max_size_, alignment ? alignment : 1);
  next_alloc_ = base_;
  commit_limit_ = base_ + max_size_;
#endif

  alloc_limit_ = base_ + max_size_;

  return base_ != nullptr;
}

void CMemoryStack::Term() {
  FreeAll();

  if (base_) {
#ifdef OS_WIN
    VirtualFree(base_, 0, MEM_RELEASE);
#else
    MemAlloc_FreeAligned(base_);
#endif
    base_ = nullptr;
  }
}

bool CMemoryStack::CommitTo(uint8_t *SOURCE_RESTRICT pNextAlloc) {
#ifdef OS_WIN
  u8 *pNewCommitLimit = AlignValue(pNextAlloc, commit_size_);
  uintptr_t commitSize = pNewCommitLimit - commit_limit_;

  if (GetSize())
    MemAlloc_RegisterExternalDeallocation(CMemoryStack, GetBase(), GetSize());

  if (commit_limit_ + commitSize > alloc_limit_) return false;

  if (!VirtualAlloc(commit_limit_, commitSize, VA_COMMIT_FLAGS,
                    PAGE_READWRITE)) {
    Assert(0);
    return false;
  }

  commit_limit_ = pNewCommitLimit;

  if (GetSize())
    MemAlloc_RegisterExternalAllocation(CMemoryStack, GetBase(), GetSize());

  return true;
#else
  Assert(0);
  return false;
#endif
}

void CMemoryStack::FreeToAllocPoint(MemoryStackMark_t mark, bool bDecommit) {
  void *pAllocPoint = base_ + mark;
  Assert(pAllocPoint >= base_ && pAllocPoint <= next_alloc_);

  if (pAllocPoint >= base_ && pAllocPoint < next_alloc_) {
    if (bDecommit) {
#ifdef OS_WIN
      u8 *pDecommitPoint = AlignValue((u8 *)pAllocPoint, commit_size_);

      if (pDecommitPoint < base_ + min_commit_) {
        pDecommitPoint = base_ + min_commit_;
      }

      uintptr_t decommitSize = commit_limit_ - pDecommitPoint;

      if (decommitSize > 0) {
        MemAlloc_RegisterExternalDeallocation(CMemoryStack, GetBase(),
                                              GetSize());

        VirtualFree(pDecommitPoint, decommitSize, MEM_DECOMMIT);
        commit_limit_ = pDecommitPoint;

        if (mark > 0) {
          MemAlloc_RegisterExternalAllocation(CMemoryStack, GetBase(),
                                              GetSize());
        }
      }
#endif
    }

    next_alloc_ = (u8 *)pAllocPoint;
  }
}

void CMemoryStack::FreeAll(bool bDecommit) {
  if (base_ && commit_limit_ - base_ > 0) {
    if (bDecommit) {
#ifdef OS_WIN
      MemAlloc_RegisterExternalDeallocation(CMemoryStack, GetBase(), GetSize());

      VirtualFree(base_, commit_limit_ - base_, MEM_DECOMMIT);
      commit_limit_ = base_;
#endif
    }
    next_alloc_ = base_;
  }
}

void CMemoryStack::Access(void **ppRegion, uintptr_t *pBytes) const {
  *ppRegion = base_;
  *pBytes = next_alloc_ - base_;
}

void CMemoryStack::PrintContents() const {
  Msg("Total used memory:      %zu bytes.\n", GetUsed());
  Msg("Total committed memory: %zu bytes.\n", GetSize());
}
