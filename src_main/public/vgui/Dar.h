// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Holds the enumerated list of default cursors

#ifndef SOURCE_VGUI_DAR_H_
#define SOURCE_VGUI_DAR_H_

#ifdef _WIN32
#pragma once
#endif

#include <cstdlib>
#include <cstring>
#include "tier1/utlvector.h"
#include "vgui/VGUI.h"

#include "tier0/include/memdbgon.h"

namespace vgui {
// Purpose: Simple lightweight dynamic array implementation
template <typename ELEMTYPE>
class Dar : public CUtlVector<ELEMTYPE> {
  using BaseClass = CUtlVector<ELEMTYPE>;

 public:
  Dar() {}
  Dar(int initialCapacity) : BaseClass(0, initialCapacity) {}

 public:
  void SetCount(int count) { this->EnsureCount(count); }

  int GetCount() const { return this->Count(); }

  int AddElement(ELEMTYPE elem) { return this->AddToTail(elem); }

  void MoveElementToEnd(ELEMTYPE elem) {
    if (this->Count() == 0) return;

    // quick check to see if it's already at the end
    if (this->Element(this->Count() - 1) == elem) return;

    int idx = this->Find(elem);
    if (idx == this->InvalidIndex()) return;

    this->Remove(idx);
    this->AddToTail(elem);
  }

  // returns the index of the element in the array, -1 if not found
  int FindElement(ELEMTYPE elem) const { return this->Find(elem); }

  bool HasElement(ELEMTYPE elem) const {
    if (FindElement(elem) != this->InvalidIndex()) {
      return true;
    }
    return false;
  }

  int PutElement(ELEMTYPE elem) {
    int index = this->FindElement(elem);
    if (index >= 0) {
      return index;
    }
    return this->AddElement(elem);
  }

  // insert element at index and move all the others down 1
  void InsertElementAt(ELEMTYPE elem, int index) {
    this->InsertBefore(index, elem);
  }

  void SetElementAt(ELEMTYPE elem, int index) {
    this->EnsureCount(index + 1);
    this->Element(index) = elem;
  }

  void RemoveElementAt(int index) { this->Remove(index); }

  void RemoveElementsBefore(int index) {
    if (index <= 0) return;
    this->RemoveMultiple(0, index - 1);
  }

  void RemoveElement(ELEMTYPE elem) { this->FindAndRemove(elem); }

  void *GetBaseData() { return this->Base(); }

  void CopyFrom(Dar<ELEMTYPE> &dar) {
    this->CopyArray(dar.Base(), dar.Count());
  }
};
}  // namespace vgui

#include "tier0/include/memdbgoff.h"

#endif  // SOURCE_VGUI_DAR_H_
