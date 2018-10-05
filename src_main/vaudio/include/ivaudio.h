// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VAUDIO_INCLUDE_IVAUDIO_H_
#define SOURCE_VAUDIO_INCLUDE_IVAUDIO_H_

#include <memory>
#include "base/include/base_types.h"
#include "tier1/interface.h"

src_interface IAudioStreamEvent {
  // Called by the stream to request more data.  Seeks the source to position,
  // offset -1 indicates previous position.  Copy the data to buffer and return
  // the number of bytes copied.  You may return less than copyCount if the end
  // of the stream is encountered.
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

src_interface IVAudio {
  virtual std::unique_ptr<IAudioStream> CreateMp3StreamDecoder(IAudioStreamEvent * event) = 0;
};

#endif  // SOURCE_VAUDIO_INCLUDE_IVAUDIO_H_
