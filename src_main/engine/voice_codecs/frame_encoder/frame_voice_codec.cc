// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "frame_voice_codec.h"

#include <algorithm>
#include <cstring>

#include "audio/public/ivoicecodec.h"
#include "iframe_encoder.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

// VoiceCodecAdapter can be used to wrap a frame encoder for the engine. As it
// gets sound data, it will queue it until it has enough for a frame, then it
// will compress it. Same thing for decompression.
class FrameVoiceCodec : public IVoiceCodec {
 public:
  FrameVoiceCodec(IFrameEncoder *frame_encoder) {
    m_nEncodeBufferSamples = 0;
    m_nRawBytes = m_nRawSamples = m_nEncodedBytes = 0;
    frame_encoder_ = frame_encoder;
  }

  virtual ~FrameVoiceCodec() {
    if (frame_encoder_) frame_encoder_->Release();
  }

  bool Init(int quality) override {
    if (frame_encoder_ &&
        frame_encoder_->Init(quality, m_nRawBytes, m_nEncodedBytes)) {
      m_nRawSamples = m_nRawBytes >> 1;
      Assert(m_nRawBytes <= kMaxFrameBufferSamples &&
             m_nEncodedBytes <= kMaxFrameBufferSamples);
      return true;
    } else {
      if (frame_encoder_) frame_encoder_->Release();

      frame_encoder_ = NULL;
      return false;
    }
  }

  void Release() override { delete this; }

  int Compress(const char *pUncompressedBytes, int nSamples, char *pCompressed,
               int maxCompressedBytes, bool bFinal) override {
    if (!frame_encoder_) return 0;

    const short *pUncompressed = (const short *)pUncompressedBytes;

    int nCompressedBytes = 0;
    while ((nSamples + m_nEncodeBufferSamples) >= m_nRawSamples &&
           (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes) {
      // Get the data block out.
      short samples[kMaxFrameBufferSamples];
      memcpy(samples, m_EncodeBuffer,
             m_nEncodeBufferSamples * BYTES_PER_SAMPLE);
      memcpy(&samples[m_nEncodeBufferSamples], pUncompressed,
             (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE);
      nSamples -= m_nRawSamples - m_nEncodeBufferSamples;
      pUncompressed += m_nRawSamples - m_nEncodeBufferSamples;
      m_nEncodeBufferSamples = 0;

      // Compress it.
      frame_encoder_->EncodeFrame((const char *)samples,
                                  &pCompressed[nCompressedBytes]);
      nCompressedBytes += m_nEncodedBytes;
    }

    // Store the remaining samples.
    int nNewSamples = std::min(
        nSamples,
        std::min(m_nRawSamples - m_nEncodeBufferSamples, m_nRawSamples));
    if (nNewSamples) {
      memcpy(&m_EncodeBuffer[m_nEncodeBufferSamples],
             &pUncompressed[nSamples - nNewSamples],
             nNewSamples * BYTES_PER_SAMPLE);
      m_nEncodeBufferSamples += nNewSamples;
    }

    // If it must get the last data, just pad with zeros..
    if (bFinal && m_nEncodeBufferSamples &&
        (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes) {
      memset(&m_EncodeBuffer[m_nEncodeBufferSamples], 0,
             (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE);
      frame_encoder_->EncodeFrame((const char *)m_EncodeBuffer,
                                  &pCompressed[nCompressedBytes]);
      nCompressedBytes += m_nEncodedBytes;
      m_nEncodeBufferSamples = 0;
    }

    return nCompressedBytes;
  }

  int Decompress(const char *pCompressed, int compressedBytes,
                 char *pUncompressed, int maxUncompressedBytes) override {
    if (!frame_encoder_) return 0;

    Assert((compressedBytes % m_nEncodedBytes) == 0);
    int nDecompressedBytes = 0;
    int curCompressedByte = 0;
    while ((compressedBytes - curCompressedByte) >= m_nEncodedBytes &&
           (maxUncompressedBytes - nDecompressedBytes) >= m_nRawBytes) {
      frame_encoder_->DecodeFrame(&pCompressed[curCompressedByte],
                                  &pUncompressed[nDecompressedBytes]);
      curCompressedByte += m_nEncodedBytes;
      nDecompressedBytes += m_nRawBytes;
    }

    return nDecompressedBytes / BYTES_PER_SAMPLE;
  }

  bool ResetState() override {
    return frame_encoder_ && frame_encoder_->ResetState();
  }

 private:
  static const usize kMaxFrameBufferSamples{1024};

  // The codec encodes and decodes samples in fixed-size blocks, so we queue up
  // uncompressed and decompressed data until we have blocks large enough to
  // give to the codec.
  short m_EncodeBuffer[kMaxFrameBufferSamples];
  int m_nEncodeBufferSamples;

  IFrameEncoder *frame_encoder_;
  int m_nRawBytes, m_nRawSamples;
  int m_nEncodedBytes;
};

IVoiceCodec *CreateFrameVoiceCodec(IFrameEncoder *frame_encoder) {
  return new FrameVoiceCodec(frame_encoder);
}
