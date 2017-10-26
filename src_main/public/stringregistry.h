// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose:	A registry of strings and associated ints

#ifndef STRINGREGISTRY_H
#define STRINGREGISTRY_H

struct StringTable_t;

//-----------------------------------------------------------------------------
// Purpose: Just a convenience/legacy wrapper for CUtlDict<> .
//-----------------------------------------------------------------------------
class CStringRegistry {
 private:
  StringTable_t *m_pStringList;

 public:
  // returns a key for a given string
  unsigned short AddString(const char *stringText, int stringID);

  // This is optimized.  It will do 2 O(logN) searches
  // Only one of the searches needs to compare strings, the other compares
  // symbols (ints) returns -1 if the string is not present in the registry.
  int GetStringID(const char *stringText);

  // This is unoptimized.  It will linearly search (but only compares ints, not
  // strings)
  const char *GetStringText(int stringID);

  // This is O(1).  It will not search.  key MUST be a value that was returned
  // by AddString
  const char *GetStringForKey(unsigned short key);
  // This is O(1).  It will not search.  key MUST be a value that was returned
  // by AddString
  int GetIDForKey(unsigned short key);

  void ClearStrings();

  // Iterate all the keys.
  unsigned short First() const;
  unsigned short Next(unsigned short key) const;
  unsigned short InvalidIndex() const;

  ~CStringRegistry();  // Need to free allocated memory
  CStringRegistry();
};

#endif  // STRINGREGISTRY_H
