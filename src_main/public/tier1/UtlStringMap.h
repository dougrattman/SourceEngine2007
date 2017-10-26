// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER1_UTLSTRINGMAP_H_
#define SOURCE_TIER1_UTLSTRINGMAP_H_

#include "tier1/utlsymbol.h"

template <class T>
class CUtlStringMap {
 public:
  CUtlStringMap(bool is_case_insensitive = true)
      : symbol_table_{0, 32, is_case_insensitive} {}

  // Get data by the string itself:
  T &operator[](const char *the_string) {
    CUtlSymbol symbol = symbol_table_.AddString(the_string);
    int index = (int)(UtlSymId_t)symbol;
    if (vector_.Count() <= index) {
      vector_.EnsureCount(index + 1);
    }
    return vector_[index];
  }

  // Get data by the string's symbol table ID - only used to retrieve a
  // pre-existing symbol, not create a new one!
  T &operator[](UtlSymId_t n) {
    Assert(n >= 0 && n <= vector_.Count());
    return vector_[n];
  }

  const T &operator[](UtlSymId_t n) const {
    Assert(n >= 0 && n <= vector_.Count());
    return vector_[n];
  }

  bool Defined(const char *pString) const {
    return symbol_table_.Find(pString) != UTL_INVAL_SYMBOL;
  }

  UtlSymId_t Find(const char *pString) const {
    return symbol_table_.Find(pString);
  }

  static UtlSymId_t InvalidIndex() { return UTL_INVAL_SYMBOL; }

  int GetNumStrings(void) const { return symbol_table_.GetNumStrings(); }

  const char *String(int n) const { return symbol_table_.String(n); }

  // Clear all of the data from the map
  void Clear() {
    vector_.RemoveAll();
    symbol_table_.RemoveAll();
  }

  void Purge() {
    vector_.Purge();
    symbol_table_.RemoveAll();
  }

  void PurgeAndDeleteElements() {
    vector_.PurgeAndDeleteElements();
    symbol_table_.RemoveAll();
  }

 private:
  CUtlVector<T> vector_;
  CUtlSymbolTable symbol_table_;
};

#endif  // SOURCE_TIER1_UTLSTRINGMAP_H_
