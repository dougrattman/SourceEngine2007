// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_
#define SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "tier1/interface.h"
#include "vstdlib/vstdlib.h"

// Handle to a KeyValues key name symbol.
using HKeySymbol = i32;
#define INVALID_KEY_SYMBOL (-1)

// Interface to shared data repository for KeyValues (included in
// vgui_controls.lib) allows for central data storage point of KeyValues symbol
// table.
the_interface IKeyValuesSystem {
 public:
  // Registers the size of the KeyValues in the specified instance
  // so it can build a properly sized memory pool for the KeyValues objects
  // the sizes will usually never differ but this is for versioning safety.
  virtual void RegisterSizeofKeyValues(usize size) = 0;

  // Allocates/frees a KeyValues object from the shared mempool.
  virtual void *AllocKeyValuesMemory(usize size) = 0;
  virtual void FreeKeyValuesMemory(void *memory) = 0;

  // Symbol table access (used for key names)
  virtual HKeySymbol GetSymbolForString(const ch *name,
                                        bool should_create = true) = 0;
  virtual const ch *GetStringForSymbol(HKeySymbol symbol) = 0;

  // For debugging, adds KeyValues record into global list so we can track
  // memory leaks.
  virtual void AddKeyValuesToMemoryLeakList(void *memory, HKeySymbol name) = 0;
  virtual void RemoveKeyValuesFromMemoryLeakList(void *memory) = 0;
};

VSTDLIB_INTERFACE IKeyValuesSystem *KeyValuesSystem();

#endif  // SOURCE_VSTDLIB_IKEYVALUESSYSTEM_H_
