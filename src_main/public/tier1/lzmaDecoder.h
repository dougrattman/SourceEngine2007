// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// LZMA SDK is written and placed in the public domain by Igor Pavlov.
//
// Some code in LZMA SDK is based on public domain code from another developers:
//   1) PPMd var.H (2001): Dmitry Shkarin
//   2) SHA-256: Wei Dai (Crypto++ library)
//
// Anyone is free to copy, modify, publish, use, compile, sell, or distribute
// the original LZMA SDK code, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any means.
//
// LZMA SDK code is compatible with open source licenses, for example, you can
// include it to GNU GPL or GNU LGPL code.

#ifndef SOURCE_TIER1_LZMADECODER_H_
#define SOURCE_TIER1_LZMADECODER_H_

#include "base/include/base_types.h"

#define LZMA_ID (('A' << 24) | ('M' << 16) | ('Z' << 8) | ('L'))

// Bind the buffer for correct identification
#pragma pack(1)
struct lzma_header_t {
  u32 id;
  u32 actualSize;  // always little endian
  u32 lzmaSize;    // always little endian
  u8 properties[5];
};
#pragma pack()

struct LZMA {
  usize Uncompress(u8 *in, u8 *out, usize out_size);
  bool IsCompressed(u8 *in);
  usize GetActualSize(u8 *in);
};

#endif  // SOURCE_TIER1_LZMADECODER_H_
