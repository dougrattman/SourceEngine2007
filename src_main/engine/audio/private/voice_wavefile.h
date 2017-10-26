// Copyright � 1996-2017, Valve Corporation, All rights reserved.

#ifndef VOICE_WAVEFILE_H
#define VOICE_WAVEFILE_H

// Load in a wave file. This isn't very flexible and is only guaranteed to work
// with files saved with WriteWaveFile.
bool ReadWaveFile(const char *pFilename, char *&pData, int &nDataBytes,
                  int &wBitsPerSample, int &nChannels, int &nSamplesPerSec);

// Write out a wave file.
bool WriteWaveFile(const char *pFilename, const char *pData, int nBytes,
                   int wBitsPerSample, int nChannels, int nSamplesPerSec);

#endif  // VOICE_WAVEFILE_H
