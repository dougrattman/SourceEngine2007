// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "common/lzma/lzma.h"

#include "deps/lzma/CPP/7zip/Compress/LZMADecoder.h"
#include "deps/lzma/CPP/7zip/Compress/LZMAEncoder.h"
#include "deps/lzma/CPP/Common/MyInitGuid.h"
#include "deps/lzma/CPP/Common/MyTypes.h"
#include "deps/lzma/CPP/Common/MyWindows.h"

#include "base/include/base_types.h"
#include "base/include/check.h"
#include "tier0/include/platform.h"
#include "tier1/lzmaDecoder.h"

// LZMA compressed file format
// ---------------------------
// Offset Size Description
//  0     1   Special LZMA properties (lc,lp, pb in encoded form)
//  1     4   Dictionary size (little endian)
//  5     8   Uncompressed size (little endian). -1 means unknown size
// 13         Compressed data
#define LZMA_ORIGINAL_HEADER_SIZE 13

#define SZE_OK 0
#define SZE_FAIL 1
#define SZE_OUTOFMEMORY 2
#define SZE_OUT_OVERFLOW 3

namespace {
class CInStreamRam : public ISequentialInStream, public CMyUnknownImp {
 public:
  CInStreamRam(const u8 *data, usize size) : data_{data}, size_{size}, pos_{0} {
    DCHECK(data, EINVAL);
  }

  MY_UNKNOWN_IMP

  STDMETHOD(Read)(void *data, u32 size, u32 *processedSize) override {
    DCHECK(data, EINVAL);

    const u32 remain{size_ - pos_};

    if (size > remain) size = remain;

    for (u32 i{0}; i < size; ++i) ((u8 *)data)[i] = data_[pos_ + i];

    pos_ += size;

    if (processedSize != nullptr) *processedSize = size;

    return S_OK;
  }

 private:
  const u8 *data_;
  const usize size_;
  usize pos_;
};

struct IOutStreamRam : public ISequentialOutStream {
  STDMETHOD(GetPos)(usize &pos) const PURE;
  STDMETHOD(SetPos)(usize pos) PURE;

  STDMETHOD(GetHasOverflow)(bool &has_overflow) const PURE;

  STDMETHOD(WriteByte)(u8 b) PURE;
};

class COutStreamRam : public IOutStreamRam, public CMyUnknownImp {
 public:
  COutStreamRam(u8 *data, usize size)
      : data_{data}, size_{size}, position_{0}, has_overflow_{false} {
    DCHECK(data, EINVAL);
  }

  MY_UNKNOWN_IMP

  STDMETHOD(Write)
  (const void *data, u32 size, u32 *processedSize) override {
    DCHECK(data, EINVAL);

    u32 i;

    for (i = 0; i < size && position_ < size_; i++)
      data_[position_++] = ((const u8 *)data)[i];

    if (processedSize != nullptr) *processedSize = i;

    if (i != size) {
      has_overflow_ = true;
      return E_FAIL;
    }

    return S_OK;
  }

  STDMETHOD(GetPos)
  (usize &pos) const override {
    pos = position_;
    return S_OK;
  }

  STDMETHOD(SetPos)(usize pos) override {
    has_overflow_ = false;
    position_ = pos;
    return S_OK;
  }

  STDMETHOD(GetHasOverflow)
  (bool &has_overflow) const override {
    has_overflow = has_overflow_;
    return S_OK;
  }

  STDMETHOD(WriteByte)(u8 b) override {
    if (position_ >= size_) {
      has_overflow_ = true;
      return E_FAIL;
    }

    data_[position_++] = b;
    return S_OK;
  }

 private:
  u8 *data_;
  const usize size_;
  usize position_;
  bool has_overflow_;
};

int LzmaEncode(const u8 *in, usize in_size, u8 *out, usize out_size,
               usize *out_size_processed, usize dictionary_size) {
  DCHECK(in, EINVAL);
  DCHECK(out, EINVAL);
  DCHECK(out_size_processed, EINVAL);

  *out_size_processed = 0;

  constexpr usize kLzmaPropsSize{5};
  constexpr usize kMinDestSize{LZMA_ORIGINAL_HEADER_SIZE};

  if (out_size < kMinDestSize) return SZE_OUT_OVERFLOW;

  auto *encoder_spec = new NCompress::NLzma::CEncoder;
  if (encoder_spec == nullptr) return SZE_OUTOFMEMORY;

  CMyComPtr<ICompressCoder> encoder{encoder_spec};

  // defaults
  u32 posStateBits = 2;
  u32 litContextBits = 3;
  u32 litPosBits = 0;
  u32 algorithm = 2;
  u32 numFastBytes = 64;
  const wch *mf = L"BT4";

  PROPID propIDs[] = {
      NCoderPropID::kDictionarySize, NCoderPropID::kPosStateBits,
      NCoderPropID::kLitContextBits, NCoderPropID::kLitPosBits,
      NCoderPropID::kAlgorithm,      NCoderPropID::kNumFastBytes,
      NCoderPropID::kMatchFinder,    NCoderPropID::kEndMarker,
  };
  PROPVARIANT properties[std::size(propIDs)];

  // set all properties
  properties[0].vt = VT_UI4;
  properties[0].ulVal = u32(dictionary_size);
  properties[1].vt = VT_UI4;
  properties[1].ulVal = u32(posStateBits);
  properties[2].vt = VT_UI4;
  properties[2].ulVal = u32(litContextBits);
  properties[3].vt = VT_UI4;
  properties[3].ulVal = u32(litPosBits);
  properties[4].vt = VT_UI4;
  properties[4].ulVal = u32(algorithm);
  properties[5].vt = VT_UI4;
  properties[5].ulVal = u32(numFastBytes);
  properties[6].vt = VT_BSTR;
  properties[6].bstrVal = (BSTR)(const wch *)mf;
  properties[7].vt = VT_BOOL;
  properties[7].boolVal = VARIANT_FALSE;

  if (encoder_spec->SetCoderProperties(propIDs, properties,
                                       std::size(propIDs)) != S_OK)
    return SZE_FAIL;

  CMyComPtr<IOutStreamRam> outStream = new COutStreamRam{out, out_size};
  if (outStream == nullptr) return SZE_OUTOFMEMORY;

  CMyComPtr<ISequentialInStream> inStream = new CInStreamRam{in, in_size};
  if (inStream == nullptr) return SZE_OUTOFMEMORY;

  if (encoder_spec->WriteCoderProperties(outStream) != S_OK)
    return SZE_OUT_OVERFLOW;

  usize pos;
  if (S_OK != outStream->GetPos(pos) || pos != kLzmaPropsSize) return SZE_FAIL;

  for (int i = 0; i < 8; i++) {
    u64 t = in_size;

    if (outStream->WriteByte((u8)((t) >> (8 * i))) != S_OK)
      return SZE_OUT_OVERFLOW;
  }

  u32 minSize = 0;
  HRESULT result_code = outStream->GetPos(pos);
  usize startPos = pos;

  if (S_OK != result_code) return SZE_FAIL;
  if (S_OK != outStream->SetPos(startPos)) return SZE_FAIL;

  HRESULT lzma_result_code = encoder->Code(inStream, outStream, 0, 0, 0);
  HRESULT pos_result_code = outStream->GetPos(pos);

  if (lzma_result_code == E_OUTOFMEMORY) {
    result_code = SZE_OUTOFMEMORY;
  } else if (pos_result_code == S_OK && pos <= minSize) {
    minSize = pos;
  }

  bool has_overflow;
  if (outStream->GetHasOverflow(has_overflow) == S_OK && has_overflow) {
    result_code = SZE_OUT_OVERFLOW;
  } else if (lzma_result_code != S_OK) {
    result_code = SZE_FAIL;
  }

  *out_size_processed = pos;

  return result_code;
}

void *LzmaSzAlloc([[maybe_unused]] ISzAllocPtr p, usize size) {
  return heap_alloc<u8>(size);
}
void LzmaSzFree([[maybe_unused]] ISzAllocPtr p, void *address) {
  heap_free(address);
}

static ISzAlloc lzma_sz_alloc{LzmaSzAlloc, LzmaSzFree};
}  // namespace

// Encoding glue.  Returns non-null compressed buffer if successful.  Caller
// must free.
u8 *LZMA_Compress(u8 *in, usize in_size, usize *out_size,
                  usize dictionary_size) {
  DCHECK(in, EINVAL);
  DCHECK(out_size, EINVAL);

  *out_size = 0;

  // pointless
  if (in_size <= sizeof(lzma_header_t)) return nullptr;

  dictionary_size = 1 << dictionary_size;

  // using same work buffer calcs as the SDK 105% + 64K
  usize outSize = in_size / 20 * 21 + (1 << 16);
  u8 *out_buf = heap_alloc<u8>(outSize);
  if (!out_buf) return nullptr;

  // compress, skipping past our header
  u32 compressedSize;
  int result = LzmaEncode(in, in_size, out_buf + sizeof(lzma_header_t),
                          outSize - sizeof(lzma_header_t), &compressedSize,
                          dictionary_size);
  if (result != SZE_OK) {
    heap_free(out_buf);
    return nullptr;
  }

  if (compressedSize - LZMA_ORIGINAL_HEADER_SIZE + sizeof(lzma_header_t) >=
      in_size) {
    // compression got worse or stayed the same
    heap_free(out_buf);
    return nullptr;
  }

  // construct our header, strip theirs
  lzma_header_t *header = (lzma_header_t *)out_buf;
  header->id = LZMA_ID;
  header->actualSize = in_size;
  header->lzmaSize = compressedSize - LZMA_ORIGINAL_HEADER_SIZE;
  memcpy(header->properties, out_buf + sizeof(lzma_header_t), LZMA_PROPS_SIZE);

  // shift the compressed data into place
  memcpy(out_buf + sizeof(lzma_header_t),
         out_buf + sizeof(lzma_header_t) + LZMA_ORIGINAL_HEADER_SIZE,
         compressedSize - LZMA_ORIGINAL_HEADER_SIZE);

  // final output size is our header plus compressed bits
  *out_size =
      sizeof(lzma_header_t) + compressedSize - LZMA_ORIGINAL_HEADER_SIZE;

  return out_buf;
}

// Decoding glue.  Returns TRUE if succesful.
bool LZMA_Uncompress(u8 *in, u8 **out, usize *out_size) {
  DCHECK(in, EINVAL);
  DCHECK(out, EINVAL);
  DCHECK(out_size, EINVAL);

  *out = nullptr;
  *out_size = 0;

  auto *header = reinterpret_cast<lzma_header_t *>(in);

  // not ours
  if (!header || header->id != LZMA_ID) return false;

  // static_assert(std::size(lzma_header_t::properties) == LZMA_PROPS_SIZE);

  CLzmaDec state;
  LzmaDec_Construct(&state);

  if (LzmaDec_Allocate(&state, header->properties, LZMA_PROPS_SIZE,
                       &lzma_sz_alloc) != SZ_OK) {
    return false;
  }

  LzmaDec_Init(&state);

  u8 *out_buf{heap_alloc<u8>(header->actualSize)};
  if (!out_buf) {
    LzmaDec_Free(&state, &lzma_sz_alloc);
    return false;
  }

  // No end marker in LZMA_Compress.
  ELzmaStatus status;
  usize out_processed{header->actualSize};
  SRes result{LzmaDec_DecodeToBuf(&state, out_buf, &out_processed,
                                  in + sizeof(lzma_header_t), &header->lzmaSize,
                                  LZMA_FINISH_ANY, &status)};

  LzmaDec_Free(&state, &lzma_sz_alloc);

  if (result != SZ_OK || header->actualSize != out_processed) {
    heap_free(out_buf);
    return false;
  }

  *out = out_buf;
  *out_size = header->actualSize;

  return true;
}

bool LZMA_IsCompressed(u8 *in) {
  auto *header = reinterpret_cast<lzma_header_t *>(in);
  return header && header->id == LZMA_ID;
}

u32 LZMA_GetActualSize(u8 *in) {
  auto *header = reinterpret_cast<lzma_header_t *>(in);
  return header && header->id == LZMA_ID ? header->actualSize : 0u;
}