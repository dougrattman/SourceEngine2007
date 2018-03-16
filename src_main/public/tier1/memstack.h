// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: A fast stack memory allocator that uses virtual memory if available

#ifndef MEMSTACK_H
#define MEMSTACK_H

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

using MemoryStackMark_t = uintptr_t;

class CMemoryStack {
 public:
  CMemoryStack();
  ~CMemoryStack();

  bool Init(usize maxSize = 0, usize commitSize = 0, usize initialCommit = 0,
            usize alignment = 16);
  void Term();

  usize GetSize() const {
#ifdef OS_WIN
    return commit_limit_ - base_;
#else
    return m_maxSize;
#endif
  }
  usize GetMaxSize() const { return max_size_; }
  usize GetUsed() const { return next_alloc_ - base_; }

  void *SOURCE_RESTRICT Alloc(usize bytes, bool is_clear = false) {
    Assert(base_);

    const usize alignment{alignment_};

    bytes = bytes ? AlignValue(bytes, alignment) : alignment;

    void *result = next_alloc_;
    u8 *next_alloc = next_alloc_ + bytes;

    if (next_alloc <= commit_limit_ || CommitTo(next_alloc)) {
      if (is_clear) memset(result, 0, bytes);

      next_alloc_ = next_alloc;
      return result;
    }

    return nullptr;
  }

  MemoryStackMark_t GetCurrentAllocPoint() const { return next_alloc_ - base_; }

  void FreeToAllocPoint(MemoryStackMark_t mark, bool bDecommit = true);
  void FreeAll(bool bDecommit = true);

  void Access(void **ppRegion, uintptr_t *pBytes) const;

  void PrintContents() const;

  void *GetBase() { return base_; }
  const void *GetBase() const {
    return const_cast<CMemoryStack *>(this)->GetBase();
  }

 private:
  bool CommitTo(u8 *SOURCE_RESTRICT);

  u8 *next_alloc_;
  u8 *commit_limit_, *alloc_limit_;
  u8 *base_;

  usize max_size_, alignment_;

#ifdef OS_WIN
  usize commit_size_, min_commit_;
#endif
};

// The CUtlMemoryStack class:
// A fixed memory class
template <typename T, typename I, usize MAX_SIZE, usize COMMIT_SIZE = 0,
          usize INITIAL_COMMIT = 0>
class CUtlMemoryStack {
 public:
  CUtlMemoryStack(int nGrowSize = 0, int nInitSize = 0) : m_nAllocated{0} {
    m_MemoryStack.Init(MAX_SIZE * sizeof(T), COMMIT_SIZE * sizeof(T),
                       INITIAL_COMMIT * sizeof(T), 4);
    static_assert(sizeof(T) % 4 == 0);
  }

  // Can we use this index?
  bool IsIdxValid(I i) const { return (i >= 0) && (i < m_nAllocated); }
  static int InvalidIndex() { return -1; }

  class Iterator_t {
    Iterator_t(I i) : index(i) {}
    I index;
    friend class CUtlMemoryStack<T, I, MAX_SIZE, COMMIT_SIZE, INITIAL_COMMIT>;

   public:
    bool operator==(const Iterator_t it) const {  //-V801
      return index == it.index;
    }
    bool operator!=(const Iterator_t it) const {  //-V801
      return index != it.index;
    }
  };
  Iterator_t First() const {
    return Iterator_t(m_nAllocated ? 0 : InvalidIndex());
  }
  Iterator_t Next(const Iterator_t &it) const {
    return Iterator_t(it.index < m_nAllocated ? it.index + 1 : InvalidIndex());
  }
  I GetIndex(const Iterator_t &it) const { return it.index; }
  bool IsIdxAfter(I i, const Iterator_t &it) const { return i > it.index; }
  bool IsValidIterator(const Iterator_t &it) const {
    return it.index >= 0 && it.index < m_nAllocated;
  }
  Iterator_t InvalidIterator() const { return Iterator_t(InvalidIndex()); }

  // Gets the base address
  T *Base() { return (T *)m_MemoryStack.GetBase(); }
  const T *Base() const { return (const T *)m_MemoryStack.GetBase(); }

  // element access
  T &operator[](I i) {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  const T &operator[](I i) const {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  T &Element(I i) {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  const T &Element(I i) const {
    Assert(IsIdxValid(i));
    return Base()[i];
  }

  // Attaches the buffer to external memory....
  void SetExternalBuffer(T *pMemory, int numElements) { Assert(0); }

  // Size
  int NumAllocated() const { return m_nAllocated; }
  int Count() const { return m_nAllocated; }

  // Grows the memory, so that at least allocated + num elements are allocated
  void Grow(int num = 1) {
    Assert(num > 0);
    m_nAllocated += num;
    m_MemoryStack.Alloc(num * sizeof(T));
  }

  // Makes sure we've got at least this much memory
  void EnsureCapacity(int num) {
    Assert(num <= MAX_SIZE);
    if (m_nAllocated < num) Grow(num - m_nAllocated);
  }

  // Memory deallocation
  void Purge() {
    m_MemoryStack.FreeAll();
    m_nAllocated = 0;
  }

  // is the memory externally allocated?
  bool IsExternallyAllocated() const { return false; }

  // Set the size by which the memory grows
  void SetGrowSize(int size) {}

 private:
  CMemoryStack m_MemoryStack;
  int m_nAllocated;
};

#endif  // MEMSTACK_H
