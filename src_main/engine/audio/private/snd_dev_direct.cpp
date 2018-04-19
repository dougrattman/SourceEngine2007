// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "audio_pch.h"

#include "snd_dev_direct.h"

#include <dsound.h>
#include <ks.h>
#include <ksmedia.h>
#include "../../sys_dll.h"
#include "avi/ibik.h"
#include "eax.h"
#include "iprediction.h"
#include "tier0/include/icommandline.h"

#include "tier0/include/memdbgon.h"

extern bool snd_firsttime;

extern void DEBUG_StartSoundMeasure(int type, int samplecount);
extern void DEBUG_StopSoundMeasure(int type, int samplecount);

// legacy support
extern ConVar sxroom_off;
extern ConVar sxroom_type;
extern ConVar sxroomwater_type;
extern float sxroom_typeprev;

extern HWND *pmainwindow;

typedef enum { SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL } sndinitstat;

#define SECONDARY_BUFFER_SIZE 0x10000  // output buffer size in bytes
#define SECONDARY_BUFFER_SIZE_SURROUND \
  0x04000  // output buffer size in bytes, one per channel

// hack - need to include latest dsound.h
#undef DSSPEAKER_5POINT1
#undef DSSPEAKER_7POINT1
#define DSSPEAKER_5POINT1 6
#define DSSPEAKER_7POINT1 7

extern void ReleaseSurround();
extern bool MIX_ScaleChannelVolume(paintbuffer_t *ppaint, channel_t *pChannel,
                                   int volume[CCHANVOLUMES], int mixchans);
void OnSndSurroundCvarChanged(IConVar *var, const ch *pOldString,
                              float flOldValue);
void OnSndSurroundLegacyChanged(IConVar *var, const ch *pOldString,
                                float flOldValue);

LPDIRECTSOUND8 pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

GUID IID_IDirectSound3DBufferDef = {
    0x279AFA86,
    0x4981,
    0x11CE,
    {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};

// Purpose: Implementation of direct sound
class CAudioDirectSound : public CAudioDeviceBase {
 public:
  ~CAudioDirectSound();
  bool IsActive() { return true; }
  bool Init();
  void Shutdown();
  void Pause();
  void UnPause();
  float MixDryVolume();
  bool Should3DMix();
  void StopAllSounds();

  int PaintBegin(float mixAheadTime, int soundtime, int paintedtime);
  void PaintEnd();

  int GetOutputPosition();
  void ClearBuffer();
  void UpdateListener(const Vector &position, const Vector &forward,
                      const Vector &right, const Vector &up);

  void ChannelReset(int entnum, int channelIndex, float distanceMod);
  void TransferSamples(int end);

  const ch *DeviceName();
  int DeviceChannels() { return m_deviceChannels; }
  int DeviceSampleBits() { return m_deviceSampleBits; }
  int DeviceSampleBytes() { return m_deviceSampleBits / 8; }
  int DeviceDmaSpeed() { return m_deviceDmaSpeed; }
  int DeviceSampleCount() { return m_deviceSampleCount; }

  bool IsInterleaved() { return m_isInterleaved; }

  // Singleton object
  static CAudioDirectSound *m_pSingleton;

 private:
  void DetectWindowsSpeakerSetup();
  bool LockDSBuffer(LPDIRECTSOUNDBUFFER pBuffer, DWORD **pdwWriteBuffer,
                    DWORD *pdwSizeBuffer, const ch *pBufferName,
                    int lockFlags = 0);
  bool IsUsingBufferPerSpeaker();

  sndinitstat SNDDMA_InitDirect();
  bool SNDDMA_InitInterleaved(LPDIRECTSOUND8 lpDS, WAVEFORMATEX *lpFormat,
                              int channelCount);
  bool SNDDMA_InitSurround(LPDIRECTSOUND8 lpDS, WAVEFORMATEX *lpFormat,
                           DSBCAPS *lpdsbc, int cchan);
  void S_TransferSurround16(portable_samplepair_t *pfront,
                            portable_samplepair_t *prear,
                            portable_samplepair_t *pcenter, int lpaintedtime,
                            int endtime, int cchan);
  void S_TransferSurround16Interleaved(const portable_samplepair_t *pfront,
                                       const portable_samplepair_t *prear,
                                       const portable_samplepair_t *pcenter,
                                       int lpaintedtime, int endtime);
  void S_TransferSurround16Interleaved_FullLock(
      const portable_samplepair_t *pfront, const portable_samplepair_t *prear,
      const portable_samplepair_t *pcenter, int lpaintedtime, int endtime);

  int m_deviceChannels;  // channels per hardware output buffer (1 for quad/5.1,
                         // 2 for stereo)
  int m_deviceSampleBits;   // bits per sample (16)
  int m_deviceSampleCount;  // count of mono samples in output buffer
  int m_deviceDmaSpeed;     // samples per second per output buffer
  int m_bufferSizeBytes;    // size of a single hardware output buffer, in bytes

  // output buffer playback starting uint8_t offset
  DWORD m_outputBufferStartOffset;
  HMODULE m_direct_sound_module;
  bool m_isInterleaved;
};

CAudioDirectSound *CAudioDirectSound::m_pSingleton = nullptr;

LPDIRECTSOUNDBUFFER pDSBufFL = nullptr;
LPDIRECTSOUNDBUFFER pDSBufFR = nullptr;
LPDIRECTSOUNDBUFFER pDSBufRL = nullptr;
LPDIRECTSOUNDBUFFER pDSBufRR = nullptr;
LPDIRECTSOUNDBUFFER pDSBufFC = nullptr;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFL = nullptr;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFR = nullptr;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DRL = nullptr;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DRR = nullptr;
LPDIRECTSOUND3DBUFFER8 pDSBuf3DFC = nullptr;

// Helpers.

CAudioDirectSound::~CAudioDirectSound() { m_pSingleton = nullptr; }

bool CAudioDirectSound::Init() {
  m_direct_sound_module = nullptr;

  static bool is_first_time_init{true};

  if (is_first_time_init) {
    snd_surround.InstallChangeCallback(&OnSndSurroundCvarChanged);
    snd_legacy_surround.InstallChangeCallback(&OnSndSurroundLegacyChanged);
    is_first_time_init = false;
  }

  if (SNDDMA_InitDirect() == SIS_SUCCESS) {
    // Tells Bink to use DirectSound for its audio decoding.
    if (!bik->SetDirectSoundDevice(pDS)) {
      AssertMsg(false, "Bink can't use DirectSound8 device.");
    }

    return true;
  }

  return false;
}

void CAudioDirectSound::Shutdown() {
  ReleaseSurround();

  if (pDSBuf) {
    pDSBuf->Stop();
    pDSBuf->Release();
  }

  // only release primary buffer if it's not also the mixing buffer we just
  // released
  if (pDSPBuf && (pDSBuf != pDSPBuf)) {
    pDSPBuf->Release();
  }

  if (pDS) {
    pDS->SetCooperativeLevel(*pmainwindow, DSSCL_NORMAL);
    pDS->Release();
  }

  pDS = nullptr;
  pDSBuf = nullptr;
  pDSPBuf = nullptr;

  if (m_direct_sound_module) {
    FreeLibrary(m_direct_sound_module);
    m_direct_sound_module = nullptr;
  }

  if (this == CAudioDirectSound::m_pSingleton) {
    CAudioDirectSound::m_pSingleton = nullptr;
  }
}

// Total number of samples that have played out to hardware
// for current output buffer (ie: from buffer offset start).
// return playback position within output playback buffer:
// the output units are dependant on the device channels
// so the ouput units for a 2 channel device are as 16 bit LR pairs
// and the output unit for a 1 channel device are as 16 bit mono samples.
// take into account the original start position within the buffer, and
// calculate difference between current position (with buffer wrap) and
// start position.
int CAudioDirectSound::GetOutputPosition() {
  int samp16;
  int start, current;
  DWORD dwCurrent;

  // get size in bytes of output buffer
  const int size_bytes = m_bufferSizeBytes;
  if (IsUsingBufferPerSpeaker()) {
    // mono output buffers
    // get uint8_t offset of playback cursor in Front Left output buffer
    pDSBufFL->GetCurrentPosition(&dwCurrent, NULL);

    start = (int)m_outputBufferStartOffset;
    current = (int)dwCurrent;
  } else {
    // multi-channel interleavd output buffer
    // get uint8_t offset of playback cursor in output buffer
    pDSBuf->GetCurrentPosition(&dwCurrent, NULL);

    start = (int)m_outputBufferStartOffset;
    current = (int)dwCurrent;
  }

  // get 16 bit samples played, relative to buffer starting offset
  if (current > start) {
    // get difference & convert to 16 bit mono samples
    samp16 = (current - start) >> SAMPLE_16BIT_SHIFT;
  } else {
    // get difference (with buffer wrap) convert to 16 bit mono samples
    samp16 = ((size_bytes - start) + current) >> SAMPLE_16BIT_SHIFT;
  }

  int outputPosition = samp16 / DeviceChannels();

  return outputPosition;
}

void CAudioDirectSound::Pause() {
  if (pDSBuf) {
    pDSBuf->Stop();
  }

  if (pDSBufFL) pDSBufFL->Stop();
  if (pDSBufFR) pDSBufFR->Stop();
  if (pDSBufRL) pDSBufRL->Stop();
  if (pDSBufRR) pDSBufRR->Stop();
  if (pDSBufFC) pDSBufFC->Stop();
}

void CAudioDirectSound::UnPause() {
  if (pDSBuf) pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

  if (pDSBufFL) pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);
  if (pDSBufFR) pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);
  if (pDSBufRL) pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);
  if (pDSBufRR) pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);
  if (pDSBufFC) pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);
}

float CAudioDirectSound::MixDryVolume() { return 0; }

bool CAudioDirectSound::Should3DMix() {
  if (m_bSurround) return true;
  return false;
}

IAudioDevice *Audio_CreateDirectSoundDevice() {
  if (!CAudioDirectSound::m_pSingleton)
    CAudioDirectSound::m_pSingleton = new CAudioDirectSound;

  if (CAudioDirectSound::m_pSingleton->Init()) {
    if (snd_firsttime) DevMsg("DirectSound initialized\n");

    return CAudioDirectSound::m_pSingleton;
  }

  DevMsg("DirectSound failed to init\n");

  delete CAudioDirectSound::m_pSingleton;
  CAudioDirectSound::m_pSingleton = NULL;

  return NULL;
}

int CAudioDirectSound::PaintBegin(float mixAheadTime, int soundtime,
                                  int lpaintedtime) {
  //  soundtime - total full samples that have been played out to hardware at
  //  dmaspeed paintedtime - total full samples that have been mixed at speed
  //  endtime - target for full samples in mixahead buffer at speed
  //  samps - size of output buffer in full samples

  int mixaheadtime = mixAheadTime * DeviceDmaSpeed();
  int endtime = soundtime + mixaheadtime;

  if (endtime <= lpaintedtime) return endtime;

  int fullsamps = DeviceSampleCount() / DeviceChannels();

  if ((endtime - soundtime) > fullsamps) endtime = soundtime + fullsamps;

  if ((endtime - lpaintedtime) & 0x3) {
    // The difference between endtime and painted time should align on
    // boundaries of 4 samples.  This is important when upsampling from 11khz ->
    // 44khz.
    endtime -= (endtime - lpaintedtime) & 0x3;
  }

  DWORD dwStatus;

  // If using surround, there are 4 or 5 different buffers being used and the
  // pDSBuf is NULL.
  if (IsUsingBufferPerSpeaker()) {
    if (pDSBufFL->GetStatus(&dwStatus) != DS_OK)
      Msg("Couldn't get SURROUND FL sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBufFL->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);

    if (pDSBufFR->GetStatus(&dwStatus) != DS_OK)
      Msg("Couldn't get SURROUND FR sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBufFR->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);

    if (pDSBufRL->GetStatus(&dwStatus) != DS_OK)
      Msg("Couldn't get SURROUND RL sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBufRL->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);

    if (pDSBufRR->GetStatus(&dwStatus) != DS_OK)
      Msg("Couldn't get SURROUND RR sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBufRR->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);

    if (m_bSurroundCenter) {
      if (pDSBufFC->GetStatus(&dwStatus) != DS_OK)
        Msg("Couldn't get SURROUND FC sound buffer status\n");

      if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBufFC->Restore();

      if (!(dwStatus & DSBSTATUS_PLAYING))
        pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);
    }
  } else if (pDSBuf) {
    if (pDSBuf->GetStatus(&dwStatus) != DS_OK)
      Msg("Couldn't get sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBuf->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
  }

  return endtime;
}

void CAudioDirectSound::PaintEnd() {}

void CAudioDirectSound::ClearBuffer() {
  int clear;

  DWORD dwSizeFL, dwSizeFR, dwSizeRL, dwSizeRR, dwSizeFC;
  ch *pDataFL, *pDataFR, *pDataRL, *pDataRR, *pDataFC;

  dwSizeFC = 0;  // compiler warning
  pDataFC = NULL;

  if (IsUsingBufferPerSpeaker()) {
    int SURROUNDreps;
    HRESULT SURROUNDhresult;
    SURROUNDreps = 0;

    if (!pDSBufFL && !pDSBufFR && !pDSBufRL && !pDSBufRR && !pDSBufFC) return;

    while ((SURROUNDhresult =
                pDSBufFL->Lock(0, m_bufferSizeBytes, (void **)&pDataFL,
                               &dwSizeFL, NULL, NULL, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("S_ClearBuffer: DS::Lock FL Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("S_ClearBuffer: DS: couldn't restore FL buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult =
                pDSBufFR->Lock(0, m_bufferSizeBytes, (void **)&pDataFR,
                               &dwSizeFR, NULL, NULL, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("S_ClearBuffer: DS::Lock FR Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("S_ClearBuffer: DS: couldn't restore FR buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult =
                pDSBufRL->Lock(0, m_bufferSizeBytes, (void **)&pDataRL,
                               &dwSizeRL, NULL, NULL, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("S_ClearBuffer: DS::Lock RL Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("S_ClearBuffer: DS: couldn't restore RL buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult =
                pDSBufRR->Lock(0, m_bufferSizeBytes, (void **)&pDataRR,
                               &dwSizeRR, NULL, NULL, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("S_ClearBuffer: DS::Lock RR Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("S_ClearBuffer: DS: couldn't restore RR buffer\n");
        S_Shutdown();
        return;
      }
    }

    if (m_bSurroundCenter) {
      SURROUNDreps = 0;
      while ((SURROUNDhresult =
                  pDSBufFC->Lock(0, m_bufferSizeBytes, (void **)&pDataFC,
                                 &dwSizeFC, NULL, NULL, 0)) != DS_OK) {
        if (SURROUNDhresult != DSERR_BUFFERLOST) {
          Msg("S_ClearBuffer: DS::Lock FC Sound Buffer Failed\n");
          S_Shutdown();
          return;
        }

        if (++SURROUNDreps > 10000) {
          Msg("S_ClearBuffer: DS: couldn't restore FC buffer\n");
          S_Shutdown();
          return;
        }
      }
    }

    Q_memset(pDataFL, 0, m_bufferSizeBytes);
    Q_memset(pDataFR, 0, m_bufferSizeBytes);
    Q_memset(pDataRL, 0, m_bufferSizeBytes);
    Q_memset(pDataRR, 0, m_bufferSizeBytes);

    if (m_bSurroundCenter) Q_memset(pDataFC, 0, m_bufferSizeBytes);

    pDSBufFL->Unlock(pDataFL, dwSizeFL, NULL, 0);
    pDSBufFR->Unlock(pDataFR, dwSizeFR, NULL, 0);
    pDSBufRL->Unlock(pDataRL, dwSizeRL, NULL, 0);
    pDSBufRR->Unlock(pDataRR, dwSizeRR, NULL, 0);

    if (m_bSurroundCenter) pDSBufFC->Unlock(pDataFC, dwSizeFC, NULL, 0);

    return;
  }

  if (!pDSBuf) return;

  if (DeviceSampleBits() == 8)
    clear = 0x80;
  else
    clear = 0;

  if (pDSBuf) {
    DWORD dwSize;
    DWORD *pData;
    int reps;
    HRESULT hresult;

    reps = 0;
    while ((hresult = pDSBuf->Lock(0, m_bufferSizeBytes, (void **)&pData,
                                   &dwSize, NULL, NULL, 0)) != DS_OK) {
      if (hresult != DSERR_BUFFERLOST) {
        Msg("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++reps > 10000) {
        Msg("S_ClearBuffer: DS: couldn't restore buffer\n");
        S_Shutdown();
        return;
      }
    }

    Q_memset(pData, clear, dwSize);

    pDSBuf->Unlock(pData, dwSize, NULL, 0);
  }
}

void CAudioDirectSound::StopAllSounds() {}

bool CAudioDirectSound::SNDDMA_InitInterleaved(LPDIRECTSOUND8 lpDS,
                                               WAVEFORMATEX *lpFormat,
                                               int channelCount) {
  WAVEFORMATEXTENSIBLE wfx = {0};  // DirectSoundBuffer wave format (extensible)

  // set the channel mask and number of channels based on the command line
  // parameter
  if (channelCount == 2) {
    wfx.Format.nChannels = 2;
    wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
  } else if (channelCount == 4) {
    wfx.Format.nChannels = 4;
    wfx.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
  } else if (channelCount == 6) {
    wfx.Format.nChannels = 6;
    wfx.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
  } else {
    return false;
  }

  // setup the extensible structure
  wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  // wfx.Format.nChannels              = SET ABOVE
  wfx.Format.nSamplesPerSec = lpFormat->nSamplesPerSec;
  wfx.Format.wBitsPerSample = lpFormat->wBitsPerSample;
  wfx.Format.nBlockAlign = wfx.Format.wBitsPerSample / 8 * wfx.Format.nChannels;
  wfx.Format.nAvgBytesPerSec =
      wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
  wfx.Format.cbSize = 22;  // size from after this to end of extensible struct.
                           // sizeof(WORD + DWORD + GUID)
  wfx.Samples.wValidBitsPerSample = lpFormat->wBitsPerSample;
  // wfx.dwChannelMask                 = SET ABOVE BASED ON COMMAND LINE
  // PARAMETERS
  wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  // setup the DirectSound
  // DirectSoundBuffer descriptor
  DSBUFFERDESC dsbdesc = {sizeof(DSBUFFERDESC)};
  dsbdesc.dwFlags = DSBCAPS_LOCHARDWARE;

  if (IsWindowsVistaOrGreater()) {
    // vista doesn't support hardware buffers, but does support surround on
    // software (XP does not)
    dsbdesc.dwFlags = DSBCAPS_LOCSOFTWARE;
  }

  dsbdesc.dwBufferBytes = SECONDARY_BUFFER_SIZE_SURROUND * channelCount;
  dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wfx;

  HRESULT hr = lpDS->CreateSoundBuffer(&dsbdesc, &pDSBuf, nullptr);
  if (FAILED(hr)) {
    Msg("DS::Can't create interleaved sound buffer (0x%.8x).\n", hr);
    return false;
  }

  DWORD dwSize = 0, dwWrite;
  DWORD *pBuffer = 0;
  if (!LockDSBuffer(pDSBuf, &pBuffer, &dwSize, "DS_INTERLEAVED",
                    DSBLOCK_ENTIREBUFFER))
    return false;

  m_deviceChannels = wfx.Format.nChannels;
  m_deviceSampleBits = wfx.Format.wBitsPerSample;
  m_deviceDmaSpeed = wfx.Format.nSamplesPerSec;
  m_bufferSizeBytes = dsbdesc.dwBufferBytes;
  m_isInterleaved = true;

  Q_memset(pBuffer, 0, dwSize);

  hr = pDSBuf->Unlock(pBuffer, dwSize, NULL, 0);
  if (FAILED(hr)) {
    Warning("DS::Can't unlock interleaved sound buffer (0x%.8x).\n", hr);
  }

  // Make sure mixer is active (this was moved after the zeroing to avoid
  // popping on startup -- at least when using the dx9.0b debug .dlls)
  hr = pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
  if (FAILED(hr)) {
    Warning("DS::Can't play interleaved sound buffer (0x%.8x).\n", hr);
  }

  hr = pDSBuf->Stop();
  if (FAILED(hr)) {
    Warning("DS::Can't stop interleaved sound buffer (0x%.8x).\n", hr);
  }

  hr = pDSBuf->GetCurrentPosition(&m_outputBufferStartOffset, &dwWrite);
  if (FAILED(hr)) {
    Warning(
        "DS::Can't get current position interleaved sound buffer (0x%.8x).\n",
        hr);
  }

  hr = pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
  if (FAILED(hr)) {
    Warning("DS::Can't play interleaved sound buffer (0x%.8x).\n", hr);
  }

  return true;
}

// Direct-Sound support
sndinitstat CAudioDirectSound::SNDDMA_InitDirect() {
  void *lpData = NULL;
  bool primary_format_set = false;
  int pri_channels = 2;

  using DirectSoundCreate8Fn = HRESULT(WINAPI *)(
      LPCGUID pcGuidDevice, LPDIRECTSOUND8 * ppDS8, LPUNKNOWN pUnkOuter);
  DirectSoundCreate8Fn ds_create_8_fn = nullptr;

  if (!m_direct_sound_module) {
    m_direct_sound_module =
        LoadLibraryExW(L"dsound.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (m_direct_sound_module == nullptr) {
      Warning("Couldn't load dsound.dll (0x%.8x).\n", GetLastError());
      return SIS_FAILURE;
    }

    ds_create_8_fn = reinterpret_cast<decltype(ds_create_8_fn)>(
        GetProcAddress(m_direct_sound_module, "DirectSoundCreate8"));
    if (!ds_create_8_fn) {
      Warning("Couldn't find DirectSoundCreate8 in dsound.dll (0x%.8x).\n",
              GetLastError());
      return SIS_FAILURE;
    }
  }

  HRESULT hr = (*ds_create_8_fn)(nullptr, &pDS, nullptr);
  if (hr != DS_OK) {
    if (hr != DSERR_ALLOCATED) {
      DevMsg("DirectSoundCreate8 failed to create DirectSound8 (0x%.8x).\n",
             hr);
      return SIS_FAILURE;
    }

    return SIS_NOTAVAIL;
  }

  // get snd_surround value from window settings
  DetectWindowsSpeakerSetup();

  m_bSurround = false;
  m_bSurroundCenter = false;
  m_bHeadphone = false;
  m_isInterleaved = false;

  switch (snd_surround.GetInt()) {
    case 0:
      m_bHeadphone = true;  // stereo headphone
      pri_channels = 2;     // primary buffer mixes stereo input data
      break;
    default:
    case 2:
      pri_channels = 2;  // primary buffer mixes stereo input data
      break;             // no surround
    case 4:
      m_bSurround = true;  // quad surround
      pri_channels = 1;    // primary buffer mixes 3d mono input data
      break;
    case 5:
    case 7:
      m_bSurround = true;  // 5.1 surround
      m_bSurroundCenter = true;
      pri_channels = 1;  // primary buffer mixes 3d mono input data
      break;
  }

  // secondary buffers should have same # channels as primary
  m_deviceChannels = pri_channels;
  m_deviceSampleBits = 16;             // hardware bits per sample
  m_deviceDmaSpeed = SOUND_DMA_SPEED;  // hardware playback rate

  WAVEFORMATEX format;
  Q_memset(&format, 0, sizeof(format));
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = pri_channels;
  format.wBitsPerSample = m_deviceSampleBits;
  format.nSamplesPerSec = m_deviceDmaSpeed;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.cbSize = 0;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

  DSCAPS dscaps = {sizeof(dscaps)};
  hr = pDS->GetCaps(&dscaps);

  if (DS_OK != hr) {
    Warning("Couldn't get DirectSound8 caps (0x%.8x).\n", hr);
    Shutdown();
    return SIS_FAILURE;
  }

  if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
    Warning("No DirectSound8 driver installed.\n");
    Shutdown();
    return SIS_FAILURE;
  }

  hr = pDS->SetCooperativeLevel(*pmainwindow, DSSCL_EXCLUSIVE);

  if (DS_OK != hr) {
    Warning(
        "Set DirectSound8 cooperative level to exclusive failed (0x%.8x).\n",
        hr);
    Shutdown();
    return SIS_FAILURE;
  }

  // get access to the primary buffer, if possible, so we can set the
  // sound hardware format
  DSBUFFERDESC primary_buffer = {sizeof(DSBUFFERDESC)};
  primary_buffer.dwFlags = DSBCAPS_PRIMARYBUFFER;

  if (snd_legacy_surround.GetBool() || m_bSurround) {
    primary_buffer.dwFlags |= DSBCAPS_CTRL3D;
  }

  primary_buffer.dwBufferBytes = 0;
  primary_buffer.lpwfxFormat = nullptr;

  DSBCAPS base_capabilities = {sizeof(base_capabilities)};

  if (!CommandLine()->CheckParm("-snoforceformat")) {
    hr = pDS->CreateSoundBuffer(&primary_buffer, &pDSPBuf, nullptr);

    if (DS_OK == hr) {
      WAVEFORMATEX pformat = format;

      hr = pDSPBuf->SetFormat(&pformat);

      if (DS_OK != hr) {
        if (snd_firsttime)
          DevMsg("Set primary DirectSound8 buffer format: no (0x%.8x).\n", hr);
      } else {
        if (snd_firsttime)
          DevMsg("Set primary DirectSound8 buffer format: yes\n");

        primary_format_set = true;
      }
    } else {
      Warning("Create primary DirectSound8 buffer failed (0x%.8x).\n", hr);
    }
  }

  if (m_bSurround) {
    // try to init surround
    m_bSurround = false;
    if (snd_legacy_surround.GetBool()) {
      if (snd_surround.GetInt() == 4) {
        // attempt to init 4 channel surround
        m_bSurround = SNDDMA_InitSurround(pDS, &format, &base_capabilities, 4);
      } else if (snd_surround.GetInt() == 5 || snd_surround.GetInt() == 7) {
        // attempt to init 5 channel surround
        m_bSurroundCenter =
            SNDDMA_InitSurround(pDS, &format, &base_capabilities, 5);
        m_bSurround = m_bSurroundCenter;
      }
    }
    if (!m_bSurround) {
      pri_channels = 6;
      if (snd_surround.GetInt() < 5) {
        pri_channels = 4;
      }

      m_bSurround = SNDDMA_InitInterleaved(pDS, &format, pri_channels);
    }
  }

  if (!m_bSurround) {
    // snd_surround.SetValue( 0 );
    if (!primary_format_set || !CommandLine()->CheckParm("-primarysound")) {
      // create the secondary buffer we'll actually work with
      Q_memset(&primary_buffer, 0, sizeof(primary_buffer));
      primary_buffer.dwSize = sizeof(DSBUFFERDESC);
      // NOTE: don't use CTRLFREQUENCY (slow)
      primary_buffer.dwFlags = DSBCAPS_LOCSOFTWARE;
      primary_buffer.dwBufferBytes = SECONDARY_BUFFER_SIZE;
      primary_buffer.lpwfxFormat = &format;

      hr = pDS->CreateSoundBuffer(&primary_buffer, &pDSBuf, nullptr);

      if (DS_OK != hr) {
        Warning("Create secondary DirectSound8 buffer failed (0x%.8x).\n", hr);
        Shutdown();
        return SIS_FAILURE;
      }

      m_deviceChannels = format.nChannels;
      m_deviceSampleBits = format.wBitsPerSample;
      m_deviceDmaSpeed = format.nSamplesPerSec;

      Q_memset(&base_capabilities, 0, sizeof(base_capabilities));
      base_capabilities.dwSize = sizeof(base_capabilities);

      if (DS_OK != pDSBuf->GetCaps(&base_capabilities)) {
        Warning("DS:GetCaps failed\n");
        Shutdown();
        return SIS_FAILURE;
      }

      if (snd_firsttime) DevMsg("Using secondary sound buffer\n");
    } else {
      if (DS_OK != pDS->SetCooperativeLevel(*pmainwindow, DSSCL_WRITEPRIMARY)) {
        Warning("Set coop level failed\n");
        Shutdown();
        return SIS_FAILURE;
      }

      Q_memset(&base_capabilities, 0, sizeof(base_capabilities));
      base_capabilities.dwSize = sizeof(base_capabilities);
      if (DS_OK != pDSPBuf->GetCaps(&base_capabilities)) {
        Msg("DS:GetCaps failed\n");
        return SIS_FAILURE;
      }

      pDSBuf = pDSPBuf;
      DevMsg("Using primary sound buffer\n");
    }

    if (snd_firsttime) {
      DevMsg(
          "   %d channel(s)\n"
          "   %d bits/sample\n"
          "   %d samples/sec\n",
          DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed());
    }

    DWORD dwSize, dwWrite;

    // initialize the buffer
    m_bufferSizeBytes = base_capabilities.dwBufferBytes;
    int reps = 0;
    while ((hr = pDSBuf->Lock(0, m_bufferSizeBytes, (void **)&lpData, &dwSize,
                              NULL, NULL, 0)) != DS_OK) {
      if (hr != DSERR_BUFFERLOST) {
        Warning("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
        Shutdown();
        return SIS_FAILURE;
      }

      if (++reps > 10000) {
        Warning("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
        Shutdown();
        return SIS_FAILURE;
      }
    }

    Q_memset(lpData, 0, dwSize);
    pDSBuf->Unlock(lpData, dwSize, NULL, 0);

    // Make sure mixer is active (this was moved after the zeroing to avoid
    // popping on startup -- at least when using the dx9.0b debug .dlls)
    pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

    // we don't want anyone to access the buffer directly w/o locking it first.
    lpData = NULL;

    pDSBuf->Stop();
    pDSBuf->GetCurrentPosition(&m_outputBufferStartOffset, &dwWrite);
    pDSBuf->Play(0, 0, DSBPLAY_LOOPING);
  }

  // number of mono samples output buffer may hold
  m_deviceSampleCount = m_bufferSizeBytes / (DeviceSampleBytes());

  return SIS_SUCCESS;
}

// Sets the snd_surround_speakers cvar based on the windows setting
void CAudioDirectSound::DetectWindowsSpeakerSetup() {
  // detect speaker settings from windows
  DWORD speaker_config = DSSPEAKER_STEREO, speaker_geometry = 0;

  if (DS_OK == pDS->GetSpeakerConfig(&speaker_config)) {
    // split out settings
    speaker_geometry = DSSPEAKER_GEOMETRY(speaker_config);
    speaker_config = DSSPEAKER_CONFIG(speaker_config);

    // DEBUG
    if (speaker_config == DSSPEAKER_MONO)
      DevMsg("DS:mono configuration detected\n");

    if (speaker_config == DSSPEAKER_HEADPHONE)
      DevMsg("DS:headphone configuration detected\n");

    if (speaker_config == DSSPEAKER_STEREO)
      DevMsg("DS:stereo speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_QUAD)
      DevMsg("DS:quad speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_SURROUND)
      DevMsg("DS:surround speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_5POINT1)
      DevMsg("DS:5.1 speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_7POINT1)
      DevMsg("DS:7.1 speaker configuration detected\n");

    // set the cvar to be the windows setting
    switch (speaker_config) {
      case DSSPEAKER_HEADPHONE:
        snd_surround.SetValue(0);
        break;

      case DSSPEAKER_MONO:
      case DSSPEAKER_STEREO:
      default:
        snd_surround.SetValue(2);
        break;

      case DSSPEAKER_QUAD:
        snd_surround.SetValue(4);
        break;

      case DSSPEAKER_5POINT1:
        snd_surround.SetValue(5);
        break;

      case DSSPEAKER_7POINT1:
        snd_surround.SetValue(7);
        break;
    }
  }
}

/*
 Updates windows settings based on snd_surround_speakers cvar changing
 This should only happen if the user has changed it via the console or the UI
 Changes won't take effect until the engine has restarted
*/
void OnSndSurroundCvarChanged(IConVar *pVar, const ch *pOldString,
                              float flOldValue) {
  if (!pDS) return;

  // get the user's previous speaker config
  DWORD speaker_config = DSSPEAKER_STEREO;
  if (DS_OK == pDS->GetSpeakerConfig(&speaker_config)) {
    // remove geometry setting
    speaker_config = DSSPEAKER_CONFIG(speaker_config);
  }

  // get the new config
  DWORD newSpeakerConfig = 0;
  const ch *speakerConfigDesc = "";

  ConVarRef var(pVar);
  switch (var.GetInt()) {
    case 0:
      newSpeakerConfig = DSSPEAKER_HEADPHONE;
      speakerConfigDesc = "headphone";
      break;

    case 2:
    default:
      newSpeakerConfig = DSSPEAKER_STEREO;
      speakerConfigDesc = "stereo speaker";
      break;

    case 4:
      newSpeakerConfig = DSSPEAKER_QUAD;
      speakerConfigDesc = "quad speaker";
      break;

    case 5:
      newSpeakerConfig = DSSPEAKER_5POINT1;
      speakerConfigDesc = "5.1 speaker";
      break;

    case 7:
      newSpeakerConfig = DSSPEAKER_7POINT1;
      speakerConfigDesc = "7.1 speaker";
      break;
  }

  // make sure the config has changed
  if (newSpeakerConfig == speaker_config) return;

  // set new configuration
  pDS->SetSpeakerConfig(DSSPEAKER_COMBINED(newSpeakerConfig, 0));

  Msg("Speaker configuration has been changed to %s.\n", speakerConfigDesc);

  // restart sound system so it takes effect
  g_pSoundServices->RestartSoundSystem();
}

void OnSndSurroundLegacyChanged(IConVar *pVar, const ch *pOldString,
                                float flOldValue) {
  if (pDS && CAudioDirectSound::m_pSingleton) {
    ConVarRef var(pVar);
    // should either be interleaved or have legacy surround set, not both
    if (CAudioDirectSound::m_pSingleton->IsInterleaved() == var.GetBool()) {
      Msg("Legacy Surround %s.\n", var.GetBool() ? "enabled" : "disabled");
      // restart sound system so it takes effect
      g_pSoundServices->RestartSoundSystem();
    }
  }
}

/*
 Release all Surround buffer pointers
*/
void ReleaseSurround() {
  if (pDSBuf3DFL != NULL) {
    pDSBuf3DFL->Release();
    pDSBuf3DFL = NULL;
  }

  if (pDSBuf3DFR != NULL) {
    pDSBuf3DFR->Release();
    pDSBuf3DFR = NULL;
  }

  if (pDSBuf3DRL != NULL) {
    pDSBuf3DRL->Release();
    pDSBuf3DRL = NULL;
  }

  if (pDSBuf3DRR != NULL) {
    pDSBuf3DRR->Release();
    pDSBuf3DRR = NULL;
  }

  if (pDSBufFL != NULL) {
    pDSBufFL->Release();
    pDSBufFL = NULL;
  }

  if (pDSBufFR != NULL) {
    pDSBufFR->Release();
    pDSBufFR = NULL;
  }

  if (pDSBufRL != NULL) {
    pDSBufRL->Release();
    pDSBufRL = NULL;
  }

  if (pDSBufRR != NULL) {
    pDSBufRR->Release();
    pDSBufRR = NULL;
  }

  if (pDSBufFC != NULL) {
    pDSBufFC->Release();
    pDSBufFC = NULL;
  }
}

void DEBUG_DS_FillSquare(void *lpData, DWORD dwSize) {
  short *lpshort = (short *)lpData;
  DWORD j = std::min(10000UL, dwSize / 2);

  for (DWORD i = 0; i < j; i++) lpshort[i] = 8000;
}

void DEBUG_DS_FillSquare2(void *lpData, DWORD dwSize) {
  short *lpshort = (short *)lpData;
  DWORD j = std::min(1000UL, dwSize / 2);

  for (DWORD i = 0; i < j; i++) lpshort[i] = 16000;
}

// helper to set default buffer params
void DS3D_SetBufferParams(LPDIRECTSOUND3DBUFFER8 pDSBuf3D, D3DVECTOR *pbpos,
                          D3DVECTOR *pbdir) {
  D3DVECTOR velocity = {0.0f, 0.0f, 0.0f};
  D3DVECTOR position = *pbpos, bdir = *pbdir;
  DS3DBUFFER buffer = {sizeof(DS3DBUFFER)};

  HRESULT hr = pDSBuf3D->GetAllParameters(&buffer);
  if (DS_OK != hr) {
    Warning("DS:3DBuffer get all parameters failed (0x%.8x).\n", hr);
  }

  buffer.vPosition = position;
  buffer.vVelocity = velocity;
  buffer.dwInsideConeAngle = 5.0;  // narrow cones for each speaker
  buffer.dwOutsideConeAngle = 10.0;
  buffer.vConeOrientation = bdir;
  buffer.lConeOutsideVolume = DSBVOLUME_MIN;
  buffer.flMinDistance = 100.0;  // no rolloff (until > 2.0 meter distance)
  buffer.flMaxDistance = DS3D_DEFAULTMAXDISTANCE;
  buffer.dwMode = DS3DMODE_NORMAL;

  hr = pDSBuf3D->SetAllParameters(&buffer, DS3D_DEFERRED);
  if (DS_OK != hr) {
    Warning("DS:3DBuffer set all parameters failed (0x%.8x).\n", hr);
  }
}

// Initialization for Surround sound support (4 channel or 5 channel).
// Creates 4 or 5 mono 3D buffers to be used as Front Left, (Front Center),
// Front Right, Rear Left, Rear Right
bool CAudioDirectSound::SNDDMA_InitSurround(LPDIRECTSOUND8 lpDS,
                                            WAVEFORMATEX *lpFormat,
                                            DSBCAPS *lpdsbc, int cchan) {
  if (lpDS == nullptr) return FALSE;

  DWORD dwSize, dwWrite;
  int reps;
  HRESULT hresult, hr;
  void *lpData = nullptr;

  // Force format to mono channel
  WAVEFORMATEX wvex;
  memcpy(&wvex, lpFormat, sizeof(*lpFormat));
  wvex.nChannels = 1;
  wvex.nBlockAlign = wvex.nChannels * wvex.wBitsPerSample / 8;
  wvex.nAvgBytesPerSec = wvex.nSamplesPerSec * wvex.nBlockAlign;

  DSBUFFERDESC dsbuf = {sizeof(DSBUFFERDESC)};
  // NOTE: LOCHARDWARE causes SB AWE64 to crash in it's DSOUND driver.
  // Don't use CTRLFREQUENCY (slow)
  dsbuf.dwFlags = DSBCAPS_CTRL3D;
  // Reserve space for each buffer
  dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE_SURROUND;
  dsbuf.lpwfxFormat = &wvex;

  // create 4 mono buffers FL, FR, RL, RR

  hr = lpDS->CreateSoundBuffer(&dsbuf, &pDSBufFL, NULL);
  if (DS_OK != hr) {
    Warning("DS:CreateSoundBuffer for 3d front left failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = lpDS->CreateSoundBuffer(&dsbuf, &pDSBufFR, NULL);
  if (DS_OK != hr) {
    Warning("DS:CreateSoundBuffer for 3d front right failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = lpDS->CreateSoundBuffer(&dsbuf, &pDSBufRL, NULL);
  if (DS_OK != hr) {
    Warning("DS:CreateSoundBuffer for 3d rear left failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = lpDS->CreateSoundBuffer(&dsbuf, &pDSBufRR, NULL);
  if (DS_OK != hr) {
    Warning("DS:CreateSoundBuffer for 3d rear right failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  // create center channel

  if (cchan == 5) {
    hr = lpDS->CreateSoundBuffer(&dsbuf, &pDSBufFC, NULL);
    if (DS_OK != hr) {
      Warning("DS:CreateSoundBuffer for 3d front center failed (0x%.8x).\n",
              hr);
      ReleaseSurround();
      return FALSE;
    }
  }

  // Try to get 4 or 5 3D buffers from the mono DS buffers

  hr = pDSBufFL->QueryInterface(IID_IDirectSound3DBufferDef,
                                (void **)&pDSBuf3DFL);
  if (DS_OK != hr) {
    Warning("DS:Query 3DBuffer for 3d front left failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = pDSBufFR->QueryInterface(IID_IDirectSound3DBufferDef,
                                (void **)&pDSBuf3DFR);
  if (DS_OK != hr) {
    Warning("DS:Query 3DBuffer for 3d front right failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = pDSBufRL->QueryInterface(IID_IDirectSound3DBufferDef,
                                (void **)&pDSBuf3DRL);
  if (DS_OK != hr) {
    Warning("DS:Query 3DBuffer for 3d rear left failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = pDSBufRR->QueryInterface(IID_IDirectSound3DBufferDef,
                                (void **)&pDSBuf3DRR);
  if (DS_OK != hr) {
    Warning("DS:Query 3DBuffer for 3d rear right failed (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  if (cchan == 5) {
    hr = pDSBufFC->QueryInterface(IID_IDirectSound3DBufferDef,
                                  (void **)&pDSBuf3DFC);
    if (DS_OK != hr) {
      Warning("DS:Query 3DBuffer for 3d front center failed (0x%.8x).\n", hr);
      ReleaseSurround();
      return FALSE;
    }
  }

  // set listener position & orientation.
  // DS uses left handed coord system: +x is right, +y is up, +z is forward

  IDirectSound3DListener *sound_3d_listener = nullptr;

  hr = pDSPBuf->QueryInterface(IID_IDirectSound3DListener,
                               (void **)&sound_3d_listener);
  if (SUCCEEDED(hr)) {
    DS3DLISTENER lparm = {sizeof(DS3DLISTENER)};

    hr = sound_3d_listener->GetAllParameters(&lparm);
    if (DS_OK != hr) {
      Warning("DS:3DListener get all parameters failed (0x%.8x).\n", hr);
    }

    // front x,y,z top x,y,z
    hr = sound_3d_listener->SetOrientation(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                           DS3D_IMMEDIATE);
    if (DS_OK != hr) {
      Warning("DS:3DListener set orientation failed (0x%.8x).\n", hr);
    }

    hr = sound_3d_listener->SetPosition(0.0f, 0.0f, 0.0f, DS3D_IMMEDIATE);
    if (DS_OK != hr) {
      Warning("DS:3DListener set position failed (0x%.8x).\n", hr);
    }
  } else {
    Warning("DS: failed to get 3D listener interface (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  // set 3d buffer position and orientation params

  D3DVECTOR bpos, bdir;

  bpos.x = -1.0;
  bpos.y = 0.0;
  bpos.z = 1.0;  // FL
  bdir.x = 1.0;
  bdir.y = 0.0;
  bdir.z = -1.0;

  DS3D_SetBufferParams(pDSBuf3DFL, &bpos, &bdir);

  bpos.x = 1.0;
  bpos.y = 0.0;
  bpos.z = 1.0;  // FR
  bdir.x = -1.0;
  bdir.y = 0.0;
  bdir.z = -1.0;

  DS3D_SetBufferParams(pDSBuf3DFR, &bpos, &bdir);

  bpos.x = -1.0;
  bpos.y = 0.0;
  bpos.z = -1.0;  // RL
  bdir.x = 1.0;
  bdir.y = 0.0;
  bdir.z = 1.0;

  DS3D_SetBufferParams(pDSBuf3DRL, &bpos, &bdir);

  bpos.x = 1.0;
  bpos.y = 0.0;
  bpos.z = -1.0;  // RR
  bdir.x = -1.0;
  bdir.y = 0.0;
  bdir.z = 1.0;

  DS3D_SetBufferParams(pDSBuf3DRR, &bpos, &bdir);

  if (cchan == 5) {
    bpos.x = 0.0;
    bpos.y = 0.0;
    bpos.z = 1.0;  // FC
    bdir.x = 0.0;
    bdir.y = 0.0;
    bdir.z = -1.0;

    DS3D_SetBufferParams(pDSBuf3DFC, &bpos, &bdir);
  }

  // commit all buffer param settings

  hr = sound_3d_listener->CommitDeferredSettings();
  if (DS_OK != hr) {
    Warning("DS:3DListener commit defered settings failed (0x%.8x).\n", hr);
  }

  m_deviceChannels = 1;  // 1 mono 3d output buffer
  m_deviceSampleBits = lpFormat->wBitsPerSample;
  m_deviceDmaSpeed = lpFormat->nSamplesPerSec;

  memset(lpdsbc, 0, sizeof(DSBCAPS));
  lpdsbc->dwSize = sizeof(DSBCAPS);

  hr = pDSBufFL->GetCaps(lpdsbc);
  if (DS_OK != hr) {
    Warning("DS:GetCaps failed for 3d sound buffer (0x%.8x).\n", hr);
    ReleaseSurround();
    return FALSE;
  }

  hr = pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("DS:Play failed for front left sound buffer (0x%.8x).\n", hr);
  }

  hr = pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("DS:Play failed for front right sound buffer (0x%.8x).\n", hr);
  }

  hr = pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("DS:Play failed for rear left sound buffer (0x%.8x).\n", hr);
  }

  hr = pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("DS:Play failed for rear right sound buffer (0x%.8x).\n", hr);
  }

  if (cchan == 5) {
    hr = pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);
    if (DS_OK != hr) {
      Warning("DS:Play failed for front center sound buffer (0x%.8x).\n", hr);
    }
  }

  if (snd_firsttime)
    DevMsg(
        "   %d channel(s)\n"
        "   %d bits/sample\n"
        "   %d samples/sec\n",
        cchan, DeviceSampleBits(), DeviceDmaSpeed());

  m_bufferSizeBytes = lpdsbc->dwBufferBytes;

  // Test everything just like in the normal initialization.
  if (cchan == 5) {
    reps = 0;
    while ((hresult = pDSBufFC->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                     &dwSize, NULL, NULL, 0)) != DS_OK) {
      if (hresult != DSERR_BUFFERLOST) {
        Warning("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for FC\n");
        ReleaseSurround();
        return FALSE;
      }

      if (++reps > 10000) {
        Warning("SNDDMA_InitDirect: DS: couldn't restore buffer for FC\n");
        ReleaseSurround();
        return FALSE;
      }
    }
    memset(lpData, 0, dwSize);
    //		DEBUG_DS_FillSquare( lpData, dwSize );
    pDSBufFC->Unlock(lpData, dwSize, NULL, 0);
  }

  reps = 0;
  while ((hresult = pDSBufFL->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                   &dwSize, NULL, NULL, 0)) != DS_OK) {
    if (hresult != DSERR_BUFFERLOST) {
      Warning("SNDDMA_InitSurround: DS::Lock Sound Buffer Failed for 3d FL\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("SNDDMA_InitSurround: DS: couldn't restore buffer for 3d FL\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  pDSBufFL->Unlock(lpData, dwSize, NULL, 0);

  reps = 0;
  while ((hresult = pDSBufFR->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                   &dwSize, NULL, NULL, 0)) != DS_OK) {
    if (hresult != DSERR_BUFFERLOST) {
      Warning("SNDDMA_InitSurround: DS::Lock Sound Buffer Failed for 3d FR\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("SNDDMA_InitSurround: DS: couldn't restore buffer for FR\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  pDSBufFR->Unlock(lpData, dwSize, NULL, 0);

  reps = 0;
  while ((hresult = pDSBufRL->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                   &dwSize, NULL, NULL, 0)) != DS_OK) {
    if (hresult != DSERR_BUFFERLOST) {
      Warning("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for RL\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("SNDDMA_InitDirect: DS: couldn't restore buffer for RL\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  pDSBufRL->Unlock(lpData, dwSize, NULL, 0);

  reps = 0;
  while ((hresult = pDSBufRR->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                   &dwSize, NULL, NULL, 0)) != DS_OK) {
    if (hresult != DSERR_BUFFERLOST) {
      Warning("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for RR\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("SNDDMA_InitDirect: DS: couldn't restore buffer for RR\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  pDSBufRR->Unlock(lpData, dwSize, NULL, 0);

  lpData = NULL;  // this is invalid now

  // OK Stop and get our positions and were good to go.
  pDSBufFL->Stop();
  pDSBufFR->Stop();
  pDSBufRL->Stop();
  pDSBufRR->Stop();
  if (cchan == 5) pDSBufFC->Stop();

  // get hardware playback position, store it, syncronize all buffers to FL

  pDSBufFL->GetCurrentPosition(&m_outputBufferStartOffset, &dwWrite);
  pDSBufFR->SetCurrentPosition(m_outputBufferStartOffset);
  pDSBufRL->SetCurrentPosition(m_outputBufferStartOffset);
  pDSBufRR->SetCurrentPosition(m_outputBufferStartOffset);
  if (cchan == 5) pDSBufFC->SetCurrentPosition(m_outputBufferStartOffset);

  pDSBufFL->Play(0, 0, DSBPLAY_LOOPING);
  pDSBufFR->Play(0, 0, DSBPLAY_LOOPING);
  pDSBufRL->Play(0, 0, DSBPLAY_LOOPING);
  pDSBufRR->Play(0, 0, DSBPLAY_LOOPING);
  if (cchan == 5) pDSBufFC->Play(0, 0, DSBPLAY_LOOPING);

  if (snd_firsttime) Warning("3d surround sound initialization successful\n");

  return TRUE;
}

void CAudioDirectSound::UpdateListener(const Vector &position,
                                       const Vector &forward,
                                       const Vector &right, const Vector &up) {}

void CAudioDirectSound::ChannelReset(int entnum, int channelIndex,
                                     float distanceMod) {}

const ch *CAudioDirectSound::DeviceName() {
  if (m_bSurroundCenter) return "5 Channel Surround";

  if (m_bSurround) return "4 Channel Surround";

  return "Direct Sound";
}

// use the partial buffer locking code in stereo as well - not available when
// recording a movie
ConVar snd_lockpartial("snd_lockpartial", "1");

// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of stereo samples
// out to the directsound secondary buffer(s).
// For 4 or 5 ch surround, there are 4 or 5 mono 16 bit secondary DS streaming
// buffers. For stereo speakers, there is one stereo 16 bit secondary DS
// streaming buffer.

void CAudioDirectSound::TransferSamples(int end) {
  int lpaintedtime = g_paintedtime;
  int endtime = end;

  // When Surround is enabled, divert to 4 or 5 chan xfer scheme.
  if (m_bSurround) {
    if (m_isInterleaved) {
      S_TransferSurround16Interleaved(PAINTBUFFER, REARPAINTBUFFER,
                                      CENTERPAINTBUFFER, lpaintedtime, endtime);
    } else {
      int cchan = (m_bSurroundCenter ? 5 : 4);

      S_TransferSurround16(PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER,
                           lpaintedtime, endtime, cchan);
    }
    return;
  } else if (snd_lockpartial.GetBool() && DeviceChannels() == 2 &&
             DeviceSampleBits() == 16 && !SND_IsRecording()) {
    S_TransferSurround16Interleaved(PAINTBUFFER, NULL, NULL, lpaintedtime,
                                    endtime);
  } else {
    DWORD *pBuffer = NULL;
    DWORD dwSize = 0;
    if (!LockDSBuffer(pDSBuf, &pBuffer, &dwSize, "DS_STEREO")) {
      S_Shutdown();
      S_Startup();
      return;
    }
    if (pBuffer) {
      if (DeviceChannels() == 2 && DeviceSampleBits() == 16) {
        S_TransferStereo16(pBuffer, PAINTBUFFER, lpaintedtime, endtime);
      } else {
        // UNDONE: obsolete - no 8 bit mono output supported
        S_TransferPaintBuffer(pBuffer, PAINTBUFFER, lpaintedtime, endtime);
      }
      pDSBuf->Unlock(pBuffer, dwSize, NULL, 0);
    }
  }
}

bool CAudioDirectSound::IsUsingBufferPerSpeaker() {
  return m_bSurround && !m_isInterleaved;
}

bool CAudioDirectSound::LockDSBuffer(LPDIRECTSOUNDBUFFER pBuffer,
                                     DWORD **pdwWriteBuffer,
                                     DWORD *pdwSizeBuffer,
                                     const ch *pBufferName, int lockFlags) {
  if (!pBuffer) return false;

  HRESULT hr;
  uint32_t reps = 0;

  while ((hr = pBuffer->Lock(0, m_bufferSizeBytes, (void **)pdwWriteBuffer,
                             pdwSizeBuffer, nullptr, nullptr, lockFlags)) !=
         DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Msg("DS::Lock sound buffer %s failed (0x%.8x).\n", pBufferName, hr);
      return false;
    }

    if (++reps > 10000) {
      Msg("DS::Couldn't restore sound buffer %s (0x%.8x).\n", pBufferName, hr);
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Given front, rear and center stereo paintbuffers, split samples into 4 or 5
// mono directsound buffers (FL, FC, FR, RL, RR)
void CAudioDirectSound::S_TransferSurround16(portable_samplepair_t *pfront,
                                             portable_samplepair_t *prear,
                                             portable_samplepair_t *pcenter,
                                             int lpaintedtime, int endtime,
                                             int cchan) {
  int lpos;
  DWORD *pdwWriteFL = NULL, *pdwWriteFR = NULL, *pdwWriteRL = NULL,
        *pdwWriteRR = NULL, *pdwWriteFC = NULL;
  DWORD dwSizeFL = 0, dwSizeFR = 0, dwSizeRL = 0, dwSizeRR = 0, dwSizeFC = 0;
  int i, j, *snd_p, *snd_rp, *snd_cp, volumeFactor;
  short *snd_out_fleft, *snd_out_fright, *snd_out_rleft, *snd_out_rright,
      *snd_out_fcenter;

  pdwWriteFC = NULL;  // compiler warning
  dwSizeFC = 0;
  snd_out_fcenter = NULL;

  volumeFactor = S_GetMasterVolume() * 256;

  // lock all 4 or 5 mono directsound buffers FL, FR, RL, RR, FC
  if (!LockDSBuffer(pDSBufFL, &pdwWriteFL, &dwSizeFL, "FL") ||
      !LockDSBuffer(pDSBufFR, &pdwWriteFR, &dwSizeFR, "FR") ||
      !LockDSBuffer(pDSBufRL, &pdwWriteRL, &dwSizeRL, "RL") ||
      !LockDSBuffer(pDSBufRR, &pdwWriteRR, &dwSizeRR, "RR")) {
    S_Shutdown();
    S_Startup();
    return;
  }

  if (cchan == 5 && !LockDSBuffer(pDSBufFC, &pdwWriteFC, &dwSizeFC, "FC")) {
    S_Shutdown();
    S_Startup();
    return;
  }

  // take stereo front and rear paintbuffers, and center paintbuffer if
  // provided, and copy samples into the 4 or 5 mono directsound buffers

  snd_rp = (int *)prear;
  snd_cp = (int *)pcenter;
  snd_p = (int *)pfront;

  int linearCount;  // space in output buffer for linearCount mono samples
  int sampleMonoCount = DeviceSampleCount();  // number of mono samples per
                                              // output buffer
                                              // (was;(DeviceSampleCount()>>1))
  int sampleMask = sampleMonoCount - 1;

  // paintedtime - number of full samples that have played since start
  // endtime - number of full samples to play to - endtime is g_soundtime +
  // mixahead samples

  while (lpaintedtime < endtime) {
    lpos = lpaintedtime &
           sampleMask;  // lpos is next output position in output buffer

    linearCount = sampleMonoCount - lpos;

    // limit output count to requested number of samples

    if (linearCount > endtime - lpaintedtime)
      linearCount = endtime - lpaintedtime;

    snd_out_fleft = (short *)pdwWriteFL + lpos;
    snd_out_fright = (short *)pdwWriteFR + lpos;
    snd_out_rleft = (short *)pdwWriteRL + lpos;
    snd_out_rright = (short *)pdwWriteRR + lpos;

    if (cchan == 5) snd_out_fcenter = (short *)pdwWriteFC + lpos;

    // for 16 bit sample in the front and rear stereo paintbuffers, copy
    // into the 4 or 5 FR, FL, RL, RR, FC directsound paintbuffers

    for (i = 0, j = 0; i < linearCount; i++, j += 2) {
      snd_out_fleft[i] = (snd_p[j] * volumeFactor) >> 8;
      snd_out_fright[i] = (snd_p[j + 1] * volumeFactor) >> 8;
      snd_out_rleft[i] = (snd_rp[j] * volumeFactor) >> 8;
      snd_out_rright[i] = (snd_rp[j + 1] * volumeFactor) >> 8;
    }

    // copy front center buffer (mono) data to center chan directsound
    // paintbuffer

    if (cchan == 5) {
      for (i = 0, j = 0; i < linearCount; i++, j += 2) {
        snd_out_fcenter[i] = (snd_cp[j] * volumeFactor) >> 8;
      }
    }

    snd_p += linearCount << 1;
    snd_rp += linearCount << 1;
    snd_cp += linearCount << 1;

    lpaintedtime += linearCount;
  }

  pDSBufFL->Unlock(pdwWriteFL, dwSizeFL, NULL, 0);
  pDSBufFR->Unlock(pdwWriteFR, dwSizeFR, NULL, 0);
  pDSBufRL->Unlock(pdwWriteRL, dwSizeRL, NULL, 0);
  pDSBufRR->Unlock(pdwWriteRR, dwSizeRR, NULL, 0);

  if (cchan == 5) pDSBufFC->Unlock(pdwWriteFC, dwSizeFC, NULL, 0);
}

struct surround_transfer_t {
  int paintedtime;
  int linearCount;
  int sampleMask;
  int channelCount;
  int *snd_p;
  int *snd_rp;
  int *snd_cp;
  short *pOutput;
};

static void TransferSamplesToSurroundBuffer(int outputCount,
                                            surround_transfer_t &transfer) {
  int i, j;
  int volumeFactor = S_GetMasterVolume() * 256;

  if (transfer.channelCount == 2) {
    for (i = 0, j = 0; i < outputCount; i++, j += 2) {
      transfer.pOutput[0] = (transfer.snd_p[j] * volumeFactor) >> 8;      // FL
      transfer.pOutput[1] = (transfer.snd_p[j + 1] * volumeFactor) >> 8;  // FR
      transfer.pOutput += 2;
    }
  }
  // no center channel, 4 channel surround
  else if (transfer.channelCount == 4) {
    for (i = 0, j = 0; i < outputCount; i++, j += 2) {
      transfer.pOutput[0] = (transfer.snd_p[j] * volumeFactor) >> 8;       // FL
      transfer.pOutput[1] = (transfer.snd_p[j + 1] * volumeFactor) >> 8;   // FR
      transfer.pOutput[2] = (transfer.snd_rp[j] * volumeFactor) >> 8;      // RL
      transfer.pOutput[3] = (transfer.snd_rp[j + 1] * volumeFactor) >> 8;  // RR
      transfer.pOutput += 4;
      // Assert( baseOffset <= (DeviceSampleCount()) );
    }
  } else {
    Assert(transfer.snd_cp);
    // 6 channel / 5.1
    for (i = 0, j = 0; i < outputCount; i++, j += 2) {
      transfer.pOutput[0] = (transfer.snd_p[j] * volumeFactor) >> 8;      // FL
      transfer.pOutput[1] = (transfer.snd_p[j + 1] * volumeFactor) >> 8;  // FR

      transfer.pOutput[2] = (transfer.snd_cp[j] * volumeFactor) >> 8;  // Center

      transfer.pOutput[3] = 0;

      transfer.pOutput[4] = (transfer.snd_rp[j] * volumeFactor) >> 8;      // RL
      transfer.pOutput[5] = (transfer.snd_rp[j + 1] * volumeFactor) >> 8;  // RR

      transfer.pOutput += 6;
    }
  }

  transfer.snd_p += outputCount << 1;
  if (transfer.snd_rp) {
    transfer.snd_rp += outputCount << 1;
  }
  if (transfer.snd_cp) {
    transfer.snd_cp += outputCount << 1;
  }

  transfer.paintedtime += outputCount;
  transfer.linearCount -= outputCount;
}

void CAudioDirectSound::S_TransferSurround16Interleaved_FullLock(
    const portable_samplepair_t *pfront, const portable_samplepair_t *prear,
    const portable_samplepair_t *pcenter, int lpaintedtime, int endtime) {
  int lpos;
  DWORD *pdwWrite = NULL;
  DWORD dwSize = 0;
  int i, j, *snd_p, *snd_rp, *snd_cp, volumeFactor;

  volumeFactor = S_GetMasterVolume() * 256;
  int channelCount = m_bSurroundCenter ? 5 : 4;
  if (DeviceChannels() == 2) {
    channelCount = 2;
  }

  // lock single interleaved buffer
  if (!LockDSBuffer(pDSBuf, &pdwWrite, &dwSize, "DS_INTERLEAVED")) {
    S_Shutdown();
    S_Startup();
    return;
  }

  // take stereo front and rear paintbuffers, and center paintbuffer if
  // provided, and copy samples into the 4 or 5 mono directsound buffers

  snd_rp = (int *)prear;
  snd_cp = (int *)pcenter;
  snd_p = (int *)pfront;

  int linearCount;  // space in output buffer for linearCount mono samples
  int sampleMonoCount =
      m_bufferSizeBytes / (DeviceSampleBytes() *
                           DeviceChannels());  // number of mono samples per
                                               // output buffer
                                               // (was;(DeviceSampleCount()>>1))
  int sampleMask = sampleMonoCount - 1;

  // paintedtime - number of full samples that have played since start
  // endtime - number of full samples to play to - endtime is g_soundtime +
  // mixahead samples

  short *pOutput = (short *)pdwWrite;
  while (lpaintedtime < endtime) {
    lpos = lpaintedtime &
           sampleMask;  // lpos is next output position in output buffer

    linearCount = sampleMonoCount - lpos;

    // limit output count to requested number of samples

    if (linearCount > endtime - lpaintedtime)
      linearCount = endtime - lpaintedtime;

    if (channelCount == 4) {
      int baseOffset = lpos * channelCount;
      for (i = 0, j = 0; i < linearCount; i++, j += 2) {
        pOutput[baseOffset + 0] = (snd_p[j] * volumeFactor) >> 8;       // FL
        pOutput[baseOffset + 1] = (snd_p[j + 1] * volumeFactor) >> 8;   // FR
        pOutput[baseOffset + 2] = (snd_rp[j] * volumeFactor) >> 8;      // RL
        pOutput[baseOffset + 3] = (snd_rp[j + 1] * volumeFactor) >> 8;  // RR
        baseOffset += 4;
      }
    } else {
      Assert(channelCount == 5);  // 6 channel / 5.1
      int baseOffset = lpos * 6;
      for (i = 0, j = 0; i < linearCount; i++, j += 2) {
        pOutput[baseOffset + 0] = (snd_p[j] * volumeFactor) >> 8;      // FL
        pOutput[baseOffset + 1] = (snd_p[j + 1] * volumeFactor) >> 8;  // FR

        pOutput[baseOffset + 2] = (snd_cp[j] * volumeFactor) >> 8;  // Center
        // NOTE: Let the hardware mix the sub from the main channels since
        //		 we don't have any sub-specific sounds, or direct
        // sub-addressing
        pOutput[baseOffset + 3] = 0;

        pOutput[baseOffset + 4] = (snd_rp[j] * volumeFactor) >> 8;      // RL
        pOutput[baseOffset + 5] = (snd_rp[j + 1] * volumeFactor) >> 8;  // RR

        baseOffset += 6;
      }
    }

    snd_p += linearCount << 1;
    snd_rp += linearCount << 1;
    snd_cp += linearCount << 1;

    lpaintedtime += linearCount;
  }

  pDSBuf->Unlock(pdwWrite, dwSize, NULL, 0);
}

void CAudioDirectSound::S_TransferSurround16Interleaved(
    const portable_samplepair_t *pfront, const portable_samplepair_t *prear,
    const portable_samplepair_t *pcenter, int lpaintedtime, int endtime) {
  if (!pDSBuf) return;
  if (!snd_lockpartial.GetBool()) {
    S_TransferSurround16Interleaved_FullLock(pfront, prear, pcenter,
                                             lpaintedtime, endtime);
    return;
  }
  // take stereo front and rear paintbuffers, and center paintbuffer if
  // provided, and copy samples into the 4 or 5 mono directsound buffers

  surround_transfer_t transfer;
  transfer.snd_rp = (int *)prear;
  transfer.snd_cp = (int *)pcenter;
  transfer.snd_p = (int *)pfront;

  int sampleMonoCount =
      DeviceSampleCount() /
      DeviceChannels();  // number of full samples per output buffer
  Assert(IsPowerOfTwo(sampleMonoCount));
  transfer.sampleMask = sampleMonoCount - 1;
  transfer.paintedtime = lpaintedtime;
  transfer.linearCount = endtime - lpaintedtime;
  // paintedtime - number of full samples that have played since start
  // endtime - number of full samples to play to - endtime is g_soundtime +
  // mixahead samples
  int channelCount = m_bSurroundCenter ? 6 : 4;
  if (DeviceChannels() == 2) {
    channelCount = 2;
  }
  transfer.channelCount = channelCount;
  void *pBuffer0 = NULL;
  void *pBuffer1 = NULL;
  DWORD size0, size1;
  int lpos =
      transfer.paintedtime &
      transfer.sampleMask;  // lpos is next output position in output buffer

  int offset = lpos * 2 * channelCount;
  int lockSize = transfer.linearCount * 2 * channelCount;
  int reps = 0;
  HRESULT hr;
  while ((hr = pDSBuf->Lock(offset, lockSize, &pBuffer0, &size0, &pBuffer1,
                            &size1, 0)) != DS_OK) {
    if (hr == DSERR_BUFFERLOST) {
      if (++reps < 10000) continue;
    }
    Msg("DS::Lock Sound Buffer Failed\n");
    return;
  }

  if (pBuffer0) {
    transfer.pOutput = (short *)pBuffer0;
    TransferSamplesToSurroundBuffer(size0 / (channelCount * 2), transfer);
  }
  if (pBuffer1) {
    transfer.pOutput = (short *)pBuffer1;
    TransferSamplesToSurroundBuffer(size1 / (channelCount * 2), transfer);
  }
  pDSBuf->Unlock(pBuffer0, size0, pBuffer1, size1);
}
