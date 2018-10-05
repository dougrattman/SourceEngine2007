// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VPHYSICS_INCLUDE_OBJECT_HASH_H_
#define SOURCE_VPHYSICS_INCLUDE_OBJECT_HASH_H_

#ifdef _WIN32
#pragma once
#endif

class IPhysicsObjectPairHash {
 public:
  virtual ~IPhysicsObjectPairHash() {}

  virtual void AddObjectPair(void *pObject0, void *pObject1) = 0;
  virtual void RemoveObjectPair(void *pObject0, void *pObject1) = 0;
  virtual bool IsObjectPairInHash(void *pObject0, void *pObject1) = 0;
  virtual void RemoveAllPairsForObject(void *pObject0) = 0;
  virtual bool IsObjectInHash(void *pObject0) = 0;

  // Used to iterate over all pairs an object is part of
  virtual int GetPairCountForObject(void *pObject0) = 0;
  virtual int GetPairListForObject(void *pObject0, int nMaxCount,
                                   void **ppObjectList) = 0;
};

#endif  // SOURCE_VPHYSICS_INCLUDE_OBJECT_HASH_H_
