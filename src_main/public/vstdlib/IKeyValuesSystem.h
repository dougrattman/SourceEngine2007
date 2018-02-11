// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_
#define SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_

#include "base/include/base_types.h"
#include "tier1/interface.h"
#include "vstdlib/vstdlib.h"

// handle to a KeyValues key name symbol
using HKeySymbol = i32;
#define INVALID_KEY_SYMBOL (-1)

// Purpose: Interface to shared data repository for KeyValues (included in
// vgui_controls.lib) allows for central data storage point of KeyValues symbol
// table.
abstract_class IKeyValuesSystem {
 public:
  // Registers the size of the KeyValues in the specified instance
  // so it can build a properly sized memory pool for the KeyValues objects
  // the sizes will usually never differ but this is for versioning safety
  virtual void RegisterSizeofKeyValues(i32 size) = 0;

  // Allocates/frees a KeyValues object from the shared mempool
  virtual void *AllocKeyValuesMemory(i32 size) = 0;
  virtual void FreeKeyValuesMemory(void *pMem) = 0;

  // Symbol table access (used for key names)
  virtual HKeySymbol GetSymbolForString(const ch *name,
                                        bool bCreate = true) = 0;
  virtual const ch *GetStringForSymbol(HKeySymbol symbol) = 0;

  // For debugging, adds KeyValues record into global list so we can track
  // memory leaks
  virtual void AddKeyValuesToMemoryLeakList(void *pMem, HKeySymbol name) = 0;
  virtual void RemoveKeyValuesFromMemoryLeakList(void *pMem) = 0;
};

VSTDLIB_INTERFACE IKeyValuesSystem *KeyValuesSystem();

#endif  // SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_
