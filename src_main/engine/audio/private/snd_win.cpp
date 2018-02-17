// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "audio_pch.h"

#include "tier0/include/memdbgon.h"

bool snd_firsttime = true;

// Global variables. Must be visible to window-procedure function
//  so it can unlock and free the data block after it has been played.
IAudioDevice *g_AudioDevice = nullptr;

void S_BlockSound() {
  if (!g_AudioDevice) return;

  g_AudioDevice->Pause();
}

void S_UnblockSound() {
  if (!g_AudioDevice) return;

  g_AudioDevice->UnPause();
}

// Try to find a sound device to mix for.
// Returns a CAudioNULLDevice if nothing is found.
IAudioDevice *IAudioDevice::AutoDetectInit(bool waveOnly) {
  IAudioDevice *pDevice = nullptr;

  if (waveOnly) {
    pDevice = Audio_CreateWaveDevice();
    if (!pDevice) goto NULLDEVICE;
  }

  if (!pDevice) {
    if (snd_firsttime) {
      pDevice = Audio_CreateDirectSoundDevice();
    }
  }

#if 0
  if (!pDevice) {
    pDevice = Audio_CreateXAudioDevice();
    if (pDevice) {
      // xaudio requires threaded mixing
      S_EnableThreadedMixing(true);
    }
  }
#endif

  // if DirectSound / XAudio2 didn't succeed in initializing, try to initialize
  // waveOut sound, unless DirectSound failed because the hardware is
  // already allocated (in which case the user has already chosen not
  // to have sound)
  // UNDONE: JAY: This doesn't test for the hardware being in use anymore,
  // REVISIT
  if (!pDevice) {
    pDevice = Audio_CreateWaveDevice();
  }

NULLDEVICE:
  snd_firsttime = false;

  if (!pDevice) {
    if (snd_firsttime) DevMsg("No sound device initialized.\n");

    return Audio_GetNullDevice();
  }

  return pDevice;
}

// Reset the sound device for exiting
void SNDDMA_Shutdown() {
  if (g_AudioDevice != Audio_GetNullDevice()) {
    if (g_AudioDevice) {
      g_AudioDevice->Shutdown();
      delete g_AudioDevice;
    }

    // the NULL device is always valid
    g_AudioDevice = Audio_GetNullDevice();
  }
}
