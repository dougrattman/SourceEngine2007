// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
//	LZMA Decoder. Designed for run time decoding.
//
//	LZMA SDK 4.43 Copyright (c) 1999-2006 Igor Pavlov (2006-05-01)
//	http://www.7-zip.org/

#ifndef SOURCE_TIER1_LZMADECODER_H_
#define SOURCE_TIER1_LZMADECODER_H_

#define LZMA_ID (('A' << 24) | ('M' << 16) | ('Z' << 8) | ('L'))

// bind the buffer for correct identification
#pragma pack(1)
struct lzma_header_t {
  unsigned int id;
  unsigned int actualSize;  // always little endian
  unsigned int lzmaSize;    // always little endian
  unsigned char properties[5];
};
#pragma pack()

class CLZMA {
 public:
  unsigned int Uncompress(unsigned char *pInput, unsigned char *pOutput);
  bool IsCompressed(unsigned char *pInput);
  unsigned int GetActualSize(unsigned char *pInput);
};

#endif  // SOURCE_TIER1_LZMADECODER_H_
