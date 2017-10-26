// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Helper methods + classes for sound.

#ifndef SOURCE_TIER2_SOUNDUTILS_H_
#define SOURCE_TIER2_SOUNDUTILS_H_

#include "tier2/riff.h"

//-----------------------------------------------------------------------------
// RIFF reader/writers that use the file system
//-----------------------------------------------------------------------------
extern IFileReadBinary *g_pFSIOReadBinary;
extern IFileWriteBinary *g_pFSIOWriteBinary;

//-----------------------------------------------------------------------------
// Returns the duration of a wav file
//-----------------------------------------------------------------------------
float GetWavSoundDuration(const char *pWavFile);

#endif  // SOURCE_TIER2_SOUNDUTILS_H_
