// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "filememcache.h"

#include <algorithm>

namespace {
unsigned long kCachedFileSignature = 0xCACEF11E;
};

// Cached file data implementation

CachedFileData *CachedFileData::Create(char const *szFilename) {
  FILE *f;
  int ok = !fopen_s(&f, szFilename, "rb");
  int nSize = -1;

  if (ok) {
    fseek(f, 0, SEEK_END);
    nSize = ftell(f);
    fseek(f, 0, SEEK_SET);
  }

  CachedFileData *pData =
      (CachedFileData *)malloc(eHeaderSize + std::max(nSize, 0));
  strcpy_s(pData->m_chFilename, szFilename);
  pData->m_numRefs = 0;
  pData->m_numDataBytes = nSize;
  pData->m_signature = kCachedFileSignature;

  if (ok) {
    fread(pData->m_data, 1, nSize, f);
    fclose(f);
  }

  return pData;
}

void CachedFileData::Free(void) { free(this); }

CachedFileData *CachedFileData::GetByDataPtr(void const *pvDataPtr) {
  unsigned char const *pbBuffer =
      reinterpret_cast<unsigned char const *>(pvDataPtr);
  // Assert( pbBuffer );

  CachedFileData const *pData =
      reinterpret_cast<CachedFileData const *>(pbBuffer - eHeaderSize);
  // Assert( pData->m_signature == s_ulCachedFileSignature );

  return const_cast<CachedFileData *>(pData);
}

char const *CachedFileData::GetFileName() const { return m_chFilename; }

void const *CachedFileData::GetDataPtr() const { return m_data; }

int CachedFileData::GetDataLen() const { return std::max(m_numDataBytes, 0); }

bool CachedFileData::IsValid() const { return m_numDataBytes >= 0; }

FileCache::FileCache() {}

CachedFileData *FileCache::Get(char const *szFilename) {
  // Search the cache first
  Mapping::iterator it = m_map.find(szFilename);
  if (it != m_map.end()) return it->second;

  // Create the cached file data
  CachedFileData *pData = CachedFileData::Create(szFilename);
  if (pData) m_map.insert(Mapping::value_type(pData->GetFileName(), pData));

  return pData;
}

void FileCache::Clear() {
  for (auto &it : m_map) {
    it.second->Free();
  }

  m_map.clear();
}
