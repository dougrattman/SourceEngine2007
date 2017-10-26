// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SND_WAVE_MIXER_ADPCM_H
#define SND_WAVE_MIXER_ADPCM_H

class CAudioMixer;
class IWaveData;

CAudioMixer *CreateADPCMMixer(IWaveData *data);

#endif  // SND_WAVE_MIXER_ADPCM_H
