// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <cstdio>

#include "deps/libspeex/include/speex/speex.h"

#include "frame_voice_codec.h"
#include "iframe_encoder.h"
#include "ivoicecodec.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

class SpeexVoiceCodec : public IFrameEncoder {
 public:
  SpeexVoiceCodec()
      : quality_{0},
        speex_encoder_state_{nullptr},
        speex_decoder_state_{nullptr},
        speex_bits_{0} {}

  bool Init(int quality, int &rawFrameSize, int &encodedFrameSize) override {
    bool is_ok = InitStates();
    if (is_ok) {
      rawFrameSize = kRawFrameSize * BYTES_PER_SAMPLE;
      // map general voice quality 1-5 to speex quality levels
      quality_ = GetQuality(quality);
      encodedFrameSize = kEncodedFrameSizes[quality_];
    }

    is_ok =
        !speex_encoder_ctl(speex_encoder_state_, SPEEX_SET_QUALITY, &quality_);
    if (is_ok) {
      is_ok = !speex_decoder_ctl(speex_decoder_state_, SPEEX_SET_QUALITY,
                                 &quality_);
    }

    if (is_ok) {
      int postfilter = 1;  // Set the perceptual enhancement on
      is_ok =
          !speex_decoder_ctl(speex_decoder_state_, SPEEX_SET_ENH, &postfilter);
    }

    if (is_ok) {
      int samplerate = kSampleRate;
      is_ok = !speex_decoder_ctl(speex_decoder_state_, SPEEX_SET_SAMPLING_RATE,
                                 &samplerate);

      if (is_ok) {
        is_ok = !speex_encoder_ctl(speex_encoder_state_,
                                   SPEEX_SET_SAMPLING_RATE, &samplerate);
      }
    }

    return is_ok;
  }

  void Release() override { delete this; }

  int EncodeFrame(const char *pUncompressedBytes, char *pCompressed) override {
    float input[kRawFrameSize];
    short *in = (short *)pUncompressedBytes;

    /*Copy the 16 bits values to float so Speex can work on them*/
    for (usize i = 0; i < kRawFrameSize; i++) {
      input[i] = (float)*in;
      in++;
    }

    /*Flush all the bits in the struct so we can encode a new frame*/
    speex_bits_reset(&speex_bits_);

    /*Encode the frame*/
    speex_encode(speex_encoder_state_, input, &speex_bits_);

    /*Copy the bits to an array of char that can be written*/
    return speex_bits_write(&speex_bits_, pCompressed,
                            kEncodedFrameSizes[quality_]);
  }

  int DecodeFrame(const char *pCompressed, char *pDecompressedBytes) override {
    float output[kRawFrameSize];
    short *out = (short *)pDecompressedBytes;

    /*Copy the data into the bit-stream struct*/
    speex_bits_read_from(&speex_bits_, pCompressed,
                         kEncodedFrameSizes[quality_]);

    /*Decode the data*/
    bool is_ok = !speex_decode(speex_decoder_state_, &speex_bits_, output);

    /*Copy from float to short (16 bits) for output*/
    for (usize i = 0; i < kRawFrameSize; i++) {
      *out = (short)output[i];
      out++;
    }

    return is_ok ? kRawFrameSize : 0;
  }

  bool ResetState() override {
    return !speex_encoder_ctl(speex_encoder_state_, SPEEX_RESET_STATE,
                              nullptr) &&
           !speex_decoder_ctl(speex_decoder_state_, SPEEX_RESET_STATE, nullptr);
  }

 private:
  virtual ~SpeexVoiceCodec() { ReleaseStates(); }

  bool InitStates() {
    speex_bits_init(&speex_bits_);

    // narrow band mode 8kbp
    speex_encoder_state_ = speex_encoder_init(&speex_nb_mode);
    if (!speex_encoder_state_) {
      Warning("SpeexVoiceCodec: Speex encoder init failure, out of memory?\n");
      return false;
    }

    speex_decoder_state_ = speex_decoder_init(&speex_nb_mode);
    if (!speex_decoder_state_) {
      Warning("SpeexVoiceCodec: Speex decoder init failure, out of memory?\n");
      return false;
    }

    return true;
  }

  void ReleaseStates() {
    if (speex_decoder_state_) {
      speex_decoder_destroy(speex_decoder_state_);
      speex_decoder_state_ = nullptr;
    }

    if (speex_encoder_state_) {
      speex_encoder_destroy(speex_encoder_state_);
      speex_encoder_state_ = nullptr;
    }

    speex_bits_destroy(&speex_bits_);
  }

  int GetQuality(int quality) const {
    switch (quality) {
      case 1:
        return 0;
      case 2:
        return 2;
      case 3:
        return 4;
      case 4:
        return 6;
      case 5:
        return 8;
    }

    AssertMsg1(0, "SpeexVoiceCodec: Uknown quality level [1-5]: %d", quality);

    return 0;
  }

  static constexpr usize kSampleRate{8000};   // get 8000 samples/sec
  static constexpr usize kRawFrameSize{160};  // in 160 samples per frame

  // Useful Speex voice qualities are 0,2,4,6 and 8. each quality
  // level has a different encoded frame size and needed bitrate:
  //
  // Quality 0 :  6 bytes/frame,  2400bps
  // Quality 2 : 15 bytes/frame,  6000bps
  // Quality 4 : 20 bytes/frame,  8000bps
  // Quality 6 : 28 bytes/frame, 11200bps
  // Quality 8 : 38 bytes/frame, 15200bps
  //
  // Each quality has a different frame size.
  static const int kEncodedFrameSizes[11];

  int quality_;                // voice codec quality ( 0,2,4,6,8 )
  void *speex_encoder_state_;  // speex internal encoder state.
  void *speex_decoder_state_;  // speex internal decoder state.

  SpeexBits speex_bits_;  // helpful bit buffer structure
};

const int SpeexVoiceCodec::kEncodedFrameSizes[11] = {6,  6,  15, 15, 20, 20,
                                                     28, 28, 38, 38, 38};

void *CreateSpeexFrameVoiceCodec() {
  auto *codec = new SpeexVoiceCodec;
  return codec ? CreateFrameVoiceCodec(codec) : nullptr;
}

EXPOSE_INTERFACE_FN(CreateSpeexFrameVoiceCodec, IVoiceCodec, SPEEX_VOICE_CODEC)
