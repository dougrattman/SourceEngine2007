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

#include "tier1/lzmaDecoder.h"

#include "deps/lzma/C/LzmaDec.h"

#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

#include "tier0/include/memdbgon.h"

namespace {
void *LzmaSzAlloc([[maybe_unused]] ISzAllocPtr p, usize size) {
  return heap_alloc<u8>(size);
}
void LzmaSzFree([[maybe_unused]] ISzAllocPtr p, void *address) {
  heap_free(address);
}

static ISzAlloc lzma_sz_alloc{LzmaSzAlloc, LzmaSzFree};
}  // namespace

// Returns true if buffer is compressed.
bool LZMA::IsCompressed(u8 *in) {
  auto *header = (lzma_header_t *)in;
  return header && header->id == LZMA_ID;
}

// Returns uncompressed size of compressed input buffer. Used for allocating
// output buffer for decompression. Returns 0 if input buffer is not compressed.
usize LZMA::GetActualSize(u8 *in) {
  auto *header = (lzma_header_t *)in;
  return header && header->id == LZMA_ID ? header->actualSize : 0;
}

// Uncompress a buffer, Returns the uncompressed size.
usize LZMA::Uncompress(u8 *in, u8 *out, usize out_size) {
  const usize actual_size{GetActualSize(in)};
  if (!actual_size) return 0;

  if (actual_size > out_size) {
    Error("Actual compressed data size is %zu, provided buffer size is %zu.\n");
    return 0;
  }

  auto *header = (lzma_header_t *)in;

  CLzmaDec state;
  LzmaDec_Construct(&state);

  if (LzmaDec_Allocate(&state, header->properties, LZMA_PROPS_SIZE,
                       &lzma_sz_alloc) != SZ_OK) {
    Error("Failed to allocate LZMA data.\n");
    return 0;
  }

  LzmaDec_Init(&state);

  ELzmaStatus status;
  usize out_processed{actual_size}, in_size{header->lzmaSize};
  SRes result{LzmaDec_DecodeToBuf(&state, out, &out_processed,
                                  in + sizeof(lzma_header_t), &in_size,
                                  LZMA_FINISH_ANY, &status)};

  LzmaDec_Free(&state, &lzma_sz_alloc);

  if (result != SZ_OK || out_processed != actual_size) {
    Error("LZMA decompress failure (%u), actual data size %zu, got %zu.\n");
    return 0;
  }

  return out_processed;
}
