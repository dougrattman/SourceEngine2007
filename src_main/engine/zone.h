// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_ENGINE_ZONE_H_
#define SOURCE_ENGINE_ZONE_H_

#include "base/include/base_types.h"

void Memory_Init();
void Memory_Shutdown();

void* Hunk_Alloc(usize size, bool is_null = true);
void* Hunk_AllocName(usize size, const char* name, bool is_null = true);

uintptr_t Hunk_LowMark();
void Hunk_FreeToLowMark(uintptr_t mark);

void Hunk_Check();

usize Hunk_MallocSize();
usize Hunk_Size();

void Hunk_Print();

template <typename T>
class CHunkMemory {
 public:
  // constructor, destructor
  CHunkMemory(int nGrowSize = 0, int nInitSize = 0) {
    m_pMemory = NULL;
    m_nAllocated = 0;
    if (nInitSize) Grow(nInitSize);
  }
  CHunkMemory(T* pMemory, int numElements) { Assert(0); }

  // Can we use this index?
  bool IsIdxValid(int i) const { return (i >= 0) && (i < m_nAllocated); }

  // Gets the base address
  T* Base() { return (T*)m_pMemory; }
  const T* Base() const { return (T*)m_pMemory; }

  // element access
  T& operator[](int i) {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  const T& operator[](int i) const {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  T& Element(int i) {
    Assert(IsIdxValid(i));
    return Base()[i];
  }
  const T& Element(int i) const {
    Assert(IsIdxValid(i));
    return Base()[i];
  }

  // Attaches the buffer to external memory....
  void SetExternalBuffer(T* pMemory, int numElements) { Assert(0); }

  // Size
  int NumAllocated() const { return m_nAllocated; }
  int Count() const { return m_nAllocated; }

  // Grows the memory, so that at least allocated + num elements are allocated
  void Grow(int num = 1) {
    Assert(!m_nAllocated);
    m_pMemory = (T*)Hunk_Alloc(num * sizeof(T), false);
    m_nAllocated = num;
  }

  // Makes sure we've got at least this much memory
  void EnsureCapacity(int num) { Assert(num <= m_nAllocated); }

  // Memory deallocation
  void Purge() { m_nAllocated = 0; }

  // Purge all but the given number of elements (NOT IMPLEMENTED IN )
  void Purge(int numElements) { Assert(0); }

  // is the memory externally allocated?
  bool IsExternallyAllocated() const { return false; }

  // Set the size by which the memory grows
  void SetGrowSize(int size) {}

 private:
  T* m_pMemory;
  int m_nAllocated;
};

#endif  // SOURCE_ENGINE_ZONE_H_
