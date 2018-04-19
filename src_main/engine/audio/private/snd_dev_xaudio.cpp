// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: XAudio2 Version.

#include "audio_pch.h"

#include "snd_dev_xaudio.h"

// Enable when finish port to XAudio2
#if 0

#include "snd_dev_xaudio.h"

#include <Mmdeviceapi.h>
#include <xaudio2.h>
#include <atomic>

#include "client.h"
#include "matchmaking.h"
#include "server.h"
#include "session.h"
#include "tier1/utllinkedlist.h"

#include "tier0/include/memdbgon.h"

// Assuming worst case 5fps = 200ms @ 44.1Khz = ~8K samples.
#define XAUDIO_BUFFER_SAMPLES 8192
// Packet return has a latency, so need a decent pool.
#define MAX_XAUDIO_PACKETS 32

#define SURROUND_HEADPHONES 0
#define SURROUND_STEREO 2
#define SURROUND_DIGITAL5DOT1 5

ConVar snd_xaudio_spew_packets("snd_xaudio_spew_packets", "0", 0,
                               "Spew XAudio packet delivery");

// Implementation of XAudio2
class CAudioXAudio2 : public CAudioDeviceBase {
 public:
  virtual ~CAudioXAudio2();

  bool IsActive() override { return true; }
  bool Init() override;
  void Shutdown() override;

  void Pause() override;
  void UnPause() override;
  int PaintBegin(float mixAheadTime, int soundtime, int paintedtime) override;
  int GetOutputPosition() override;
  void ClearBuffer() override;
  void TransferSamples(int end) override;

  const ch *DeviceName() override;
  int DeviceChannels() override { return m_deviceChannels; }
  int DeviceSampleBits() override { return m_deviceSampleBits; }
  int DeviceSampleBytes() override { return m_deviceSampleBits / 8; }
  int DeviceDmaSpeed() override { return m_deviceDmaSpeed; }
  int DeviceSampleCount() override { return m_deviceSampleCount; }

  void XAudio2PacketCallback(int hCompletedBuffer);

  static CAudioXAudio2 *m_pSingleton;

  CXboxVoice *GetVoiceData() const { return &m_VoiceData; }

 private:
  int TransferStereo(const portable_samplepair_t *pFront, int paintedTime,
                     int endTime, ch *pOutptuBuffer);
  int TransferSurroundInterleaved(const portable_samplepair_t *pFront,
                                  const portable_samplepair_t *pRear,
                                  const portable_samplepair_t *pCenter,
                                  int paintedTime, int endTime,
                                  ch *pOutputBuffer);

  int m_deviceChannels;  // channels per hardware output buffer (1 for quad/5.1,
                         // 2 for stereo)
  int m_deviceSampleBits;   // bits per sample (16)
  int m_deviceSampleCount;  // count of mono samples in output buffer
  int m_deviceDmaSpeed;     // samples per second per output buffer

  XAUDIOPACKET m_Packets[MAX_XAUDIO_PACKETS];
  IXAudio2 *m_pXAudio2;
  IXAudio2SourceVoice *m_pSourceVoice;

  ch *m_pOutputBuffer;
  int m_bufferSizeBytes;  // size of a single hardware output buffer, in bytes

  std::atomic_uint32_t m_PacketTail;
  std::atomic_uint32_t m_PacketHead;

  CXboxVoice m_VoiceData;
};

CAudioXAudio2 *CAudioXAudio2::m_pSingleton = nullptr;

// XAudio packet completion callback
void CALLBACK XAudioPacketCallback(LPCXAUDIOVOICEPACKETCALLBACK pCallbackData) {
  CAudioXAudio2::m_pSingleton->XAudio2PacketCallback(
      (int)pCallbackData->pPacketContext);
}

// Create XAudio Device
IAudioDevice *Audio_CreateXAudioDevice() {
  MEM_ALLOC_CREDIT();

  if (!CAudioXAudio2::m_pSingleton) {
    CAudioXAudio2::m_pSingleton = new CAudioXAudio2;
  }

  if (!CAudioXAudio2::m_pSingleton->Init()) {
    delete CAudioXAudio2::m_pSingleton;
    CAudioXAudio2::m_pSingleton = nullptr;
  }

  return CAudioXAudio2::m_pSingleton;
}

CXboxVoice *Audio_GetXVoice() {
  if (CAudioXAudio2::m_pSingleton) {
    return CAudioXAudio2::m_pSingleton->GetVoiceData();
  }

  return nullptr;
}

CAudioXAudio2::~CAudioXAudio2() { m_pSingleton = nullptr; }

// Initialize XAudio2
bool CAudioXAudio2::Init() {
  HRESULT hr = XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  if (FAILED(hr)) {
    Warning("XAudio2Create can't create XAudio2 device (0x%.8x).\n", hr);
    return false;
  }

  // XAUDIOSPEAKERCONFIG xAudioConfig = 0;
  // XAudioGetSpeakerConfig(&xAudioConfig);
  snd_surround.SetValue(
      // (xAudioConfig & XAUDIOSPEAKERCONFIG_DIGITAL_DOLBYDIGITAL)
      //    ? SURROUND_DIGITAL5DOT1
      /*    : */ SURROUND_STEREO);

  m_bHeadphone = false;
  m_bSurround = false;
  m_bSurroundCenter = false;

  switch (snd_surround.GetInt()) {
    case SURROUND_HEADPHONES:
      m_bHeadphone = true;
      m_deviceChannels = 2;
      break;

    default:
    case SURROUND_STEREO:
      m_deviceChannels = 2;
      break;

    case SURROUND_DIGITAL5DOT1:
      m_bSurround = true;
      m_bSurroundCenter = true;
      m_deviceChannels = 6;
      break;
  }

  m_deviceSampleBits = 16;
  m_deviceDmaSpeed = SOUND_DMA_SPEED;

  XAUDIOCHANNELMAPENTRY channelMapEntries[] = {
      {XAUDIOSPEAKER_FRONTLEFT, XAUDIOSPEAKER_FRONTLEFT, 1.0f},
      {XAUDIOSPEAKER_FRONTRIGHT, XAUDIOSPEAKER_FRONTRIGHT, 1.0f},
      {XAUDIOSPEAKER_FRONTCENTER, XAUDIOSPEAKER_FRONTCENTER, 1.0f},
      {XAUDIOSPEAKER_LOWFREQUENCY, XAUDIOSPEAKER_LOWFREQUENCY, 1.0f},
      {XAUDIOSPEAKER_BACKLEFT, XAUDIOSPEAKER_BACKLEFT, 1.0f},
      {XAUDIOSPEAKER_BACKRIGHT, XAUDIOSPEAKER_BACKRIGHT, 1.0f}};
  XAUDIOCHANNELMAP channelMap = {6, channelMapEntries};
  XAUDIOVOICEOUTPUTENTRY voiceOutputEntry = {NULL, &channelMap};
  XAUDIOVOICEOUTPUT voiceOutput = {1, &voiceOutputEntry};

  if (!m_bSurround) {
    channelMapEntries[XAUDIOSPEAKER_FRONTCENTER].Volume = 0;
    channelMapEntries[XAUDIOSPEAKER_LOWFREQUENCY].Volume = 0;
    channelMapEntries[XAUDIOSPEAKER_BACKLEFT].Volume = 0;
    channelMapEntries[XAUDIOSPEAKER_BACKRIGHT].Volume = 0;
  }

  // setup the playback buffer
  XAUDIOSOURCEVOICEINIT SourceVoiceInit = {0};
  SourceVoiceInit.Format.SampleType = XAUDIOSAMPLETYPE_16BITPCM;
  SourceVoiceInit.Format.ChannelCount = m_deviceChannels;
  SourceVoiceInit.Format.SampleRate = m_deviceDmaSpeed;
  SourceVoiceInit.MaxOutputVoiceCount = 1;
  SourceVoiceInit.MaxPacketCount = MAX_XAUDIO_PACKETS;
  SourceVoiceInit.MaxChannelMapEntryCount = 6;
  SourceVoiceInit.pVoiceOutput = &voiceOutput;
  SourceVoiceInit.pfnPacketCompletionCallback = ::XAudioPacketCallback;

  m_pSourceVoice = NULL;
  hr = XAudioCreateSourceVoice(&SourceVoiceInit, &m_pSourceVoice);
  if (FAILED(hr)) return false;

  m_bufferSizeBytes =
      XAUDIO_BUFFER_SAMPLES * (m_deviceSampleBits / 8) * m_deviceChannels;
  m_pOutputBuffer = new ch[MAX_XAUDIO_PACKETS * m_bufferSizeBytes];
  ClearBuffer();

  V_memset(m_Packets, 0, MAX_XAUDIO_PACKETS * sizeof(XAUDIOPACKET));
  for (int i = 0; i < MAX_XAUDIO_PACKETS; i++) {
    m_Packets[i].pBuffer = m_pOutputBuffer + i * m_bufferSizeBytes;
    m_Packets[i].pContext = (LPVOID)i;
  }
  m_PacketHead = 0;
  m_PacketTail = 0;

  // number of mono samples output buffer may hold
  m_deviceSampleCount = m_bufferSizeBytes / (DeviceSampleBytes());

  hr = m_pSourceVoice->Start(0);
  if (FAILED(hr)) return false;

  DevMsg("XAudio Device Initialized:\n");
  DevMsg(
      "   %s\n"
      "   %d channel(s)\n"
      "   %d bits/sample\n"
      "   %d samples/sec\n",
      DeviceName(), DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed());

  m_VoiceData.VoiceInit();

  // success
  return true;
}

// Shutdown XAudio2
void CAudioXAudio2::Shutdown() {
  if (m_pSourceVoice) {
    m_pSourceVoice->Stop(0);
    m_pSourceVoice->DestroyVoice();

    delete[] m_pOutputBuffer;
  }

  if (m_pXAudio2) {
    m_pXAudio2->Release();
    m_pXAudio2 = nullptr;
  }

  if (this == CAudioXAudio2::m_pSingleton) {
    CAudioXAudio2::m_pSingleton = nullptr;
  }
}

// XAudio has completed a packet. Assuming these are sequential
void CAudioXAudio2::XAudio2PacketCallback(int hCompletedBuffer) {
  // packet completion expected to be sequential
  Assert(hCompletedBuffer == (int)(m_PacketTail % MAX_XAUDIO_PACKETS));

  m_PacketTail++;

  if (snd_xaudio_spew_packets.GetBool()) {
    if (m_PacketTail == m_PacketHead) {
      Warning("XAudio: Starved\n");
    } else {
      Msg("XAudio: Packet Callback, Submit: %2d, Free: %2d\n",
          m_PacketHead - m_PacketTail,
          MAX_XAUDIO_PACKETS - (m_PacketHead - m_PacketTail));
    }
  }

  if (m_PacketTail == m_PacketHead) {
    // very bad, out of packets, xaudio is starving
    // mix thread didn't keep up with audio clock and submit packets
    // submit a silent buffer to keep stream playing and audio clock running
    int head = m_PacketHead++;
    XAUDIOPACKET *pPacket = &m_Packets[head % MAX_XAUDIO_PACKETS];
    V_memset(pPacket->pBuffer, 0, m_bufferSizeBytes);
    pPacket->BufferSize = m_bufferSizeBytes;
    m_pSourceVoice->SubmitPacket(pPacket, XAUDIOSUBMITPACKET_DISCONTINUITY);
  }
}

// Return the "write" cursor.  Used to clock the audio mixing.
// The actual hw write cursor and the number of samples it fetches is unknown.
int CAudioXAudio2::GetOutputPosition() {
  DWORD tempPosition = 0;
  m_pSourceVoice->GetState(&tempPosition);

  return (tempPosition % XAUDIO_BUFFER_SAMPLES);
}

// Pause playback
void CAudioXAudio2::Pause() {
  if (m_pSourceVoice) {
    m_pSourceVoice->Stop(0);
  }
}

// Resume playback
void CAudioXAudio2::UnPause() {
  if (m_pSourceVoice) {
    m_pSourceVoice->Start(0);
  }
}

//-----------------------------------------------------------------------------
// Calc the paint position
//-----------------------------------------------------------------------------
int CAudioXAudio2::PaintBegin(float mixAheadTime, int soundtime,
                              int paintedtime) {
  //  soundtime = total full samples that have been played out to hardware at
  //  dmaspeed paintedtime = total full samples that have been mixed at speed

  //  endtime = target for full samples in mixahead buffer at speed
  int mixaheadtime = mixAheadTime * DeviceDmaSpeed();
  int endtime = soundtime + mixaheadtime;
  if (endtime <= paintedtime) {
    return endtime;
  }

  int fullsamps = DeviceSampleCount() / DeviceChannels();

  if ((endtime - soundtime) > fullsamps) {
    endtime = soundtime + fullsamps;
  }
  if ((endtime - paintedtime) & 0x03) {
    // The difference between endtime and painted time should align on
    // boundaries of 4 samples.  This is important when upsampling from 11khz ->
    // 44khz.
    endtime -= (endtime - paintedtime) & 0x03;
  }

  return endtime;
}

// Fill the output buffers with silence
void CAudioXAudio2::ClearBuffer() {
  V_memset(m_pOutputBuffer, 0, MAX_XAUDIO_PACKETS * m_bufferSizeBytes);
}

// Fill the output buffer with L/R samples
int CAudioXAudio2::TransferStereo(const portable_samplepair_t *pFrontBuffer,
                                  int paintedTime, int endTime,
                                  ch *pOutputBuffer) {
  int linearCount;
  int i;
  int val;

  int volumeFactor = S_GetMasterVolume() * 256;

  int *pFront = (int *)pFrontBuffer;
  short *pOutput = (short *)pOutputBuffer;

  // get size of output buffer in full samples (LR pairs)
  // number of sequential sample pairs that can be wrriten
  linearCount = g_AudioDevice->DeviceSampleCount() >> 1;

  // clamp output count to requested number of samples
  if (linearCount > endTime - paintedTime) {
    linearCount = endTime - paintedTime;
  }

  // linearCount is now number of mono 16 bit samples (L and R) to xfer.
  linearCount <<= 1;

  // transfer mono 16bit samples multiplying each sample by volume.
  for (i = 0; i < linearCount; i += 2) {
    // L Channel
    val = (pFront[i] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);

    // R Channel
    val = (pFront[i + 1] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);
  }

  return linearCount * DeviceSampleBytes();
}

// Fill the output buffer with interleaved surround samples
int CAudioXAudio2::TransferSurroundInterleaved(
    const portable_samplepair_t *pFrontBuffer,
    const portable_samplepair_t *pRearBuffer,
    const portable_samplepair_t *pCenterBuffer, int paintedTime, int endTime,
    ch *pOutputBuffer) {
  int linearCount;
  int i, j;
  int val;

  int volumeFactor = S_GetMasterVolume() * 256;

  int *pFront = (int *)pFrontBuffer;
  int *pRear = (int *)pRearBuffer;
  int *pCenter = (int *)pCenterBuffer;
  short *pOutput = (short *)pOutputBuffer;

  // number of mono samples per channel
  // number of sequential samples that can be wrriten
  linearCount = m_bufferSizeBytes / (DeviceSampleBytes() * DeviceChannels());

  // clamp output count to requested number of samples
  if (linearCount > endTime - paintedTime) {
    linearCount = endTime - paintedTime;
  }

  for (i = 0, j = 0; i < linearCount; i++, j += 2) {
    // FL
    val = (pFront[j] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);

    // FR
    val = (pFront[j + 1] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);

    // Center
    val = (pCenter[j] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);

    // Let the hardware mix the sub from the main channels since
    // we don't have any sub-specific sounds, or direct sub-addressing
    *pOutput++ = 0;

    // RL
    val = (pRear[j] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);

    // RR
    val = (pRear[j + 1] * volumeFactor) >> 8;
    *pOutput++ = CLIP(val);
  }

  return linearCount * DeviceSampleBytes() * DeviceChannels();
}

// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of samples out to the
// xaudio buffer(s).
void CAudioXAudio2::TransferSamples(int endTime) {
  XAUDIOPACKET *pPacket;

  if (m_PacketHead - m_PacketTail >= MAX_XAUDIO_PACKETS) {
    DevWarning("XAudio: No Free Packets!\n");
    return;
  }

  unsigned int packet = m_PacketHead++;
  pPacket = &m_Packets[packet % MAX_XAUDIO_PACKETS];

  if (!m_bSurround) {
    pPacket->BufferSize = TransferStereo(PAINTBUFFER, g_paintedtime, endTime,
                                         (ch *)pPacket->pBuffer);
  } else {
    pPacket->BufferSize = TransferSurroundInterleaved(
        PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, g_paintedtime, endTime,
        (ch *)pPacket->pBuffer);
  }

  // submit packet
  m_pSourceVoice->SubmitPacket(pPacket, XAUDIOSUBMITPACKET_DISCONTINUITY);
}

//-----------------------------------------------------------------------------
// Get our device name
//-----------------------------------------------------------------------------
const ch *CAudioXAudio2::DeviceName() {
  if (m_bSurround) {
    return "XAudio: 5.1 Channel Surround";
  }

  return "XAudio: Stereo";
}

//-----------------------------------------------------------------------------
// Initialize Voice
//-----------------------------------------------------------------------------
void CXboxVoice::VoiceInit() {
  // Set the processing modes
  XHV_PROCESSING_MODE rgMode = XHV_VOICECHAT_MODE;

  // Set up parameters for the voice chat engine
  XHV_INIT_PARAMS xhvParams = {0};
  xhvParams.dwMaxRemoteTalkers = MAX_PLAYERS;
  xhvParams.dwMaxLocalTalkers = 1;
  xhvParams.localTalkerEnabledModes = &rgMode;
  xhvParams.remoteTalkerEnabledModes = &rgMode;
  xhvParams.dwNumLocalTalkerEnabledModes = 1;
  xhvParams.dwNumRemoteTalkerEnabledModes = 1;

  // Create the engine
  HRESULT hr = XHVCreateEngine(&xhvParams, NULL, &m_pXHVEngine);

  if (hr != S_OK) {
    Warning("Couldn't load XHV engine!\n");
  }

  VoiceResetLocalData();
}

void CXboxVoice::AddPlayerToVoiceList(CClientInfo *pClient, bool bLocal) {
  XHV_PROCESSING_MODE local_proc_mode = XHV_VOICECHAT_MODE;

  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    if (pClient->m_xuids[i] == 0) continue;

    if (bLocal == true) {
      if (m_pXHVEngine->RegisterLocalTalker(pClient->m_iControllers[i]) ==
          S_OK) {
        m_pXHVEngine->StartLocalProcessingModes(pClient->m_iControllers[i],
                                                &local_proc_mode, 1);
      }
    } else {
      if (m_pXHVEngine->RegisterRemoteTalker(pClient->m_xuids[i], NULL, NULL,
                                             NULL) == S_OK) {
        m_pXHVEngine->StartRemoteProcessingModes(pClient->m_xuids[i],
                                                 &local_proc_mode, 1);
      }
    }
  }
}

void CXboxVoice::RemovePlayerFromVoiceList(CClientInfo *pClient, bool bLocal) {
  for (int i = 0; i < pClient->m_cPlayers; ++i) {
    if (pClient->m_xuids[i] == 0) continue;

    if (bLocal == true) {
      m_pXHVEngine->UnregisterLocalTalker(pClient->m_iControllers[i]);
    } else {
      m_pXHVEngine->UnregisterRemoteTalker(pClient->m_xuids[i]);
    }
  }
}

void CXboxVoice::PlayIncomingVoiceData(XUID xuid, const byte *pbData,
                                       DWORD pdwSize) {
  XUID localXUID;

  XUserGetXUID(XBX_GetPrimaryUserId(), &localXUID);

  // Hack: Don't play stuff that comes from ourselves.
  if (localXUID == xuid) return;

  m_pXHVEngine->SubmitIncomingChatData(xuid, pbData, &pdwSize);
}

void CXboxVoice::UpdateHUDVoiceStatus() {
  for (int iClient = 0; iClient < cl.m_nMaxClients; iClient++) {
    bool bSelf = (cl.m_nPlayerSlot == iClient);

    int iIndex = iClient + 1;
    XUID id = g_pMatchmaking->PlayerIdToXuid(iIndex);

    if (id != 0) {
      bool bTalking = false;

      if (bSelf == true) {
        // Make sure the player's own label is not on.
        g_pSoundServices->OnChangeVoiceStatus(iIndex, false);

        iIndex = -1;

        if (IsPlayerTalking(XBX_GetPrimaryUserId(), true)) {
          bTalking = true;
        }
      } else {
        if (IsPlayerTalking(id, false)) {
          bTalking = true;
        }
      }

      g_pSoundServices->OnChangeVoiceStatus(iIndex, bTalking);
    } else {
      g_pSoundServices->OnChangeVoiceStatus(iIndex, false);
    }
  }
}

bool CXboxVoice::VoiceUpdateData() {
  DWORD dwNumPackets = 0;
  DWORD dwBytes = 0;
  WORD wVoiceBytes = 0;
  bool bShouldSend = false;
  DWORD dwVoiceFlags = m_pXHVEngine->GetDataReadyFlags();

  // Update UI stuff.
  UpdateHUDVoiceStatus();

  for (uint i = 0; i < XUSER_MAX_COUNT; ++i) {
    // We currently only allow one player per console
    if (i != XBX_GetPrimaryUserId()) {
      continue;
    }

    if (IsHeadsetPresent(i) == false) continue;

    if (!(dwVoiceFlags & (1 << i))) continue;

    dwBytes = m_ChatBufferSize - m_wLocalDataSize;

    if (dwBytes < XHV_VOICECHAT_MODE_PACKET_SIZE) {
      bShouldSend = true;
    } else {
      m_pXHVEngine->GetLocalChatData(i, m_ChatBuffer + m_wLocalDataSize,
                                     &dwBytes, &dwNumPackets);
      m_wLocalDataSize += ((WORD)dwBytes) & MAXWORD;

      if (m_wLocalDataSize > ((m_ChatBufferSize * 7) / 10)) {
        bShouldSend = true;
      }
    }

    wVoiceBytes += m_wLocalDataSize & MAXWORD;
    break;
  }

  return bShouldSend || (wVoiceBytes && (GetTickCount() - m_dwLastVoiceSend) >
                                            MAX_VOICE_BUFFER_TIME);
}

void CXboxVoice::SetPlaybackPriority(XUID remoteTalker, DWORD dwUserIndex,
                                     XHV_PLAYBACK_PRIORITY playbackPriority) {
  m_pXHVEngine->SetPlaybackPriority(remoteTalker, dwUserIndex,
                                    playbackPriority);
}

void CXboxVoice::GetRemoteTalkers(int *pNumTalkers, XUID *pRemoteTalkers) {
  m_pXHVEngine->GetRemoteTalkers((DWORD *)pNumTalkers, pRemoteTalkers);
}

void CXboxVoice::GetVoiceData(CLC_VoiceData *pMessage) {
  byte *puchVoiceData = NULL;
  pMessage->m_nLength = m_wLocalDataSize;
  XUserGetXUID(XBX_GetPrimaryUserId(), &pMessage->m_xuid);

  puchVoiceData = m_ChatBuffer;

  pMessage->m_DataOut.StartWriting(puchVoiceData, pMessage->m_nLength);
  pMessage->m_nLength *= 8;
  pMessage->m_DataOut.SeekToBit(
      pMessage->m_nLength);  // set correct writing position
}

void CXboxVoice::VoiceSendData(INetChannel *pChannel) {
  CLC_VoiceData voiceMsg;
  GetVoiceData(&voiceMsg);

  if (pChannel) {
    pChannel->SendNetMsg(voiceMsg, false, true);
    VoiceResetLocalData();
  }
}

void CXboxVoice::VoiceResetLocalData() {
  m_wLocalDataSize = 0;
  m_dwLastVoiceSend = GetTickCount();
  Q_memset(m_ChatBuffer, 0, m_ChatBufferSize);
}

bool CXboxVoice::IsPlayerTalking(XUID uid, bool bLocal) {
  if (bLocal) {
    return m_pXHVEngine->IsLocalTalking(XBX_GetPrimaryUserId());
  }

  return !g_pMatchmaking->IsPlayerMuted(XBX_GetPrimaryUserId(), uid) &&
         m_pXHVEngine->IsRemoteTalking(uid);
}

bool CXboxVoice::IsHeadsetPresent(int id) {
  return m_pXHVEngine->IsHeadsetPresent(id);
}

void CXboxVoice::RemoveAllTalkers(CClientInfo *pLocal) {
  int numRemoteTalkers;
  XUID remoteTalkers[MAX_PLAYERS];
  GetRemoteTalkers(&numRemoteTalkers, remoteTalkers);

  for (int iRemote = 0; iRemote < numRemoteTalkers; iRemote++) {
    m_pXHVEngine->UnregisterRemoteTalker(remoteTalkers[iRemote]);
  }

  if (pLocal) {
    for (int i = 0; i < pLocal->m_cPlayers; ++i) {
      if (pLocal->m_xuids[i] == 0) continue;

      m_pXHVEngine->UnregisterLocalTalker(pLocal->m_iControllers[i]);
    }
  }
}

#endif
