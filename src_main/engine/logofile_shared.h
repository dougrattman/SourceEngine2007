// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef LOGOFILE_SHARED_H
#define LOGOFILE_SHARED_H

#include "tier1/checksum_crc.h"

#define CUSTOM_FILES_FOLDER "downloads"

// Turns a CRC value into a filename.
class CCustomFilename {
 public:
  CCustomFilename(CRC32_t value) {
    char hex[16];
    Q_binarytohex((uint8_t *)&value, sizeof(value), hex, sizeof(hex));
    Q_snprintf(m_Filename, sizeof(m_Filename), "%s/%s.dat", CUSTOM_FILES_FOLDER,
               hex);
  }

  char m_Filename[MAX_OSPATH];
};

// Validate a VTF file.
bool LogoFile_IsValidVTFFile(const void *pData, int len);

// Read in and validate a logo file.
bool LogoFile_ReadFile(CRC32_t crcValue, CUtlVector<char> &fileData);

#endif  // LOGOFILE_SHARED_H
