// Copyright © 1996-2007, Valve Corporation, All rights reserved.
//
// Purpose: LZMA Glue. Designed for Tool time Encoding/Decoding.
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

#ifndef SOURCE_LZMA_H_
#define SOURCE_LZMA_H_

#include "base/include/base_types.h"

// power of two, 256k
#define LZMA_DEFAULT_DICTIONARY 18u

// These routines are designed for TOOL TIME encoding/decoding on the PC!
// They have not been made to encode/decode on the PPC and lack big endian
// awarnesss.  Lightweight GAME TIME Decoding is part of tier1.lib, via LZMA.

// Encoding glue. Returns non-null Compressed buffer if successful.
// Caller must free.
u8 *LZMA_Compress(u8 *in, usize in_size, usize *out_size,
  usize dictionary_size = LZMA_DEFAULT_DICTIONARY);

// Decoding glue. Returns true if successful.
bool LZMA_Uncompress(u8 *in, u8 **out, usize *out_size);

// Decoding helper, returns TRUE if buffer is LZMA compressed.
bool LZMA_IsCompressed(u8 *in);

// Decoding helper, returns non-zero size of data when uncompressed, otherwise
// 0.
u32 LZMA_GetActualSize(u8 *in);

#endif  // SOURCE_LZMA_H_
