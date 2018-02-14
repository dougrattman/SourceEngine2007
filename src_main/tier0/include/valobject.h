// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:	CValObject is used for tracking individual objects that report
// in to CValidator.  Whenever a new object reports in (via CValidator::Push),
// we create a new CValObject to aggregate stats for it.

#ifndef SOURCE_TIER0_INCLUDE_VALOBJECT_H_
#define SOURCE_TIER0_INCLUDE_VALOBJECT_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#ifdef DBGFLAG_VALIDATE
class CValObject {
 public:
  // Constructors & destructors
  CValObject(void){};
  ~CValObject();

  void Init(ch *pchType, void *pvObj, ch *pchName, CValObject *pValObjectParent,
            CValObject *pValObjectPrev);

  // Our object has claimed ownership of a memory block
  void ClaimMemoryBlock(void *pvMem);

  // A child of ours has claimed ownership of a memory block
  void ClaimChildMemoryBlock(i32 cubUser);

  // Accessors
  ch *PchType() { return m_rgchType; };
  void *PvObj() { return m_pvObj; };
  ch *PchName() { return m_rgchName; };
  CValObject *PValObjectParent() { return m_pValObjectParent; };
  i32 NLevel() { return m_nLevel; };
  CValObject *PValObjectNext() { return m_pValObjectNext; };
  i32 CpubMemSelf() { return m_cpubMemSelf; };
  i32 CubMemSelf() { return m_cubMemSelf; };
  i32 CpubMemTree() { return m_cpubMemTree; };
  i32 CubMemTree() { return m_cubMemTree; };
  i32 NUser() { return m_nUser; };
  void SetNUser(i32 nUser) { m_nUser = nUser; };
  void SetBNewSinceSnapshot(bool bNewSinceSnapshot) {
    m_bNewSinceSnapshot = bNewSinceSnapshot;
  }
  bool BNewSinceSnapshot() { return m_bNewSinceSnapshot; }

 private:
  bool m_bNewSinceSnapshot;  // If this block is new since the snapshot.
  ch m_rgchType[64];         // Type of the object we represent
  ch m_rgchName[64];         // Name of this particular object
  void *m_pvObj;             // Pointer to the object we represent

  CValObject *m_pValObjectParent;  // Our parent object in the tree.
  i32 m_nLevel;                    // Our depth in the tree

  CValObject *m_pValObjectNext;  // Next ValObject in the linked list

  i32 m_cpubMemSelf;  // # of memory blocks we own directly
  i32 m_cubMemSelf;   // Total size of the memory blocks we own directly

  i32 m_cpubMemTree;  // # of memory blocks owned by us and our children
  i32 m_cubMemTree;   // Total size of the memory blocks owned by us and our
                      // children

  i32 m_nUser;  // Field provided for use by our users
};
#endif  // DBGFLAG_VALIDATE

#endif  // SOURCE_TIER0_INCLUDE_VALOBJECT_H_
