// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IVAUDIO_H
#define IVAUDIO_H

#include "base/include/base_types.h"
#include "tier1/interface.h"

the_interface IAudioStreamEvent {
 public:
  // called by the stream to request more data
  // seek the source to position offset
  // -1 indicates previous position
  // copy the data to buffer and return the number of bytes copied
  // you may return less than copyCount if the end of the stream
  // is encountered.
  virtual int StreamRequestData(v * buffer, int copyCount, int offset) = 0;
};

the_interface IAudioStream {
 public:
  // Decode another bufferSize output bytes from the stream
  // returns number of bytes decoded
  virtual int Decode(v * buffer, unsigned int size) = 0;

  // output sampling bits (8/16)
  virtual int GetOutputBits() = 0;
  // output sampling rate in Hz
  virtual int GetOutputRate() = 0;
  // output channels (1=mono,2=stereo)
  virtual int GetOutputChannels() = 0;

  // seek
  virtual unsigned int GetPosition() = 0;

  // NOTE: BUGBUG: Only supports seeking forward currently!
  virtual v SetPosition(unsigned int position) = 0;

  // reset?
};

#define VAUDIO_INTERFACE_VERSION "VAudio002"

the_interface IVAudio {
 public:
  virtual IAudioStream *CreateMP3StreamDecoder(IAudioStreamEvent * event) = 0;
  virtual v DestroyMP3StreamDecoder(IAudioStream * stream) = 0;
};

#endif  // IVAUDIO_H
