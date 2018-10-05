// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "miles_base_types.h"
#include "tier1/interface.h"
#include "vaudio/include/ivaudio.h"

#include "tier0/include/memdbgon.h"

static S32 AILCALLBACK AudioStreamEventCB(U32 user, void FAR *dest,
                                          S32 bytes_requested, S32 offset) {
  IAudioStreamEvent *self = reinterpret_cast<IAudioStreamEvent *>((uintptr_t)user);
  return self->StreamRequestData(dest, bytes_requested, offset);
}

class MilesMp3AudioStream : public IAudioStream {
 public:
  MilesMp3AudioStream(IAudioStreamEvent *event)
      : init_ok_{
            miles_decoder_.Init(event, ".MP3", ".RAW", &AudioStreamEventCB)} {}

  int Decode(void *buffer, unsigned int bufferSize) override {
    return miles_decoder_.Process(buffer, bufferSize);
  }
  int GetOutputBits() override {
    return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_BITS);
  }
  int GetOutputRate() override {
    return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_RATE);
  }
  int GetOutputChannels() override {
    return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_CHANNELS);
  }
  unsigned int GetPosition() override {
    return miles_decoder_.GetAttribute(miles_decoder_.POSITION);
  }
  // NOTE: Only supports seeking forward right now.
  void SetPosition(unsigned int position) override {
    miles_decoder_.Seek(position);
  }

  bool init_ok() const { return init_ok_; }

 private:
  ASISTRUCT miles_decoder_;
  const bool init_ok_;
};

class MilesVAudio : public IVAudio {
 public:
  MilesVAudio() {
    // Assume the user will be creating multiple miles objects, so
    // keep miles running while this exists
    IncrementRefMiles();
  }

  virtual ~MilesVAudio() { DecrementRefMiles(); }

  std::unique_ptr<IAudioStream> CreateMp3StreamDecoder(
      IAudioStreamEvent *event) override {
    auto stream = std::make_unique<MilesMp3AudioStream>(event);
    return stream->init_ok() ? std::move(stream) : nullptr;
  }
};

EXPOSE_INTERFACE(MilesVAudio, IVAudio, VAUDIO_INTERFACE_VERSION);
