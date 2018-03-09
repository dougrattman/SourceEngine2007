// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef IFRAME_ENCODER_H_
#define IFRAME_ENCODER_H_

#include "tier1/interface.h"

// A frame encoder is a codec that encodes and decodes data in fixed-size
// frames.
the_interface IFrameEncoder {
 public:
  virtual ~IFrameEncoder() {}

  // This is called by VoiceCodec_Frame to see if it can initialize..
  // Fills in the uncompressed and encoded frame size (both are in BYTES).
  virtual bool Init(int quality, int &rawFrameSize, int &encodedFrameSize) = 0;

  virtual void Release() = 0;

  // pUncompressed is 8-bit signed mono sound data with 'rawFrameSize' bytes.
  // pCompressed is the size of encodedFrameSize.
  virtual int EncodeFrame(const char *pUncompressed, char *pCompressed) = 0;

  // pCompressed is encodedFrameSize.
  // pDecompressed is where the 8-bit signed mono samples are stored and has
  // space for 'rawFrameSize' bytes.
  virtual int DecodeFrame(const char *pCompressed, char *pDecompressed) = 0;

  // Some codecs maintain state between Compress and Decompress calls. This
  // should clear that state.
  virtual bool ResetState() = 0;
};

#endif  // IFRAME_ENCODER_H_
