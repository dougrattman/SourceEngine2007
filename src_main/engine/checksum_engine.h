// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Engine crc routines

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include "quakedef.h"
#include "tier1/checksum_crc.h"
#include "tier1/checksum_md5.h"

bool CRC_File(CRC32_t *crcvalue, const char *pszFileName);
uint8_t COM_BlockSequenceCRCByte(uint8_t *base, int length, int sequence);
bool CRC_MapFile(CRC32_t *crcvalue, const char *pszFileName);

bool MD5_Hash_File(unsigned char digest[16], const char *pszFileName,
                   bool bSeed, unsigned int seed[4]);

#endif  // CHECKSUM_H
