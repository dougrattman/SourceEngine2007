// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FRAME_VOICE_CODEC_H_
#define FRAME_VOICE_CODEC_H_

#include "audio/public/ivoicecodec.h"
#include "iframe_encoder.h"

extern IVoiceCodec *CreateFrameVoiceCodec(IFrameEncoder *frame_encoder);

#endif  // !FRAME_VOICE_CODEC_H_
