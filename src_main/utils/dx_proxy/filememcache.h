// Copyright c 1996-2007, Valve Corporation, All rights reserved.

#ifndef FILEMEMCACHE_H
#define FILEMEMCACHE_H

#include <atomic>
#include <unordered_map>

#include "base/include/windows/windows_light.h"
#include "tier0/include/platform.h"

class CachedFileData {
  friend class FileCache;

 protected:  // Constructed by FileCache
  CachedFileData() {}
  static CachedFileData *Create(char const *szFilename);
  void Free(void);

 public:
  static CachedFileData *GetByDataPtr(void const *pvDataPtr);

  char const *GetFileName() const;
  void const *GetDataPtr() const;
  int GetDataLen() const;

  unsigned long AddRef() { return ::InterlockedIncrement(&m_numRefs); }
  unsigned long Release() { return ::InterlockedDecrement(&m_numRefs); }

  bool IsValid() const;

 protected:
  enum { eHeaderSize = 256 };
  char m_chFilename[256 - 12];
  volatile unsigned long m_numRefs;
  int m_numDataBytes;
  int m_signature;
  unsigned char m_data[0];  // file data spans further
};

class FileCache {
 public:
  FileCache();
  ~FileCache() { Clear(); }

 public:
  CachedFileData *Get(char const *szFilename);
  void Clear();

 protected:
  struct icmp {
    bool operator()(char const *x, char const *y) const {
      return _stricmp(x, y) < 0;
    }
  };

  typedef std::unordered_map<char const *, CachedFileData *,
                             stdext::hash_compare<char const *, icmp> >
      Mapping;
  Mapping m_map;
};

#endif  // #ifndef FILEMEMCACHE_H
