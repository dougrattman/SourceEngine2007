// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "pch_materialsystem.h"

// NOTE: currently this file is marked as "exclude from build"

//#define _CHECK_MATERIALS_FOR_PROBLEMS 1
#ifdef _CHECK_MATERIALS_FOR_PROBLEMS
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "vtf/vtf.h"
void CheckMateralsInDirectoryRecursive(const char *pRoot,
                                       const char *pDirectory);
#endif

#ifdef _CHECK_MATERIALS_FOR_PROBLEMS
// Does a texture have alpha?
static bool DoesTextureUseAlpha(const char *pTextureName,
                                const char *pMaterialName) {
  // Special textures start with '_'..
  if (pTextureName[0] == '_') return false;

  // The texture name doubles as the relative file name
  // It's assumed to have already been set by this point
  // Compute the cache name
  char pCacheFileName[MATERIAL_MAX_PATH];
  Q_snprintf(pCacheFileName, sizeof(pCacheFileName), "materials/%s.vtf",
             pTextureName);

  CUtlBuffer buf;
  FileHandle_t fileHandle = g_pFullFileSystem->Open(pCacheFileName, "rb");
  if (fileHandle == FILESYSTEM_INVALID_HANDLE) {
    Warning("Material \"%s\": can't open texture \"%s\"\n", pMaterialName,
            pCacheFileName);
    return false;
  }

  // Check the .vtf for an alpha channel
  IVTFTexture *pVTFTexture = CreateVTFTexture();

  int nHeaderSize = VTFFileHeaderSize(VTF_MAJOR_VERSION);
  buf.EnsureCapacity(nHeaderSize);

  // read the header first.. it's faster!!
  g_pFullFileSystem->Read(buf.Base(), nHeaderSize, fileHandle);
  buf.SeekPut(CUtlBuffer::SEEK_HEAD, nHeaderSize);

  // Unserialize the header
  bool bUsesAlpha = false;

  if (!pVTFTexture->Unserialize(buf, true)) {
    Warning("Error reading material \"%s\"\n", pCacheFileName);
    g_pFullFileSystem->Close(fileHandle);
  } else {
    if (pVTFTexture->Flags() &
        (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA)) {
      bUsesAlpha = true;
    }
  }

  DestroyVTFTexture(pVTFTexture);
  g_pFullFileSystem->Close(fileHandle);
  return bUsesAlpha;
}

// Does a texture have alpha?
static bool DoesTextureUseNormal(const char *pTextureName,
                                 const char *pMaterialName, bool &bUsesAlpha,
                                 bool &bIsCompressed, int &nSizeInBytes) {
  nSizeInBytes = 0;
  bUsesAlpha = false;

  // Special textures start with '_'..
  if (!pTextureName || (pTextureName[0] == '_') || (pTextureName[0] == 0))
    return false;

  // The texture name doubles as the relative file name
  // It's assumed to have already been set by this point
  // Compute the cache name
  char pCacheFileName[MATERIAL_MAX_PATH];
  Q_snprintf(pCacheFileName, sizeof(pCacheFileName), "materials/%s.vtf",
             pTextureName);

  CUtlBuffer buf;
  FileHandle_t fileHandle = g_pFullFileSystem->Open(pCacheFileName, "rb");
  if (fileHandle == FILESYSTEM_INVALID_HANDLE) {
    //		Warning( "Material \"%s\": can't open texture \"%s\"\n",
    // pMaterialName, pCacheFileName );
    return false;
  }

  // Check the .vtf for an alpha channel
  IVTFTexture *pVTFTexture = CreateVTFTexture();

  int nHeaderSize = VTFFileHeaderSize(VTF_MAJOR_VERSION);
  buf.EnsureCapacity(nHeaderSize);

  // read the header first.. it's faster!!
  g_pFullFileSystem->Read(buf.Base(), nHeaderSize, fileHandle);
  buf.SeekPut(CUtlBuffer::SEEK_HEAD, nHeaderSize);

  // Unserialize the header
  bool bUsesNormal = false;
  if (!pVTFTexture->Unserialize(buf, true)) {
    Warning("Error reading material \"%s\"\n", pCacheFileName);
  } else {
    if (pVTFTexture->Flags() & TEXTUREFLAGS_NORMAL) {
      bUsesAlpha = false;
      bUsesNormal = true;
      bIsCompressed = ImageLoader::IsCompressed(pVTFTexture->Format()) ||
                      (pVTFTexture->Format() == IMAGE_FORMAT_A8);
      nSizeInBytes = pVTFTexture->ComputeTotalSize();
      if (pVTFTexture->Flags() &
          (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA)) {
        bUsesAlpha = true;
      }
    }
  }

  DestroyVTFTexture(pVTFTexture);
  g_pFullFileSystem->Close(fileHandle);
  return bUsesNormal;
}

// Is this a real texture
static bool IsTexture(const char *pTextureName) {
  // Special textures start with '_'..
  if (pTextureName[0] == '_') return false;

  // The texture name doubles as the relative file name
  // It's assumed to have already been set by this point
  // Compute the cache name
  char pCacheFileName[MATERIAL_MAX_PATH];
  Q_snprintf(pCacheFileName, sizeof(pCacheFileName), "materials/%s.vtf",
             pTextureName);

  FileHandle_t fileHandle = g_pFullFileSystem->Open(pCacheFileName, "rb");
  if (fileHandle == FILESYSTEM_INVALID_HANDLE) return false;

  g_pFullFileSystem->Close(fileHandle);
  return true;
}

// Scan material + all subsections for key
static float MaterialFloatKeyValue(KeyValues *pKeyValues, const char *pKeyName,
                                   float flDefault) {
  float flValue = pKeyValues->GetFloat(pKeyName, flDefault);
  if (flValue != flDefault) return flValue;

  for (KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey;
       pSubKey = pSubKey->GetNextTrueSubKey()) {
    float flValue = MaterialFloatKeyValue(pSubKey, pKeyName, flDefault);
    if (flValue != flDefault) return flValue;
  }

  return flDefault;
}

int ParseVectorFromKeyValueString(KeyValues *pKeyValue,
                                  const char *pMaterialName, float vecVal[4]);

static bool AsVectorsEqual(int nDim1, float *pVector1, int nDim2,
                           float *pVector2) {
  if (nDim1 != nDim2) return false;

  for (int i = 0; i < nDim1; ++i) {
    if (fabs(pVector1[i] - pVector2[i]) > 1e-3) return false;
  }

  return true;
}

static bool MaterialVectorKeyValue(KeyValues *pKeyValues, const char *pKeyName,
                                   int nDefaultDim, float *pDefault, int *pDim,
                                   float *pVector) {
  int nDim;
  float retVal[4];

  KeyValues *pValue = pKeyValues->FindKey(pKeyName);
  if (pValue) {
    switch (pValue->GetDataType()) {
      case KeyValues::TYPE_INT: {
        int nInt = pValue->GetInt();
        for (int i = 0; i < 4; ++i) {
          retVal[i] = nInt;
        }
        if (!AsVectorsEqual(nDefaultDim, pDefault, nDefaultDim, retVal)) {
          *pDim = nDefaultDim;
          memcpy(pVector, retVal, nDefaultDim * sizeof(float));
          return true;
        }
      } break;

      case KeyValues::TYPE_FLOAT: {
        float flFloat = pValue->GetFloat();
        for (int i = 0; i < 4; ++i) {
          retVal[i] = flFloat;
        }
        if (!AsVectorsEqual(nDefaultDim, pDefault, nDefaultDim, retVal)) {
          *pDim = nDefaultDim;
          memcpy(pVector, retVal, nDefaultDim * sizeof(float));
          return true;
        }
      } break;

      case KeyValues::TYPE_STRING: {
        nDim = ParseVectorFromKeyValueString(pValue, "", retVal);
        if (!AsVectorsEqual(nDefaultDim, pDefault, nDim, retVal)) {
          *pDim = nDim;
          memcpy(pVector, retVal, nDim * sizeof(float));
          return true;
        }
      } break;
    }
  }

  for (KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey;
       pSubKey = pSubKey->GetNextTrueSubKey()) {
    if (MaterialVectorKeyValue(pSubKey, pKeyName, nDefaultDim, pDefault, &nDim,
                               retVal)) {
      *pDim = nDim;
      memcpy(pVector, retVal, nDim * sizeof(float));
      return true;
    }
  }

  *pDim = nDefaultDim;
  memcpy(pVector, pDefault, nDefaultDim * sizeof(float));
  return false;
}

// Scan material + all subsections for key
static bool DoesMaterialHaveKey(KeyValues *pKeyValues, const char *pKeyName) {
  if (pKeyValues->GetString(pKeyName, NULL) != NULL) return true;

  for (KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey;
       pSubKey = pSubKey->GetNextTrueSubKey()) {
    if (DoesMaterialHaveKey(pSubKey, pKeyName)) return true;
  }

  return false;
}

// Scan all materials for errors
static int s_nNormalBytes;
static int s_nNormalCompressedBytes;
static int s_nNormalPalettizedBytes;
static int s_nNormalWithAlphaBytes;
static int s_nNormalWithAlphaCompressedBytes;

struct VTFInfo_t {
  CUtlString m_VTFName;
  bool m_bFoundInVMT;
};

void CheckKeyValues(KeyValues *pKeyValues, CUtlVector<VTFInfo_t> &vtf) {
  for (KeyValues *pSubKey = pKeyValues->GetFirstValue(); pSubKey;
       pSubKey = pSubKey->GetNextValue()) {
    if (pSubKey->GetDataType() != KeyValues::TYPE_STRING) continue;

    if (IsTexture(pSubKey->GetString())) {
      int nLen = Q_strlen(pSubKey->GetString()) + 1;
      char *pTemp = (char *)_alloca(nLen);
      memcpy(pTemp, pSubKey->GetString(), nLen);
      Q_FixSlashes(pTemp);

      int nCount = vtf.Count();
      for (int i = 0; i < nCount; ++i) {
        if (Q_stricmp(vtf[i].m_VTFName, pTemp)) continue;
        vtf[i].m_bFoundInVMT = true;
        break;
      }
    }
  }

  for (KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey;
       pSubKey = pSubKey->GetNextTrueSubKey()) {
    CheckKeyValues(pSubKey, vtf);
  }
}

void CheckMaterial(KeyValues *pKeyValues, const char *pRoot,
                   const char *pFileName, CUtlVector<VTFInfo_t> &vtf) {
  const char *pShaderName = pKeyValues->GetName();

  for (KeyValues *pSubKey = pKeyValues->GetFirstValue(); pSubKey;
       pSubKey = pSubKey->GetNextValue()) {
    //		Msg( " Checking %s\n", pSubKey->GetString() );
    if (pSubKey->GetDataType() != KeyValues::TYPE_STRING) continue;

    bool bUsesAlpha, bIsCompressed;
    int nSizeInBytes;
    if (DoesTextureUseNormal(pSubKey->GetString(), pFileName, bUsesAlpha,
                             bIsCompressed, nSizeInBytes)) {
      if (bUsesAlpha) {
        if (bIsCompressed) {
          s_nNormalWithAlphaCompressedBytes += nSizeInBytes;
        } else {
          s_nNormalWithAlphaBytes += nSizeInBytes;
          Msg("Normal texture w alpha uncompressed %s\n", pSubKey->GetString());
        }
      } else {
        if (bIsCompressed) {
          s_nNormalCompressedBytes += nSizeInBytes;
        } else {
          s_nNormalBytes += nSizeInBytes;
        }
      }
    }
  }

  /*
          if ( !Q_stristr( pShaderName, "VertexLitGeneric" ) )
                  return;

          if ( !DoesMaterialHaveKey( pKeyValues, "$envmap" ) &&
     DoesMaterialHaveKey( pKeyValues, "$bumpmap" ) )
          {
                  Warning("BUMPMAP + no ENVMAP : Material \"%s\"\n", pFileName
     );
          }
  */

  //	CheckKeyValues( pKeyValues, vtf );
}

// Build list of all VTFs
void CheckVTFInDirectoryRecursive(const char *pRoot, const char *pDirectory,
                                  CUtlVector<VTFInfo_t> &vtf) {
#define BUF_SIZE 1024
  char buf[BUF_SIZE];
  WIN32_FIND_DATA wfd;
  HANDLE findHandle;

  sprintf(buf, "%s/%s/*.vtf", pRoot, pDirectory);

  findHandle = FindFirstFile(buf, &wfd);
  if (findHandle != INVALID_HANDLE_VALUE) {
    do {
      int i = vtf.AddToTail();

      char buf[SOURCE_MAX_PATH];
      char buf2[SOURCE_MAX_PATH];
      Q_snprintf(buf, SOURCE_MAX_PATH, "%s/%s", pDirectory, wfd.cFileName);
      Q_FixSlashes(buf);

      Q_StripExtension(buf, buf2, sizeof(buf2));
      Assert(!Q_strnicmp(buf2, "materials\\", 10));

      vtf[i].m_VTFName = &buf2[10];
      vtf[i].m_bFoundInVMT = false;

    } while (FindNextFile(findHandle, &wfd));

    FindClose(findHandle);
  }

  // do subdirectories
  sprintf(buf, "%s/%s/*.*", pRoot, pDirectory);
  findHandle = FindFirstFile(buf, &wfd);
  if (findHandle != INVALID_HANDLE_VALUE) {
    do {
      if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if ((strcmp(wfd.cFileName, "..") == 0) ||
            (strcmp(wfd.cFileName, ".") == 0)) {
          continue;
        }

        char buf[SOURCE_MAX_PATH];
        Q_snprintf(buf, SOURCE_MAX_PATH, "%s/%s", pDirectory, wfd.cFileName);
        CheckVTFInDirectoryRecursive(pRoot, buf, vtf);
      }
    } while (FindNextFile(findHandle, &wfd));
    FindClose(findHandle);
  }

#undef BUF_SIZE
}

// Scan all materials for errors
void _CheckMateralsInDirectoryRecursive(const char *pRoot,
                                        const char *pDirectory,
                                        CUtlVector<VTFInfo_t> &vtf) {
#define BUF_SIZE 1024
  char buf[BUF_SIZE];
  WIN32_FIND_DATA wfd;
  HANDLE findHandle;

  sprintf(buf, "%s/%s/*.vmt", pRoot, pDirectory);
  findHandle = FindFirstFile(buf, &wfd);
  if (findHandle != INVALID_HANDLE_VALUE) {
    do {
      KeyValues *vmtKeyValues = new KeyValues("vmt");

      char pFileName[SOURCE_MAX_PATH];
      Q_snprintf(pFileName, sizeof(pFileName), "%s/%s", pDirectory,
                 wfd.cFileName);
      if (!vmtKeyValues->LoadFromFile(g_pFullFileSystem, pFileName, "GAME")) {
        Warning("CheckMateralsInDirectoryRecursive: can't open \"%s\"\n",
                pFileName);
        continue;
      }

      CheckMaterial(vmtKeyValues, pRoot, pFileName, vtf);

      vmtKeyValues->deleteThis();

    } while (FindNextFile(findHandle, &wfd));

    FindClose(findHandle);
  }

  // do subdirectories
  sprintf(buf, "%s/%s/*.*", pRoot, pDirectory);
  findHandle = FindFirstFile(buf, &wfd);
  if (findHandle != INVALID_HANDLE_VALUE) {
    do {
      if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if ((strcmp(wfd.cFileName, "..") == 0) ||
            (strcmp(wfd.cFileName, ".") == 0)) {
          continue;
        }

        char buf[SOURCE_MAX_PATH];
        Q_snprintf(buf, SOURCE_MAX_PATH, "%s/%s", pDirectory, wfd.cFileName);
        _CheckMateralsInDirectoryRecursive(pRoot, buf, vtf);
      }
    } while (FindNextFile(findHandle, &wfd));
    FindClose(findHandle);
  }

//	Msg( "Normal only %d/%d/%d Normal w alpha %d/%d\n", s_nNormalBytes,
// s_nNormalPalettizedBytes, s_nNormalCompressedBytes, s_nNormalWithAlphaBytes,
// s_nNormalWithAlphaCompressedBytes );
#undef BUF_SIZE
}

void CheckMateralsInDirectoryRecursive(const char *pRoot,
                                       const char *pDirectory) {
  CUtlVector<VTFInfo_t> vtfNames;
  //	CheckVTFInDirectoryRecursive( pRoot, pDirectory, vtfNames );
  _CheckMateralsInDirectoryRecursive(pRoot, pDirectory, vtfNames);

  /*
  int nCount = vtfNames.Count();
  for ( int i = 0; i < nCount; ++i )
  {
          if ( !vtfNames[i].m_bFoundInVMT )
          {
                  Msg( "Unused VTF %s\n", vtfNames[i].m_VTFName );
          }
  }
  */
}

#endif  // _CHECK_MATERIALS_FOR_PROBLEMS
