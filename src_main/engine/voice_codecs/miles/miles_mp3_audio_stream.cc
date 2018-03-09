// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "miles_base_types.h"
#include "tier1/interface.h"
#include "vaudio/ivaudio.h"

#include "tier0/include/memdbgon.h"

class MilesMp3AudioStream : public IAudioStream {
 public:
  static MilesMp3AudioStream *Create(IAudioStreamEvent *event);

  // IAudioStream functions
  int Decode(void *buffer, unsigned int bufferSize) override;
  int GetOutputBits() override;
  int GetOutputRate() override;
  int GetOutputChannels() override;
  unsigned int GetPosition() override;
  void SetPosition(unsigned int position) override;

 private:
  ASISTRUCT miles_decoder_;

  MilesMp3AudioStream() = default;
  bool Init(IAudioStreamEvent *event);
};

MilesMp3AudioStream *MilesMp3AudioStream::Create(IAudioStreamEvent *event) {
  auto *stream = new MilesMp3AudioStream;
  if (stream->Init(event)) {
    return stream;
  }

  delete stream;
  return nullptr;
}

static S32 AILCALLBACK AudioStreamEventCB(U32 user, void FAR *dest,
                                          S32 bytes_requested, S32 offset) {
  IAudioStreamEvent *self = static_cast<IAudioStreamEvent *>((void *)user);
  return self->StreamRequestData(dest, bytes_requested, offset);
}

bool MilesMp3AudioStream::Init(IAudioStreamEvent *event) {
  return miles_decoder_.Init(event, ".MP3", ".RAW", &AudioStreamEventCB);
}

int MilesMp3AudioStream::Decode(void *pBuffer, unsigned int bufferSize) {
  return miles_decoder_.Process(pBuffer, bufferSize);
}

int MilesMp3AudioStream::GetOutputBits() {
  return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_BITS);
}

int MilesMp3AudioStream::GetOutputRate() {
  return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_RATE);
}

int MilesMp3AudioStream::GetOutputChannels() {
  return miles_decoder_.GetAttribute(miles_decoder_.OUTPUT_CHANNELS);
}

unsigned int MilesMp3AudioStream::GetPosition() {
  return miles_decoder_.GetAttribute(miles_decoder_.POSITION);
}

// NOTE: Only supports seeking forward right now.
void MilesMp3AudioStream::SetPosition(unsigned int position) {
  miles_decoder_.Seek(position);
}

class MilesVAudio : public IVAudio {
 public:
  MilesVAudio() {
    // Assume the user will be creating multiple miles objects, so
    // keep miles running while this exists
    IncrementRefMiles();
  }

  virtual ~MilesVAudio() { DecrementRefMiles(); }

  IAudioStream *CreateMP3StreamDecoder(IAudioStreamEvent *event) override {
    return MilesMp3AudioStream::Create(event);
  }

  void DestroyMP3StreamDecoder(IAudioStream *decoder) override {
    delete decoder;
  }
};

EXPOSE_INTERFACE(MilesVAudio, IVAudio, VAUDIO_INTERFACE_VERSION);
