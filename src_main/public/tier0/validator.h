// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER0_VALIDATOR_H_
#define SOURCE_TIER0_VALIDATOR_H_

#include "base/include/base_types.h"
#include "build/include/build_config.h"

#include "tier0/include/valobject.h"

#ifdef DBGFLAG_VALIDATE

class CValidator {
 public:
  // Constructors & destructors
  CValidator();
  ~CValidator();

  // Call this each time we enter a new Validate function
  void Push(ch *pchType, void *pvObj, ch *pchName);

  // Call this each time we exit a Validate function
  void Pop();

  // Claim ownership of a memory block
  void ClaimMemory(void *pvMem);

  // Finish performing a check and perform necessary computations
  void Finalize();

  // Render our results to the console
  void RenderObjects(i32 cubThreshold);  // Render all reported objects
  void RenderLeaks();                    // Render all memory leaks

  // List manipulation functions:
  CValObject *FindObject(
      void *pvObj);  // Returns CValObject containing pvObj, or nullptr.
  void DiffAgainst(CValidator *pOtherValidator);  // Removes any entries from
                                                  // this validator that are
                                                  // also present in the other.

  // Accessors
  bool BMemLeaks() { return m_bMemLeaks; };
  CValObject *PValObjectFirst() { return m_pValObjectFirst; };

  void Validate(CValidator &validator,
                ch *pchName);  // Validate our internal structures

 private:
  CValObject *m_pValObjectFirst;  // Linked list of all ValObjects
  CValObject *m_pValObjectLast;   // Last ValObject on the linked list

  CValObject *m_pValObjectCur;  // Object we're current processing

  i32 m_cpvOwned;  // Total # of blocks owned

  i32 m_cpubLeaked;  // # of leaked memory blocks
  i32 m_cubLeaked;   // Amount of leaked memory
  bool m_bMemLeaks;  // Has any memory leaked?
};

#endif  // DBGFLAG_VALIDATE

#endif  // SOURCE_TIER0_VALIDATOR_H_
