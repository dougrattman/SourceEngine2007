// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Cache for VCDs. PC async loads and uses the datacache to manage.
// 360 uses a baked resident image of aggregated compiled VCDs.
//
//=============================================================================

#include "choreoscene.h"
#include "filesystem.h"
#include "scenefilecache/ISceneFileCache.h"
#include "scenefilecache/SceneImageFile.h"
#include "tier1/lzmaDecoder.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"

#include "tier0/include/memdbgon.h"

IFileSystem *filesystem = NULL;

bool IsBufferBinaryVCD(char *pBuffer, int bufferSize) {
  if (bufferSize > 4 && (*(int *)pBuffer) == SCENE_BINARY_TAG) {
    return true;
  }

  return false;
}

class CSceneFileCache : public CBaseAppSystem<ISceneFileCache> {
 public:
  // IAppSystem
  virtual bool Connect(CreateInterfaceFn factory);
  virtual void Disconnect();
  virtual InitReturnVal_t Init();
  virtual void Shutdown();

  // ISceneFileCache
  // Physically reloads image from disk
  virtual void Reload();

  virtual size_t GetSceneBufferSize(char const *pFilename);
  virtual bool GetSceneData(char const *pFilename, uint8_t *buf,
                            size_t bufsize);

  // alternate resident image implementation
  virtual bool GetSceneCachedData(char const *pFilename,
                                  SceneCachedData_t *pData);
  virtual short GetSceneCachedSound(int iScene, int iSound);
  virtual const char *GetSceneString(short stringId);

 private:
  // alternate implementation - uses a resident baked image of the file cache,
  // contains all the compiled VCDs single i/o read at startup to mount the
  // image
  int FindSceneInImage(const char *pSceneName);
  bool GetSceneDataFromImage(const char *pSceneName, int iIndex, uint8_t *pData,
                             size_t *pLength);

 private:
  CUtlBuffer m_SceneImageFile;
};

bool CSceneFileCache::Connect(CreateInterfaceFn factory) {
  if ((filesystem = (IFileSystem *)factory(FILESYSTEM_INTERFACE_VERSION,
                                           NULL)) == NULL) {
    return false;
  }

  return true;
}

void CSceneFileCache::Disconnect() {}

InitReturnVal_t CSceneFileCache::Init() {
  const char *pSceneImageName = "scenes/scenes.image";

  if (m_SceneImageFile.TellMaxPut() == 0) {
    MEM_ALLOC_CREDIT();

    if (filesystem->ReadFile(pSceneImageName, "GAME", m_SceneImageFile)) {
      SceneImageHeader_t *pHeader =
          (SceneImageHeader_t *)m_SceneImageFile.Base();
      if (pHeader->nId != SCENE_IMAGE_ID ||
          pHeader->nVersion != SCENE_IMAGE_VERSION) {
        Error("CSceneFileCache: Bad scene image file %s\n", pSceneImageName);
      }
    } else {
      m_SceneImageFile.Purge();
    }
  }

  return INIT_OK;
}

void CSceneFileCache::Shutdown() { m_SceneImageFile.Purge(); }

// Physically reloads image from disk
void CSceneFileCache::Reload() {
  Shutdown();
  Init();
}

size_t CSceneFileCache::GetSceneBufferSize(char const *pFilename) {
  size_t returnSize = 0;

  char fn[SOURCE_MAX_PATH];
  Q_strncpy(fn, pFilename, sizeof(fn));
  Q_FixSlashes(fn);
  Q_strlower(fn);

  GetSceneDataFromImage(pFilename, FindSceneInImage(fn), NULL, &returnSize);
  return returnSize;
}

bool CSceneFileCache::GetSceneData(char const *pFilename, uint8_t *buf,
                                   size_t bufsize) {
  Assert(pFilename);
  Assert(buf);
  Assert(bufsize > 0);

  char fn[SOURCE_MAX_PATH];
  Q_strncpy(fn, pFilename, sizeof(fn));
  Q_FixSlashes(fn);
  Q_strlower(fn);

  size_t nLength = bufsize;
  return GetSceneDataFromImage(pFilename, FindSceneInImage(fn), buf, &nLength);
}

bool CSceneFileCache::GetSceneCachedData(char const *pFilename,
                                         SceneCachedData_t *pData) {
  int iScene = FindSceneInImage(pFilename);
  SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
  if (!pHeader || iScene < 0 || iScene >= pHeader->nNumScenes) {
    // not available
    pData->sceneId = -1;
    pData->msecs = 0;
    pData->numSounds = 0;
    return false;
  }

  // get scene summary
  SceneImageEntry_t *pEntries =
      (SceneImageEntry_t *)((uint8_t *)pHeader + pHeader->nSceneEntryOffset);
  SceneImageSummary_t *pSummary =
      (SceneImageSummary_t *)((uint8_t *)pHeader +
                              pEntries[iScene].nSceneSummaryOffset);

  pData->sceneId = iScene;
  pData->msecs = pSummary->msecs;
  pData->numSounds = pSummary->numSounds;

  return true;
}

short CSceneFileCache::GetSceneCachedSound(int iScene, int iSound) {
  SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
  if (!pHeader || iScene < 0 || iScene >= pHeader->nNumScenes) {
    // huh?, image file not present or bad index
    return -1;
  }

  SceneImageEntry_t *pEntries =
      (SceneImageEntry_t *)((uint8_t *)pHeader + pHeader->nSceneEntryOffset);
  SceneImageSummary_t *pSummary =
      (SceneImageSummary_t *)((uint8_t *)pHeader +
                              pEntries[iScene].nSceneSummaryOffset);
  if (iSound < 0 || iSound >= pSummary->numSounds) {
    // bad index
    Assert(0);
    return -1;
  }

  return pSummary->soundStrings[iSound];
}

const char *CSceneFileCache::GetSceneString(short stringId) {
  SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
  if (!pHeader || stringId < 0 || stringId >= pHeader->nNumStrings) {
    // huh?, image file not present, or index bad
    return NULL;
  }

  return pHeader->String(stringId);
}

//-----------------------------------------------------------------------------
//  Returns -1 if not found, otherwise [0..n] index.
//-----------------------------------------------------------------------------
int CSceneFileCache::FindSceneInImage(const char *pSceneName) {
  auto *header = static_cast<SceneImageHeader_t *>(m_SceneImageFile.Base());
  if (!header) {
    return -1;
  }

  auto *entries = reinterpret_cast<SceneImageEntry_t *>(
      reinterpret_cast<uint8_t *>(header) + header->nSceneEntryOffset);

  char clean_name[SOURCE_MAX_PATH];
  strcpy_s(clean_name, pSceneName);
  _strlwr_s(clean_name);

#ifdef OS_POSIX
  V_FixSlashes(clean_name, '\\');
#else
  V_FixSlashes(clean_name);
#endif

  const CRC32_t file_name_crc =
      CRC32_ProcessSingleBuffer(clean_name, strlen(clean_name));

  // use binary search, entries are sorted by ascending crc
  int lower_idx = 1, upper_idx = header->nNumScenes;

  for (;;) {
    if (upper_idx < lower_idx) {
      return -1;
    }

    int middle_index = (lower_idx + upper_idx) / 2;
    const CRC32_t probe_crc = entries[middle_index - 1].crcFilename;

    if (file_name_crc < probe_crc) {
      upper_idx = middle_index - 1;
    } else {
      if (file_name_crc > probe_crc) {
        lower_idx = middle_index + 1;
      } else {
        return middle_index - 1;
      }
    }
  }
}

//-----------------------------------------------------------------------------
//  Returns true if success, false otherwise. Caller must free ouput scene data
//-----------------------------------------------------------------------------
bool CSceneFileCache::GetSceneDataFromImage(const char *pFileName, int iScene,
                                            uint8_t *pSceneData,
                                            size_t *pSceneLength) {
  SceneImageHeader_t *pHeader = (SceneImageHeader_t *)m_SceneImageFile.Base();
  if (!pHeader || iScene < 0 || iScene >= pHeader->nNumScenes) {
    if (pSceneData) {
      *pSceneData = NULL;
    }
    if (pSceneLength) {
      *pSceneLength = 0;
    }
    return false;
  }

  SceneImageEntry_t *pEntries =
      (SceneImageEntry_t *)((uint8_t *)pHeader + pHeader->nSceneEntryOffset);
  unsigned char *pData =
      (unsigned char *)pHeader + pEntries[iScene].nDataOffset;
  LZMA lzma;
  bool bIsCompressed;
  bIsCompressed = lzma.IsCompressed(pData);
  if (bIsCompressed) {
    usize originalSize = lzma.GetActualSize(pData);
    if (pSceneData) {
      usize nMaxLen = *pSceneLength;
      if (originalSize <= nMaxLen) {
        lzma.Uncompress(pData, pSceneData, nMaxLen);
      } else {
        u8 *pOutputData = (u8 *)malloc(originalSize);
        lzma.Uncompress(pData, pOutputData, originalSize);
        V_memcpy(pSceneData, pOutputData, nMaxLen);
        heap_free(pOutputData);
      }
    }
    if (pSceneLength) {
      *pSceneLength = originalSize;
    }
  } else {
    if (pSceneData) {
      size_t nCountToCopy =
          std::min(*pSceneLength, (size_t)pEntries[iScene].nDataLength);
      memcpy(pSceneData, pData, nCountToCopy);
    }
    if (pSceneLength) {
      *pSceneLength = (size_t)pEntries[iScene].nDataLength;
    }
  }
  return true;
}

static CSceneFileCache g_SceneFileCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSceneFileCache, ISceneFileCache,
                                  SCENE_FILE_CACHE_INTERFACE_VERSION,
                                  g_SceneFileCache);
