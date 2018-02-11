// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// LZSS Codec. Designed for fast cheap game time encoding/decoding.
// Compression results are	not aggressive as other algorithms, but gets 2:1
// on most arbitrary uncompressed data.

#ifndef SOURCE_TIER1_LZSS_H_
#define SOURCE_TIER1_LZSS_H_

#include "tier0/include/calling_conventions.h"

#define LZSS_ID (('S' << 24) | ('S' << 16) | ('Z' << 8) | ('L'))

// Bind the buffer for correct identification.
struct lzss_header_t {
  unsigned int id;
  unsigned int actualSize;  // Always little endian.
};

#define DEFAULT_LZSS_WINDOW_SIZE 4096

class CLZSS {
 public:
  unsigned char *Compress(unsigned char *pInput, int inputlen,
                          unsigned int *pOutputSize);
  unsigned char *CompressNoAlloc(unsigned char *pInput, int inputlen,
                                 unsigned char *pOutput,
                                 unsigned int *pOutputSize);
  unsigned int Uncompress(unsigned char *pInput, unsigned char *pOutput);
  bool IsCompressed(unsigned char *pInput);
  unsigned int GetActualSize(unsigned char *pInput);

  // Windowsize must be a power of two.
  FORCEINLINE CLZSS(int nWindowSize = DEFAULT_LZSS_WINDOW_SIZE);

 private:
  // Expected to be sixteen bytes.
  struct lzss_node_t {
    unsigned char *pData;
    lzss_node_t *pPrev;
    lzss_node_t *pNext;
    char empty[4];
  };

  struct lzss_list_t {
    lzss_node_t *pStart;
    lzss_node_t *pEnd;
  };

  void BuildHash(unsigned char *pData);
  lzss_list_t *m_pHashTable;
  lzss_node_t *m_pHashTarget;
  int m_nWindowSize;
};

FORCEINLINE CLZSS::CLZSS(int nWindowSize) { m_nWindowSize = nWindowSize; }

#endif  // SOURCE_TIER1_LZSS_H_
