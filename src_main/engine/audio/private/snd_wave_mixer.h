// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SND_WAVE_MIXER_H
#define SND_WAVE_MIXER_H

class IWaveData;
class CAudioMixer;

CAudioMixer *CreateWaveMixer(IWaveData *data, int format, int channels,
                             int bits, int initialStreamPosition);

#endif  // SND_WAVE_MIXER_H
