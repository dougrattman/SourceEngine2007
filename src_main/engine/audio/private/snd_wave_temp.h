// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Create an output wave stream.  Used to record audio for in-engine
// movies or mixer debugging.

#ifndef SND_WAVE_TEMP_H
#define SND_WAVE_TEMP_H

extern void WaveCreateTmpFile(const ch *filename, int rate, int bits,
                              int channels);
extern void WaveAppendTmpFile(const ch *filename, void *buffer,
                              int sampleBits, int numSamples);
extern void WaveFixupTmpFile(const ch *filename);

#endif  // SND_WAVE_TEMP_H
