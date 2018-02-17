// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Interface for the ObjectContainer class.

#ifndef IOBJECTCONTAINER_H
#define IOBJECTCONTAINER_H

#include "tier1/interface.h"

abstract_class IObjectContainer {
 public:
  virtual void Init() = 0;

  virtual bool Add(void* newObject) = 0;
  virtual bool Remove(void* object) = 0;
  virtual void Clear(bool freeElementsMemory) = 0;

  virtual void* GetFirst() = 0;
  virtual void* GetNext() = 0;

  virtual int CountElements() = 0;

  virtual bool Contains(void* object) = 0;
  virtual bool IsEmpty() = 0;
};

#endif  // IOBJECTCONTAINER_H
