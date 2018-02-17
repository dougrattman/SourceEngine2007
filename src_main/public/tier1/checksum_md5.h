// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Generic MD5 hashing algorithm.

#ifndef SOURCE_TIER1_CHECKSUM_MD5_H_
#define SOURCE_TIER1_CHECKSUM_MD5_H_

// 16 bytes == 128 bit digest
#define MD5_DIGEST_LENGTH 16

// MD5 Hash
typedef struct {
  unsigned int buf[4];
  unsigned int bits[2];
  unsigned char in[64];
} MD5Context_t;

void MD5Init(MD5Context_t *context);
void MD5Update(MD5Context_t *context, unsigned char const *buf,
               unsigned int len);
void MD5Final(unsigned char digest[MD5_DIGEST_LENGTH], MD5Context_t *context);

char *MD5_Print(unsigned char *digest, int hashlen);

unsigned int MD5_PseudoRandom(unsigned int nSeed);

#endif  // SOURCE_TIER1_CHECKSUM_MD5_H_
