// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "audio_pch.h"

#include "snd_dev_direct.h"

#include <dsound.h>
#include <ks.h>
#include <ksmedia.h>
#include "../../sys_dll.h"
#include "avi/ibik.h"
#include "base/include/windows/windows_errno_info.h"
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

enum sndinitstat { SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL };

// output buffer size in bytes
#define SECONDARY_BUFFER_SIZE 0x10000
// output buffer size in bytes, one per channel
#define SECONDARY_BUFFER_SIZE_SURROUND 0x04000

extern bool MIX_ScaleChannelVolume(paintbuffer_t *ppaint, channel_t *pChannel,
                                   int volume[CCHANVOLUMES], int mixchans);
void OnSndSurroundCvarChanged(IConVar *var, const ch *pOldString,
                              float flOldValue);
void OnSndSurroundLegacyChanged(IConVar *var, const ch *pOldString,
                                float flOldValue);

IDirectSound8 *pDS;

// Purpose: Implementation of Direct Sound audio device.
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
  int DeviceChannels() { return device_channels_count_; }
  int DeviceSampleBits() { return device_bits_per_sample_; }
  int DeviceSampleBytes() { return device_bits_per_sample_ / 8; }
  int DeviceDmaSpeed() { return device_samples_per_second_; }
  int DeviceSampleCount() { return device_samples_count_; }

  bool IsInterleaved() { return is_interleaved_; }

  // Singleton object
  static CAudioDirectSound *singleton_;

 private:
  void DetectWindowsSpeakerSetup();
  bool LockDSBuffer(IDirectSoundBuffer *pBuffer, DWORD **pdwWriteBuffer,
                    DWORD *pdwSizeBuffer, const ch *pBufferName,
                    int lockFlags = 0);
  bool IsUsingBufferPerSpeaker();

  sndinitstat SNDDMA_InitDirect();
  bool SNDDMA_InitInterleaved(IDirectSound8 *lpDS, WAVEFORMATEX *lpFormat,
                              u16 channelCount);
  bool SNDDMA_InitSurround(IDirectSound8 *lpDS, WAVEFORMATEX *lpFormat,
                           DSBCAPS *lpdsbc, int cchan);
  void ReleaseSurround();
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

  IDirectSoundBuffer *ds_buffer_, *ds_p_buffer_;

  u16 device_channels_count_;      // channels per hardware output buffer (1 for
                                   // quad/5.1, 2 for stereo)
  u16 device_bits_per_sample_;     // bits per sample (16)
  u32 device_samples_count_;       // count of mono samples in output buffer
  u32 device_samples_per_second_;  // samples per second per output buffer
  u32 device_buffer_size_bytes_;   // size of a single hardware output buffer

  // output buffer playback starting uint8_t offset
  DWORD output_buffer_start_offset_bytes_;
  HMODULE directsound8_module_;
  bool is_interleaved_;

  IDirectSoundBuffer *ds_buf_fl_ = nullptr;
  IDirectSoundBuffer *ds_buf_fr_ = nullptr;
  IDirectSoundBuffer *ds_buf_rl_ = nullptr;
  IDirectSoundBuffer *ds_buf_rr_ = nullptr;
  IDirectSoundBuffer *ds_bug_fc_ = nullptr;
  IDirectSound3DBuffer8 *ds_3d_buf_fl_ = nullptr;
  IDirectSound3DBuffer8 *ds_3d_buf_fr_ = nullptr;
  IDirectSound3DBuffer8 *ds_3d_buf_rl_ = nullptr;
  IDirectSound3DBuffer8 *ds_3d_buf_rr_ = nullptr;
  IDirectSound3DBuffer8 *ds_3d_buf_fc_ = nullptr;
};

CAudioDirectSound *CAudioDirectSound::singleton_ = nullptr;

CAudioDirectSound::~CAudioDirectSound() { singleton_ = nullptr; }

bool CAudioDirectSound::Init() {
  directsound8_module_ = nullptr;

  static bool is_first_time_init{true};

  if (is_first_time_init) {
    snd_surround.InstallChangeCallback(&OnSndSurroundCvarChanged);
    snd_legacy_surround.InstallChangeCallback(&OnSndSurroundLegacyChanged);
    is_first_time_init = false;
  }

  if (SNDDMA_InitDirect() == SIS_SUCCESS) {
    // Tells Bink to use DirectSound for its audio decoding.
    if (!bik->SetDirectSoundDevice(pDS)) {
      AssertMsg(false,
                "Audio Direct Sound: bink can't use DirectSound8 device.");
    }

    return true;
  }

  return false;
}

void CAudioDirectSound::Shutdown() {
  ReleaseSurround();

  if (ds_buffer_) {
    ds_buffer_->Stop();
    ds_buffer_->Release();
  }

  // only release primary buffer if it's not also the mixing buffer we just
  // released
  if (ds_p_buffer_ && (ds_buffer_ != ds_p_buffer_)) {
    ds_p_buffer_->Release();
  }

  if (pDS) {
    pDS->SetCooperativeLevel(*pmainwindow, DSSCL_NORMAL);
    pDS->Release();
  }

  pDS = nullptr;
  ds_buffer_ = nullptr;
  ds_p_buffer_ = nullptr;

  if (directsound8_module_) {
    FreeLibrary(directsound8_module_);
    directsound8_module_ = nullptr;
  }

  if (this == CAudioDirectSound::singleton_) {
    CAudioDirectSound::singleton_ = nullptr;
  }
}

// Total number of samples that have played out to hardware for current output
// buffer (ie: from buffer offset start). return playback position within output
// playback buffer: the output units are dependant on the device channels so the
// ouput units for a 2 channel device are as 16 bit LR pairs and the output unit
// for a 1 channel device are as 16 bit mono samples. take into account the
// original start position within the buffer, and calculate difference between
// current position (with buffer wrap) and start position.
int CAudioDirectSound::GetOutputPosition() {
  u32 start, current;
  DWORD dwCurrent;

  // get size in bytes of output buffer
  const u32 size_bytes = device_buffer_size_bytes_;
  if (IsUsingBufferPerSpeaker()) {
    // mono output buffers
    // get uint8_t offset of playback cursor in Front Left output buffer
    ds_buf_fl_->GetCurrentPosition(&dwCurrent, nullptr);

    start = output_buffer_start_offset_bytes_;
    current = dwCurrent;
  } else {
    // multi-channel interleavd output buffer
    // get uint8_t offset of playback cursor in output buffer
    ds_buffer_->GetCurrentPosition(&dwCurrent, nullptr);

    start = output_buffer_start_offset_bytes_;
    current = dwCurrent;
  }

  u32 samp16;
  // get 16 bit samples played, relative to buffer starting offset
  if (current > start) {
    // get difference & convert to 16 bit mono samples
    samp16 = (current - start) >> SAMPLE_16BIT_SHIFT;
  } else {
    // get difference (with buffer wrap) convert to 16 bit mono samples
    samp16 = ((size_bytes - start) + current) >> SAMPLE_16BIT_SHIFT;
  }

  return samp16 / DeviceChannels();
}

void CAudioDirectSound::Pause() {
  if (ds_buffer_) ds_buffer_->Stop();

  if (ds_buf_fl_) ds_buf_fl_->Stop();
  if (ds_buf_fr_) ds_buf_fr_->Stop();
  if (ds_buf_rl_) ds_buf_rl_->Stop();
  if (ds_buf_rr_) ds_buf_rr_->Stop();
  if (ds_bug_fc_) ds_bug_fc_->Stop();
}

void CAudioDirectSound::UnPause() {
  if (ds_buffer_) ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);

  if (ds_buf_fl_) ds_buf_fl_->Play(0, 0, DSBPLAY_LOOPING);
  if (ds_buf_fr_) ds_buf_fr_->Play(0, 0, DSBPLAY_LOOPING);
  if (ds_buf_rl_) ds_buf_rl_->Play(0, 0, DSBPLAY_LOOPING);
  if (ds_buf_rr_) ds_buf_rr_->Play(0, 0, DSBPLAY_LOOPING);
  if (ds_bug_fc_) ds_bug_fc_->Play(0, 0, DSBPLAY_LOOPING);
}

float CAudioDirectSound::MixDryVolume() { return 0.0f; }

bool CAudioDirectSound::Should3DMix() { return m_bSurround; }

IAudioDevice *Audio_CreateDirectSoundDevice() {
  if (!CAudioDirectSound::singleton_)
    CAudioDirectSound::singleton_ = new CAudioDirectSound;

  if (CAudioDirectSound::singleton_->Init()) {
    if (snd_firsttime)
      DevMsg("Audio Direct Sound: using DirectSound8 as audio interface.\n");

    return CAudioDirectSound::singleton_;
  }

  DevMsg("Audio Direct Sound: DirectSound8 failed to init.\n");

  delete CAudioDirectSound::singleton_;
  CAudioDirectSound::singleton_ = nullptr;

  return nullptr;
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
  // pDSBuf is nullptr.
  if (IsUsingBufferPerSpeaker()) {
    if (ds_buf_fl_->GetStatus(&dwStatus) != DS_OK)
      Msg("Audio Direct Sound: Couldn't get SURROUND FL sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) ds_buf_fl_->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING))
      ds_buf_fl_->Play(0, 0, DSBPLAY_LOOPING);

    if (ds_buf_fr_->GetStatus(&dwStatus) != DS_OK)
      Msg("Audio Direct Sound: Couldn't get SURROUND FR sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) ds_buf_fr_->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING))
      ds_buf_fr_->Play(0, 0, DSBPLAY_LOOPING);

    if (ds_buf_rl_->GetStatus(&dwStatus) != DS_OK)
      Msg("Audio Direct Sound: Couldn't get SURROUND RL sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) ds_buf_rl_->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING))
      ds_buf_rl_->Play(0, 0, DSBPLAY_LOOPING);

    if (ds_buf_rr_->GetStatus(&dwStatus) != DS_OK)
      Msg("Audio Direct Sound: Couldn't get SURROUND RR sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) ds_buf_rr_->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING))
      ds_buf_rr_->Play(0, 0, DSBPLAY_LOOPING);

    if (m_bSurroundCenter) {
      if (ds_bug_fc_->GetStatus(&dwStatus) != DS_OK)
        Msg("Audio Direct Sound: Couldn't get SURROUND FC sound buffer "
            "status\n");

      if (dwStatus & DSBSTATUS_BUFFERLOST) ds_bug_fc_->Restore();

      if (!(dwStatus & DSBSTATUS_PLAYING))
        ds_bug_fc_->Play(0, 0, DSBPLAY_LOOPING);
    }
  } else if (ds_buffer_) {
    if (ds_buffer_->GetStatus(&dwStatus) != DS_OK)
      Msg("Audio Direct Sound: Couldn't get sound buffer status\n");

    if (dwStatus & DSBSTATUS_BUFFERLOST) ds_buffer_->Restore();

    if (!(dwStatus & DSBSTATUS_PLAYING))
      ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);
  }

  return endtime;
}

void CAudioDirectSound::PaintEnd() {}

void CAudioDirectSound::ClearBuffer() {
  int clear;

  DWORD dwSizeFL, dwSizeFR, dwSizeRL, dwSizeRR, dwSizeFC;
  ch *pDataFL, *pDataFR, *pDataRL, *pDataRR, *pDataFC;

  dwSizeFC = 0;  // compiler warning
  pDataFC = nullptr;

  if (IsUsingBufferPerSpeaker()) {
    int SURROUNDreps;
    HRESULT SURROUNDhresult;
    SURROUNDreps = 0;

    if (!ds_buf_fl_ && !ds_buf_fr_ && !ds_buf_rl_ && !ds_buf_rr_ && !ds_bug_fc_)
      return;

    while ((SURROUNDhresult = ds_buf_fl_->Lock(0, device_buffer_size_bytes_,
                                               (void **)&pDataFL, &dwSizeFL,
                                               nullptr, nullptr, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("Audio Direct Sound: lock FL Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("Audio Direct Sound: couldn't restore FL buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult = ds_buf_fr_->Lock(0, device_buffer_size_bytes_,
                                               (void **)&pDataFR, &dwSizeFR,
                                               nullptr, nullptr, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("Audio Direct Sound: lock FR Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("Audio Direct Sound: couldn't restore FR buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult = ds_buf_rl_->Lock(0, device_buffer_size_bytes_,
                                               (void **)&pDataRL, &dwSizeRL,
                                               nullptr, nullptr, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("Audio Direct Sound: lock RL Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("Audio Direct Sound: couldn't restore RL buffer\n");
        S_Shutdown();
        return;
      }
    }

    SURROUNDreps = 0;
    while ((SURROUNDhresult = ds_buf_rr_->Lock(0, device_buffer_size_bytes_,
                                               (void **)&pDataRR, &dwSizeRR,
                                               nullptr, nullptr, 0)) != DS_OK) {
      if (SURROUNDhresult != DSERR_BUFFERLOST) {
        Msg("Audio Direct Sound: lock RR Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++SURROUNDreps > 10000) {
        Msg("Audio Direct Sound: couldn't restore RR buffer\n");
        S_Shutdown();
        return;
      }
    }

    if (m_bSurroundCenter) {
      SURROUNDreps = 0;
      while ((SURROUNDhresult = ds_bug_fc_->Lock(
                  0, device_buffer_size_bytes_, (void **)&pDataFC, &dwSizeFC,
                  nullptr, nullptr, 0)) != DS_OK) {
        if (SURROUNDhresult != DSERR_BUFFERLOST) {
          Msg("Audio Direct Sound: lock FC Sound Buffer Failed\n");
          S_Shutdown();
          return;
        }

        if (++SURROUNDreps > 10000) {
          Msg("Audio Direct Sound: couldn't restore FC buffer\n");
          S_Shutdown();
          return;
        }
      }
    }

    memset(pDataFL, 0, device_buffer_size_bytes_);
    memset(pDataFR, 0, device_buffer_size_bytes_);
    memset(pDataRL, 0, device_buffer_size_bytes_);
    memset(pDataRR, 0, device_buffer_size_bytes_);

    if (m_bSurroundCenter) memset(pDataFC, 0, device_buffer_size_bytes_);

    ds_buf_fl_->Unlock(pDataFL, dwSizeFL, nullptr, 0);
    ds_buf_fr_->Unlock(pDataFR, dwSizeFR, nullptr, 0);
    ds_buf_rl_->Unlock(pDataRL, dwSizeRL, nullptr, 0);
    ds_buf_rr_->Unlock(pDataRR, dwSizeRR, nullptr, 0);

    if (m_bSurroundCenter) ds_bug_fc_->Unlock(pDataFC, dwSizeFC, nullptr, 0);

    return;
  }

  if (!ds_buffer_) return;

  if (DeviceSampleBits() == 8)
    clear = 0x80;
  else
    clear = 0;

  if (ds_buffer_) {
    DWORD dwSize;
    DWORD *pData;
    int reps;
    HRESULT hresult;

    reps = 0;
    while ((hresult =
                ds_buffer_->Lock(0, device_buffer_size_bytes_, (void **)&pData,
                                 &dwSize, nullptr, nullptr, 0)) != DS_OK) {
      if (hresult != DSERR_BUFFERLOST) {
        Msg("Audio Direct Sound: lock Sound Buffer Failed\n");
        S_Shutdown();
        return;
      }

      if (++reps > 10000) {
        Msg("Audio Direct Sound: couldn't restore buffer\n");
        S_Shutdown();
        return;
      }
    }

    memset(pData, clear, dwSize);

    ds_buffer_->Unlock(pData, dwSize, nullptr, 0);
  }
}

void CAudioDirectSound::StopAllSounds() {}

bool CAudioDirectSound::SNDDMA_InitInterleaved(IDirectSound8 *lpDS,
                                               WAVEFORMATEX *lpFormat,
                                               u16 channelCount) {
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

  HRESULT hr = lpDS->CreateSoundBuffer(&dsbdesc, &ds_buffer_, nullptr);
  if (FAILED(hr)) {
    Msg("Audio Direct Sound: can't create interleaved sound buffer (0x%.8x).\n",
        hr);
    return false;
  }

  DWORD dwSize = 0, dwWrite;
  DWORD *pBuffer = 0;
  if (!LockDSBuffer(ds_buffer_, &pBuffer, &dwSize, "DS_INTERLEAVED",
                    DSBLOCK_ENTIREBUFFER))
    return false;

  device_channels_count_ = wfx.Format.nChannels;
  device_bits_per_sample_ = wfx.Format.wBitsPerSample;
  device_samples_per_second_ = wfx.Format.nSamplesPerSec;
  device_buffer_size_bytes_ = dsbdesc.dwBufferBytes;
  is_interleaved_ = true;

  memset(pBuffer, 0, dwSize);

  hr = ds_buffer_->Unlock(pBuffer, dwSize, nullptr, 0);
  if (FAILED(hr)) {
    Warning(
        "Audio Direct Sound: can't unlock interleaved sound buffer (0x%.8x).\n",
        hr);
  }

  // Make sure mixer is active (this was moved after the zeroing to avoid
  // popping on startup -- at least when using the dx9.0b debug .dlls)
  hr = ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);
  if (FAILED(hr)) {
    Warning(
        "Audio Direct Sound: can't play interleaved sound buffer (0x%.8x).\n",
        hr);
  }

  hr = ds_buffer_->Stop();
  if (FAILED(hr)) {
    Warning(
        "Audio Direct Sound: can't stop interleaved sound buffer (0x%.8x).\n",
        hr);
  }

  hr = ds_buffer_->GetCurrentPosition(&output_buffer_start_offset_bytes_,
                                      &dwWrite);
  if (FAILED(hr)) {
    Warning(
        "Audio Direct Sound: can't get current position interleaved sound "
        "buffer (0x%.8x).\n",
        hr);
  }

  hr = ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);
  if (FAILED(hr)) {
    Warning(
        "Audio Direct Sound: can't play interleaved sound buffer (0x%.8x).\n",
        hr);
  }

  return true;
}

// Direct-Sound support
sndinitstat CAudioDirectSound::SNDDMA_InitDirect() {
  void *lpData = nullptr;
  bool primary_format_set = false;

  using DirectSoundCreate8Fn = HRESULT(WINAPI *)(
      LPCGUID pcGuidDevice, IDirectSound8 * *ppDS8, LPUNKNOWN pUnkOuter);
  DirectSoundCreate8Fn ds_create_8_fn = nullptr;

  if (!directsound8_module_) {
    directsound8_module_ =
        LoadLibraryExW(L"dsound.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (directsound8_module_ == nullptr) {
      Warning("Audio Direct Sound: couldn't load dsound.dll: %s.\n",
              source::windows::windows_errno_info_last_error().description);
      return SIS_FAILURE;
    }

    ds_create_8_fn = reinterpret_cast<decltype(ds_create_8_fn)>(
        GetProcAddress(directsound8_module_, "DirectSoundCreate8"));
    if (!ds_create_8_fn) {
      Warning(
          "Audio Direct Sound: couldn't find DirectSoundCreate8 in dsound.dll: "
          "%s.\n",
          source::windows::windows_errno_info_last_error().description);
      return SIS_FAILURE;
    }
  }

  HRESULT hr = (*ds_create_8_fn)(nullptr, &pDS, nullptr);
  if (hr != DS_OK) {
    if (hr != DSERR_ALLOCATED) {
      DevMsg(
          "Audio Direct Sound: DirectSoundCreate8 failed to create "
          "DirectSound8: %s.\n",
          source::windows::make_windows_errno_info(hr).description);
      return SIS_FAILURE;
    }

    return SIS_NOTAVAIL;
  }

  // get snd_surround value from window settings
  DetectWindowsSpeakerSetup();

  m_bSurround = false;
  m_bSurroundCenter = false;
  m_bHeadphone = false;
  is_interleaved_ = false;

  u16 pri_channels = 2;
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
  device_channels_count_ = pri_channels;
  device_bits_per_sample_ = 16;                  // hardware bits per sample
  device_samples_per_second_ = SOUND_DMA_SPEED;  // hardware playback rate

  WAVEFORMATEX format;
  memset(&format, 0, sizeof(format));
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = pri_channels;
  format.wBitsPerSample = device_bits_per_sample_;
  format.nSamplesPerSec = device_samples_per_second_;
  format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
  format.cbSize = 0;
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

  DSCAPS dscaps = {sizeof(dscaps)};
  hr = pDS->GetCaps(&dscaps);

  if (DS_OK != hr) {
    Warning("Audio Direct Sound: couldn't get DirectSound8 caps %s.\n",
            source::windows::make_windows_errno_info(hr).description);
    Shutdown();
    return SIS_FAILURE;
  }

  if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
    Warning("Audio Direct Sound: no DirectSound8 driver installed.\n");
    Shutdown();
    return SIS_FAILURE;
  }

  hr = pDS->SetCooperativeLevel(*pmainwindow, DSSCL_EXCLUSIVE);

  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: set DirectSound8 cooperative level to exclusive "
        "failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
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
    hr = pDS->CreateSoundBuffer(&primary_buffer, &ds_p_buffer_, nullptr);

    if (DS_OK == hr) {
      WAVEFORMATEX pformat = format;

      hr = ds_p_buffer_->SetFormat(&pformat);

      if (DS_OK != hr) {
        if (snd_firsttime)
          DevMsg(
              "Audio Direct Sound: set primary DirectSound8 buffer format: no "
              "%s.\n",
              source::windows::make_windows_errno_info(hr).description);
      } else {
        if (snd_firsttime)
          DevMsg(
              "Audio Direct Sound: set primary DirectSound8 buffer format: "
              "yes\n");

        primary_format_set = true;
      }
    } else {
      Warning(
          "Audio Direct Sound: create primary DirectSound8 buffer failed %s.\n",
          source::windows::make_windows_errno_info(hr).description);
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
      memset(&primary_buffer, 0, sizeof(primary_buffer));
      primary_buffer.dwSize = sizeof(DSBUFFERDESC);
      // NOTE: don't use CTRLFREQUENCY (slow)
      primary_buffer.dwFlags = DSBCAPS_LOCSOFTWARE;
      primary_buffer.dwBufferBytes = SECONDARY_BUFFER_SIZE;
      primary_buffer.lpwfxFormat = &format;

      hr = pDS->CreateSoundBuffer(&primary_buffer, &ds_buffer_, nullptr);

      if (DS_OK != hr) {
        Warning(
            "Audio Direct Sound: create secondary DirectSound8 buffer failed "
            "%s.\n",
            source::windows::make_windows_errno_info(hr).description);
        Shutdown();
        return SIS_FAILURE;
      }

      device_channels_count_ = format.nChannels;
      device_bits_per_sample_ = format.wBitsPerSample;
      device_samples_per_second_ = format.nSamplesPerSec;

      memset(&base_capabilities, 0, sizeof(base_capabilities));
      base_capabilities.dwSize = sizeof(base_capabilities);

      hr = ds_buffer_->GetCaps(&base_capabilities);

      if (DS_OK != hr) {
        Warning("Audio Direct Sound: GetCaps failed %s.\n",
                source::windows::make_windows_errno_info(hr).description);
        Shutdown();
        return SIS_FAILURE;
      }

      if (snd_firsttime)
        DevMsg("Audio Direct Sound: using secondary sound buffer\n");
    } else {
      hr = pDS->SetCooperativeLevel(*pmainwindow, DSSCL_WRITEPRIMARY);

      if (DS_OK != hr) {
        Warning("Audio Direct Sound: set coop level failed %s.\n",
                source::windows::make_windows_errno_info(hr).description);
        Shutdown();
        return SIS_FAILURE;
      }

      memset(&base_capabilities, 0, sizeof(base_capabilities));
      base_capabilities.dwSize = sizeof(base_capabilities);

      hr = ds_p_buffer_->GetCaps(&base_capabilities);

      if (DS_OK != hr) {
        Msg("Audio Direct Sound: GetCaps failed %s.\n",
            source::windows::make_windows_errno_info(hr).description);
        return SIS_FAILURE;
      }

      ds_buffer_ = ds_p_buffer_;
      DevMsg("Audio Direct Sound: using primary sound buffer\n");
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
    device_buffer_size_bytes_ = base_capabilities.dwBufferBytes;
    int reps = 0;
    while (
        (hr = ds_buffer_->Lock(0, device_buffer_size_bytes_, (void **)&lpData,
                               &dwSize, nullptr, nullptr, 0)) != DS_OK) {
      if (hr != DSERR_BUFFERLOST) {
        Warning("Audio Direct Sound: lock Sound Buffer Failed\n");
        Shutdown();
        return SIS_FAILURE;
      }

      if (++reps > 10000) {
        Warning("Audio Direct Sound: couldn't restore buffer\n");
        Shutdown();
        return SIS_FAILURE;
      }
    }

    memset(lpData, 0, dwSize);
    ds_buffer_->Unlock(lpData, dwSize, nullptr, 0);

    // Make sure mixer is active (this was moved after the zeroing to avoid
    // popping on startup -- at least when using the dx9.0b debug .dlls)
    ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);

    // we don't want anyone to access the buffer directly w/o locking it first.
    lpData = nullptr;

    ds_buffer_->Stop();
    ds_buffer_->GetCurrentPosition(&output_buffer_start_offset_bytes_,
                                   &dwWrite);
    ds_buffer_->Play(0, 0, DSBPLAY_LOOPING);
  }

  // number of mono samples output buffer may hold
  device_samples_count_ = device_buffer_size_bytes_ / (DeviceSampleBytes());

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
      DevMsg("Audio Direct Sound: mono configuration detected\n");

    if (speaker_config == DSSPEAKER_HEADPHONE)
      DevMsg("Audio Direct Sound: headphone configuration detected\n");

    if (speaker_config == DSSPEAKER_STEREO)
      DevMsg("Audio Direct Sound: stereo speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_QUAD)
      DevMsg("Audio Direct Sound: quad speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_SURROUND)
      DevMsg("Audio Direct Sound: surround speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_5POINT1)
      DevMsg("Audio Direct Sound: 5.1 speaker configuration detected\n");

    if (speaker_config == DSSPEAKER_7POINT1)
      DevMsg("Audio Direct Sound: 7.1 speaker configuration detected\n");

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

// Updates windows settings based on snd_surround_speakers cvar changing. This
// should only happen if the user has changed it via the console or the UI
// Changes won't take effect until the engine has restarted.
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

  Msg("Audio Direct Sound: speaker configuration has been changed to %s.\n",
      speakerConfigDesc);

  // restart sound system so it takes effect
  g_pSoundServices->RestartSoundSystem();
}

void OnSndSurroundLegacyChanged(IConVar *pVar, const ch *pOldString,
                                float flOldValue) {
  if (pDS && CAudioDirectSound::singleton_) {
    ConVarRef var(pVar);
    // should either be interleaved or have legacy surround set, not both
    if (CAudioDirectSound::singleton_->IsInterleaved() == var.GetBool()) {
      Msg("Audio Direct Sound: legacy Surround %s.\n",
          var.GetBool() ? "enabled" : "disabled");
      // restart sound system so it takes effect
      g_pSoundServices->RestartSoundSystem();
    }
  }
}

// Release all Surround buffer pointers
void CAudioDirectSound::ReleaseSurround() {
  if (ds_3d_buf_fl_ != nullptr) {
    ds_3d_buf_fl_->Release();
    ds_3d_buf_fl_ = nullptr;
  }

  if (ds_3d_buf_fr_ != nullptr) {
    ds_3d_buf_fr_->Release();
    ds_3d_buf_fr_ = nullptr;
  }

  if (ds_3d_buf_rl_ != nullptr) {
    ds_3d_buf_rl_->Release();
    ds_3d_buf_rl_ = nullptr;
  }

  if (ds_3d_buf_rr_ != nullptr) {
    ds_3d_buf_rr_->Release();
    ds_3d_buf_rr_ = nullptr;
  }

  if (ds_buf_fl_ != nullptr) {
    ds_buf_fl_->Release();
    ds_buf_fl_ = nullptr;
  }

  if (ds_buf_fr_ != nullptr) {
    ds_buf_fr_->Release();
    ds_buf_fr_ = nullptr;
  }

  if (ds_buf_rl_ != nullptr) {
    ds_buf_rl_->Release();
    ds_buf_rl_ = nullptr;
  }

  if (ds_buf_rr_ != nullptr) {
    ds_buf_rr_->Release();
    ds_buf_rr_ = nullptr;
  }

  if (ds_bug_fc_ != nullptr) {
    ds_bug_fc_->Release();
    ds_bug_fc_ = nullptr;
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
void DS3D_SetBufferParams(IDirectSound3DBuffer8 *pDSBuf3D, D3DVECTOR *pbpos,
                          D3DVECTOR *pbdir) {
  D3DVECTOR velocity = {0.0f, 0.0f, 0.0f};
  D3DVECTOR position = *pbpos, bdir = *pbdir;
  DS3DBUFFER buffer = {sizeof(DS3DBUFFER)};

  HRESULT hr = pDSBuf3D->GetAllParameters(&buffer);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: 3d buffer get all parameters failed %s.\n",
            source::windows::make_windows_errno_info(hr).description);
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
    Warning("Audio Direct Sound: 3d buffer set all parameters failed %s.\n",
            source::windows::make_windows_errno_info(hr).description);
  }
}

// Initialization for Surround sound support (4 channel or 5 channel).
// Creates 4 or 5 mono 3D buffers to be used as Front Left, (Front Center),
// Front Right, Rear Left, Rear Right
bool CAudioDirectSound::SNDDMA_InitSurround(IDirectSound8 *lpDS,
                                            WAVEFORMATEX *lpFormat,
                                            DSBCAPS *lpdsbc, int cchan) {
  if (lpDS == nullptr) return FALSE;

  DWORD dwSize, dwWrite;
  int reps;
  void *lpData = nullptr;

  // Force format to mono channel
  WAVEFORMATEX wvex;
  memcpy(&wvex, lpFormat, sizeof(*lpFormat));
  wvex.nChannels = 1;
  wvex.nBlockAlign = wvex.nChannels * wvex.wBitsPerSample / 8;
  wvex.nAvgBytesPerSec = wvex.nSamplesPerSec * wvex.nBlockAlign;

  DSBUFFERDESC ds_buf_desc = {sizeof(DSBUFFERDESC)};
  // NOTE: LOCHARDWARE causes SB AWE64 to crash in it's DSOUND driver.
  // Don't use CTRLFREQUENCY (slow)
  ds_buf_desc.dwFlags = DSBCAPS_CTRL3D;
  // Reserve space for each buffer
  ds_buf_desc.dwBufferBytes = SECONDARY_BUFFER_SIZE_SURROUND;
  ds_buf_desc.lpwfxFormat = &wvex;

  // create 4 mono buffers FL, FR, RL, RR
  HRESULT hr = lpDS->CreateSoundBuffer(&ds_buf_desc, &ds_buf_fl_, nullptr);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: CreateSoundBuffer for 3d front left failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return false;
  }

  hr = lpDS->CreateSoundBuffer(&ds_buf_desc, &ds_buf_fr_, nullptr);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: CreateSoundBuffer for 3d front right failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return false;
  }

  hr = lpDS->CreateSoundBuffer(&ds_buf_desc, &ds_buf_rl_, nullptr);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: CreateSoundBuffer for 3d rear left failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return false;
  }

  hr = lpDS->CreateSoundBuffer(&ds_buf_desc, &ds_buf_rr_, nullptr);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: CreateSoundBuffer for 3d rear right failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return false;
  }

  // create center channel

  if (cchan == 5) {
    hr = lpDS->CreateSoundBuffer(&ds_buf_desc, &ds_bug_fc_, nullptr);
    if (DS_OK != hr) {
      Warning(
          "Audio Direct Sound: CreateSoundBuffer for 3d front center failed "
          "%s.\n",
          source::windows::make_windows_errno_info(hr).description);
      ReleaseSurround();
      return false;
    }
  }

  // Try to get 4 or 5 3D buffers from the mono DS buffers

  hr = ds_buf_fl_->QueryInterface(IID_IDirectSound3DBuffer,
                                  (void **)&ds_3d_buf_fl_);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: query 3d buffer for 3d front left failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return FALSE;
  }

  hr = ds_buf_fr_->QueryInterface(IID_IDirectSound3DBuffer,
                                  (void **)&ds_3d_buf_fr_);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: query 3d buffer for 3d front right failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return FALSE;
  }

  hr = ds_buf_rl_->QueryInterface(IID_IDirectSound3DBuffer,
                                  (void **)&ds_3d_buf_rl_);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: query 3d buffer for 3d rear left failed %s.\n",
            source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return FALSE;
  }

  hr = ds_buf_rr_->QueryInterface(IID_IDirectSound3DBuffer,
                                  (void **)&ds_3d_buf_rr_);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: query 3d buffer for 3d rear right failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return FALSE;
  }

  if (cchan == 5) {
    hr = ds_bug_fc_->QueryInterface(IID_IDirectSound3DBuffer,
                                    (void **)&ds_3d_buf_fc_);
    if (DS_OK != hr) {
      Warning(
          "Audio Direct Sound: query 3d buffer for 3d front center failed "
          "%s.\n",
          source::windows::make_windows_errno_info(hr).description);
      ReleaseSurround();
      return FALSE;
    }
  }

  // set listener position & orientation.
  // DS uses left handed coord system: +x is right, +y is up, +z is forward

  IDirectSound3DListener *sound_3d_listener = nullptr;

  hr = ds_p_buffer_->QueryInterface(IID_IDirectSound3DListener,
                                    (void **)&sound_3d_listener);
  if (SUCCEEDED(hr)) {
    DS3DLISTENER lparm = {sizeof(DS3DLISTENER)};

    hr = sound_3d_listener->GetAllParameters(&lparm);
    if (DS_OK != hr) {
      Warning("Audio Direct Sound: 3d listener get all parameters failed %s.\n",
              source::windows::make_windows_errno_info(hr).description);
    }

    // front x,y,z top x,y,z
    hr = sound_3d_listener->SetOrientation(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                                           DS3D_IMMEDIATE);
    if (DS_OK != hr) {
      Warning("Audio Direct Sound: 3d listener set orientation failed %s.\n",
              source::windows::make_windows_errno_info(hr).description);
    }

    hr = sound_3d_listener->SetPosition(0.0f, 0.0f, 0.0f, DS3D_IMMEDIATE);
    if (DS_OK != hr) {
      Warning("Audio Direct Sound: 3d listener set position failed %s.\n",
              source::windows::make_windows_errno_info(hr).description);
    }
  } else {
    Warning("Audio Direct Sound: failed to get 3D listener interface %s.\n",
            source::windows::make_windows_errno_info(hr).description);
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

  DS3D_SetBufferParams(ds_3d_buf_fl_, &bpos, &bdir);

  bpos.x = 1.0;
  bpos.y = 0.0;
  bpos.z = 1.0;  // FR
  bdir.x = -1.0;
  bdir.y = 0.0;
  bdir.z = -1.0;

  DS3D_SetBufferParams(ds_3d_buf_fr_, &bpos, &bdir);

  bpos.x = -1.0;
  bpos.y = 0.0;
  bpos.z = -1.0;  // RL
  bdir.x = 1.0;
  bdir.y = 0.0;
  bdir.z = 1.0;

  DS3D_SetBufferParams(ds_3d_buf_rl_, &bpos, &bdir);

  bpos.x = 1.0;
  bpos.y = 0.0;
  bpos.z = -1.0;  // RR
  bdir.x = -1.0;
  bdir.y = 0.0;
  bdir.z = 1.0;

  DS3D_SetBufferParams(ds_3d_buf_rr_, &bpos, &bdir);

  if (cchan == 5) {
    bpos.x = 0.0;
    bpos.y = 0.0;
    bpos.z = 1.0;  // FC
    bdir.x = 0.0;
    bdir.y = 0.0;
    bdir.z = -1.0;

    DS3D_SetBufferParams(ds_3d_buf_fc_, &bpos, &bdir);
  }

  // commit all buffer param settings

  hr = sound_3d_listener->CommitDeferredSettings();
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: 3d listener commit defered settings failed %s.\n",
        source::windows::make_windows_errno_info(hr).description);
  }

  device_channels_count_ = 1;  // 1 mono 3d output buffer
  device_bits_per_sample_ = lpFormat->wBitsPerSample;
  device_samples_per_second_ = lpFormat->nSamplesPerSec;

  memset(lpdsbc, 0, sizeof(DSBCAPS));
  lpdsbc->dwSize = sizeof(DSBCAPS);

  hr = ds_buf_fl_->GetCaps(lpdsbc);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: GetCaps failed for 3d sound buffer %s.\n",
            source::windows::make_windows_errno_info(hr).description);
    ReleaseSurround();
    return FALSE;
  }

  hr = ds_buf_fl_->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: play failed for front left sound buffer %s.\n",
            source::windows::make_windows_errno_info(hr).description);
  }

  hr = ds_buf_fr_->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning(
        "Audio Direct Sound: play failed for front right sound buffer %s.\n",
        source::windows::make_windows_errno_info(hr).description);
  }

  hr = ds_buf_rl_->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: play failed for rear left sound buffer %s.\n",
            source::windows::make_windows_errno_info(hr).description);
  }

  hr = ds_buf_rr_->Play(0, 0, DSBPLAY_LOOPING);
  if (DS_OK != hr) {
    Warning("Audio Direct Sound: play failed for rear right sound buffer %s.\n",
            source::windows::make_windows_errno_info(hr).description);
  }

  if (cchan == 5) {
    hr = ds_bug_fc_->Play(0, 0, DSBPLAY_LOOPING);
    if (DS_OK != hr) {
      Warning(
          "Audio Direct Sound: play failed for front center sound buffer %s.\n",
          source::windows::make_windows_errno_info(hr).description);
    }
  }

  if (snd_firsttime)
    DevMsg(
        "   %d channel(s)\n"
        "   %d bits/sample\n"
        "   %d samples/sec\n",
        cchan, DeviceSampleBits(), DeviceDmaSpeed());

  device_buffer_size_bytes_ = lpdsbc->dwBufferBytes;

  // Test everything just like in the normal initialization.
  if (cchan == 5) {
    reps = 0;
    while ((hr = ds_bug_fc_->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                  &dwSize, nullptr, nullptr, 0)) != DS_OK) {
      if (hr != DSERR_BUFFERLOST) {
        Warning("Audio Direct Sound: lock Sound Buffer Failed for FC\n");
        ReleaseSurround();
        return FALSE;
      }

      if (++reps > 10000) {
        Warning("Audio Direct Sound: couldn't restore buffer for FC\n");
        ReleaseSurround();
        return FALSE;
      }
    }
    memset(lpData, 0, dwSize);
    //		DEBUG_DS_FillSquare( lpData, dwSize );
    ds_bug_fc_->Unlock(lpData, dwSize, nullptr, 0);
  }

  reps = 0;
  while ((hr = ds_buf_fl_->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                &dwSize, nullptr, nullptr, 0)) != DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Warning("Audio Direct Sound: lock Sound Buffer Failed for 3d FL\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("Audio Direct Sound: couldn't restore buffer for 3d FL\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  ds_buf_fl_->Unlock(lpData, dwSize, nullptr, 0);

  reps = 0;
  while ((hr = ds_buf_fr_->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                &dwSize, nullptr, nullptr, 0)) != DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Warning("Audio Direct Sound: lock Sound Buffer Failed for 3d FR\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("Audio Direct Sound: couldn't restore buffer for FR\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  ds_buf_fr_->Unlock(lpData, dwSize, nullptr, 0);

  reps = 0;
  while ((hr = ds_buf_rl_->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                &dwSize, nullptr, nullptr, 0)) != DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Warning("Audio Direct Sound: lock Sound Buffer Failed for RL\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("Audio Direct Sound: couldn't restore buffer for RL\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  ds_buf_rl_->Unlock(lpData, dwSize, nullptr, 0);

  reps = 0;
  while ((hr = ds_buf_rr_->Lock(0, lpdsbc->dwBufferBytes, (void **)&lpData,
                                &dwSize, nullptr, nullptr, 0)) != DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Warning("Audio Direct Sound: lock Sound Buffer Failed for RR\n");
      ReleaseSurround();
      return FALSE;
    }

    if (++reps > 10000) {
      Warning("Audio Direct Sound: couldn't restore buffer for RR\n");
      ReleaseSurround();
      return FALSE;
    }
  }
  memset(lpData, 0, dwSize);
  //	DEBUG_DS_FillSquare( lpData, dwSize );
  ds_buf_rr_->Unlock(lpData, dwSize, nullptr, 0);

  lpData = nullptr;  // this is invalid now

  // OK Stop and get our positions and were good to go.
  ds_buf_fl_->Stop();
  ds_buf_fr_->Stop();
  ds_buf_rl_->Stop();
  ds_buf_rr_->Stop();
  if (cchan == 5) ds_bug_fc_->Stop();

  // get hardware playback position, store it, syncronize all buffers to FL

  ds_buf_fl_->GetCurrentPosition(&output_buffer_start_offset_bytes_, &dwWrite);
  ds_buf_fr_->SetCurrentPosition(output_buffer_start_offset_bytes_);
  ds_buf_rl_->SetCurrentPosition(output_buffer_start_offset_bytes_);
  ds_buf_rr_->SetCurrentPosition(output_buffer_start_offset_bytes_);
  if (cchan == 5)
    ds_bug_fc_->SetCurrentPosition(output_buffer_start_offset_bytes_);

  ds_buf_fl_->Play(0, 0, DSBPLAY_LOOPING);
  ds_buf_fr_->Play(0, 0, DSBPLAY_LOOPING);
  ds_buf_rl_->Play(0, 0, DSBPLAY_LOOPING);
  ds_buf_rr_->Play(0, 0, DSBPLAY_LOOPING);
  if (cchan == 5) ds_bug_fc_->Play(0, 0, DSBPLAY_LOOPING);

  if (snd_firsttime)
    Warning(
        "Audio Direct Sound: 3d surround sound initialization successful\n");

  return true;
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
    if (is_interleaved_) {
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
    S_TransferSurround16Interleaved(PAINTBUFFER, nullptr, nullptr, lpaintedtime,
                                    endtime);
  } else {
    DWORD *pBuffer = nullptr;
    DWORD dwSize = 0;
    if (!LockDSBuffer(ds_buffer_, &pBuffer, &dwSize, "DS_STEREO")) {
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
      ds_buffer_->Unlock(pBuffer, dwSize, nullptr, 0);
    }
  }
}

bool CAudioDirectSound::IsUsingBufferPerSpeaker() {
  return m_bSurround && !is_interleaved_;
}

bool CAudioDirectSound::LockDSBuffer(IDirectSoundBuffer *pBuffer,
                                     DWORD **pdwWriteBuffer,
                                     DWORD *pdwSizeBuffer,
                                     const ch *pBufferName, int lockFlags) {
  if (!pBuffer) return false;

  HRESULT hr;
  uint32_t reps = 0;

  while ((hr = pBuffer->Lock(0, device_buffer_size_bytes_,
                             (void **)pdwWriteBuffer, pdwSizeBuffer, nullptr,
                             nullptr, lockFlags)) != DS_OK) {
    if (hr != DSERR_BUFFERLOST) {
      Msg("Audio Direct Sound: lock sound buffer %s failed (0x%.8x).\n",
          pBufferName, hr);
      return false;
    }

    if (++reps > 10000) {
      Msg("Audio Direct Sound: couldn't restore sound buffer %s (0x%.8x).\n",
          pBufferName, hr);
      return false;
    }
  }

  return true;
}

// Given front, rear and center stereo paintbuffers, split samples into 4 or 5
// mono directsound buffers (FL, FC, FR, RL, RR)
void CAudioDirectSound::S_TransferSurround16(portable_samplepair_t *pfront,
                                             portable_samplepair_t *prear,
                                             portable_samplepair_t *pcenter,
                                             int lpaintedtime, int endtime,
                                             int cchan) {
  int lpos;
  DWORD *pdwWriteFL = nullptr, *pdwWriteFR = nullptr, *pdwWriteRL = nullptr,
        *pdwWriteRR = nullptr, *pdwWriteFC = nullptr;
  DWORD dwSizeFL = 0, dwSizeFR = 0, dwSizeRL = 0, dwSizeRR = 0, dwSizeFC = 0;
  int i, j, *snd_p, *snd_rp, *snd_cp, volumeFactor;
  short *snd_out_fleft, *snd_out_fright, *snd_out_rleft, *snd_out_rright,
      *snd_out_fcenter;

  pdwWriteFC = nullptr;  // compiler warning
  dwSizeFC = 0;
  snd_out_fcenter = nullptr;

  volumeFactor = S_GetMasterVolume() * 256;

  // lock all 4 or 5 mono directsound buffers FL, FR, RL, RR, FC
  if (!LockDSBuffer(ds_buf_fl_, &pdwWriteFL, &dwSizeFL, "FL") ||
      !LockDSBuffer(ds_buf_fr_, &pdwWriteFR, &dwSizeFR, "FR") ||
      !LockDSBuffer(ds_buf_rl_, &pdwWriteRL, &dwSizeRL, "RL") ||
      !LockDSBuffer(ds_buf_rr_, &pdwWriteRR, &dwSizeRR, "RR")) {
    S_Shutdown();
    S_Startup();
    return;
  }

  if (cchan == 5 && !LockDSBuffer(ds_bug_fc_, &pdwWriteFC, &dwSizeFC, "FC")) {
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

  ds_buf_fl_->Unlock(pdwWriteFL, dwSizeFL, nullptr, 0);
  ds_buf_fr_->Unlock(pdwWriteFR, dwSizeFR, nullptr, 0);
  ds_buf_rl_->Unlock(pdwWriteRL, dwSizeRL, nullptr, 0);
  ds_buf_rr_->Unlock(pdwWriteRR, dwSizeRR, nullptr, 0);

  if (cchan == 5) ds_bug_fc_->Unlock(pdwWriteFC, dwSizeFC, nullptr, 0);
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
  DWORD *pdwWrite = nullptr;
  DWORD dwSize = 0;
  int i, j, *snd_p, *snd_rp, *snd_cp, volumeFactor;

  volumeFactor = S_GetMasterVolume() * 256;
  int channelCount = m_bSurroundCenter ? 5 : 4;
  if (DeviceChannels() == 2) {
    channelCount = 2;
  }

  // lock single interleaved buffer
  if (!LockDSBuffer(ds_buffer_, &pdwWrite, &dwSize, "DS_INTERLEAVED")) {
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
  int sampleMonoCount = device_buffer_size_bytes_ /
                        (DeviceSampleBytes() *
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

  ds_buffer_->Unlock(pdwWrite, dwSize, nullptr, 0);
}

void CAudioDirectSound::S_TransferSurround16Interleaved(
    const portable_samplepair_t *pfront, const portable_samplepair_t *prear,
    const portable_samplepair_t *pcenter, int lpaintedtime, int endtime) {
  if (!ds_buffer_) return;
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
  void *pBuffer0 = nullptr;
  void *pBuffer1 = nullptr;
  DWORD size0, size1;
  int lpos =
      transfer.paintedtime &
      transfer.sampleMask;  // lpos is next output position in output buffer

  int offset = lpos * 2 * channelCount;
  int lockSize = transfer.linearCount * 2 * channelCount;
  int reps = 0;
  HRESULT hr;
  while ((hr = ds_buffer_->Lock(offset, lockSize, &pBuffer0, &size0, &pBuffer1,
                                &size1, 0)) != DS_OK) {
    if (hr == DSERR_BUFFERLOST) {
      if (++reps < 10000) continue;
    }
    Msg("Audio Direct Sound: lock Sound Buffer Failed\n");
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
  ds_buffer_->Unlock(pBuffer0, size0, pBuffer1, size1);
}
