// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "frame_voice_codec.h"
#include "iframe_encoder.h"
#include "ivoicecodec.h"
#include "miles_base_types.h"
#include "quakedef.h"

#include "tier0/include/dbg.h"
#include "tier1/interface.h"

#include "tier0/include/memdbgon.h"

class MilesVoiceCodec : public IFrameEncoder {
 public:
  bool Init(int quality, int &rawFrameSize, int &encodedFrameSize) override {
    Shutdown();

    // This tells what protocol we're using.
    // (.v12, .v24, .v29, or .raw)
    const char *suffix = ".v12";

    // encoder converts from RAW to v12
    if (!asi_encoder_.Init((void *)this, ".RAW", suffix,
                           &MilesVoiceCodec::EncodeStreamCB)) {
      Warning("MilesVoiceCodec: Can't initialize .RAW => %c ASI encoder.\n",
              suffix);
      Shutdown();
      return false;
    }

    // decoder converts from v12 to RAW
    if (!asi_decoder_.Init((void *)this, suffix, ".RAW",
                           &MilesVoiceCodec::DecodeStreamCB)) {
      Warning("MilesVoiceCodec: Can't initialize %c => .RAW ASI decoder.\n",
              suffix);
      Shutdown();
      return false;
    }

    FigureOutFrameSizes();

    // Output.. They'll be using 16-bit samples and we're quantizing to 8-bit.
    rawFrameSize = raw_bytes_count_ * 2;
    encodedFrameSize = encoded_bytes_count_;

    return true;
  }

  void Release() override { delete this; }

  int EncodeFrame(const char *pUncompressedBytes, char *pCompressed) {
    char samples[1024];
    if (!asi_encoder_.IsActive() || raw_bytes_count_ > sizeof(samples))
      return 0;

    const short *pUncompressed = (const short *)pUncompressedBytes;
    for (int i = 0; i < raw_bytes_count_; i++)
      samples[i] = (char)(pUncompressed[i] >> 8);

    destination_ = samples;
    destination_len_ = raw_bytes_count_;
    destination_pos_ = 0;

    int len = asi_encoder_.Process(pCompressed, encoded_bytes_count_);
    if (len != encoded_bytes_count_) {
      Assert(0);
    }

    return len;
  }

  int DecodeFrame(const char *pCompressed, char *pDecompressed) override {
    if (!asi_decoder_.IsActive()) return 0;

    destination_ = pCompressed;
    destination_len_ = encoded_bytes_count_;
    destination_pos_ = 0;

    int outputSize = raw_bytes_count_ * 2;
    int len = asi_decoder_.Process(pDecompressed, (U32)outputSize);

    if (len != outputSize) {
      Assert(0);
    }

    return len;
  }

  bool ResetState() override {
    if (!asi_decoder_.IsActive() || !asi_encoder_.IsActive()) return true;

    for (int i = 0; i < 2; i++) {
      char data[2048], compressed[2048];
      memset(data, 0, sizeof(data));
      destination_ = data;
      destination_len_ = raw_bytes_count_;
      destination_pos_ = 0;

      U32 len = asi_encoder_.Process(compressed, encoded_bytes_count_);
      if (len != (U32)encoded_bytes_count_) {
        Assert(0);
      }

      destination_ = compressed;
      destination_len_ = encoded_bytes_count_;
      destination_pos_ = 0;

      asi_decoder_.Process(data, raw_bytes_count_ * 2);
    }

    // Encode and decode a couple frames of zeros.
    return true;
  }

 private:
  virtual ~MilesVoiceCodec() { Shutdown(); }

  void Shutdown() {
    asi_decoder_.Shutdown();
    asi_encoder_.Shutdown();
  }

  static S32 CALLBACK EncodeStreamCB(
      U32 user,        // User value passed to ASI_open_stream()
      void FAR *dest,  // Location to which stream data should be copied by app
      S32 bytes_requested,  // # of bytes requested by ASI codec
      S32 offset  // If not -1, application should seek to this point in stream
  ) {
    MilesVoiceCodec *pThis = (MilesVoiceCodec *)user;
    Assert(pThis && offset == -1);

    // Figure out how many samples we can safely give it.
    int maxSamples = pThis->destination_len_ - pThis->destination_pos_;
    int samplesToGive = std::min(maxSamples, bytes_requested / 2);

    // Convert to 16-bit signed mono.
    short *pOut = (short *)dest;
    for (int i = 0; i < samplesToGive; i++) {
      pOut[i] = pThis->destination_[pThis->destination_pos_ + i] << 8;
    }

    pThis->destination_pos_ += samplesToGive;
    return samplesToGive * 2;
  }

  static S32 CALLBACK DecodeStreamCB(
      U32 user,        // User value passed to ASI_open_stream()
      void FAR *dest,  // Location to which stream data should be copied by app
      S32 bytes_requested,  // # of bytes requested by ASI codec
      S32 offset  // If not -1, application should seek to this point in stream
  ) {
    MilesVoiceCodec *pThis = (MilesVoiceCodec *)user;
    Assert(pThis && offset == -1);

    int maxBytes = pThis->destination_len_ - pThis->destination_pos_;
    int bytesToGive = std::min(maxBytes, bytes_requested);
    memcpy(dest, &pThis->destination_[pThis->destination_pos_], bytesToGive);

    pThis->destination_pos_ += bytesToGive;
    return bytesToGive;
  }

  void FigureOutFrameSizes() {
    // Figure out the frame sizes. It is probably not prudent in general to
    // assume fixed frame sizes with Miles codecs but it works with the voxware
    // codec right now and simplifies things a lot.
    raw_bytes_count_ =
        (int)asi_encoder_.GetAttribute(asi_encoder_.INPUT_BLOCK_SIZE);

    char uncompressed[1024];
    char compressed[1024];

    Assert(raw_bytes_count_ <= sizeof(uncompressed));

    destination_ = uncompressed;
    destination_len_ = raw_bytes_count_;
    destination_pos_ = 0;

    encoded_bytes_count_ =
        (int)asi_encoder_.Process(compressed, sizeof(compressed));
  }

 private:
  // Encoder, decoder stuff.
  ASISTRUCT asi_encoder_, asi_decoder_;

  // Destination for encoding and decoding.
  const char *destination_;
  int destination_len_, destination_pos_;

  // Frame sizes..
  int raw_bytes_count_, encoded_bytes_count_;
};

void *CreateMilesVoiceCodecFrame() {
  auto *codec = new MilesVoiceCodec;
  return codec ? CreateFrameVoiceCodec(codec) : nullptr;
}

EXPOSE_INTERFACE_FN(CreateMilesVoiceCodecFrame, IVoiceCodec, MILES_VOICE_CODEC)
