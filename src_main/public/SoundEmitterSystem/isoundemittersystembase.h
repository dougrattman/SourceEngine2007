// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ISOUNDEMITTERSYSTEMBASE_H
#define ISOUNDEMITTERSYSTEMBASE_H

#include "appframework/IAppSystem.h"
#include "base/include/base_types.h"
#include "base/include/macros.h"
#include "game/shared/interval.h"
#include "mathlib/compressed_vector.h"
#include "soundflags.h"
#include "tier1/utldict.h"

#define SOUNDEMITTERSYSTEM_INTERFACE_VERSION "VSoundEmitter002"

#define SOUNDGENDER_MACRO "$gender"
#define SOUNDGENDER_MACRO_LENGTH 7  // Length of above including $

using HSOUNDSCRIPTHANDLE = short;
#define SOUNDEMITTER_INVALID_HANDLE (HSOUNDSCRIPTHANDLE) - 1

struct CSoundParameters {
  CSoundParameters() {
    channel = CHAN_AUTO;  // 0
    volume = VOL_NORM;    // 1.0f
    pitch = PITCH_NORM;   // 100

    pitchlow = PITCH_NORM;
    pitchhigh = PITCH_NORM;

    soundlevel = SNDLVL_NORM;  // 75dB
    play_to_owner_only = false;
    count = 0;
    soundname[0] = 0;

    delay_msec = 0;
  }

  int channel;
  f32 volume;
  int pitch, pitchlow, pitchhigh;
  soundlevel_t soundlevel;
  // For weapon sounds...
  bool play_to_owner_only;
  int count;
  char soundname[128];
  int delay_msec;
};

// A bit of a hack, but these are just utility function which are implemented in
// the SouneParametersInternal.cpp file which all users of this lib also compile
const ch *SoundLevelToString(soundlevel_t level);
const ch *ChannelToString(int channel);
const ch *VolumeToString(f32 volume);
const ch *PitchToString(f32 pitch);
soundlevel_t TextToSoundLevel(const ch *key);
int TextToChannel(const ch *name);

enum gender_t {
  GENDER_NONE = 0,
  GENDER_MALE,
  GENDER_FEMALE,
};

#pragma pack(1)
struct SoundFile {
  SoundFile() : symbol{UTL_INVAL_SYMBOL}, gender{GENDER_NONE}, available{true} {
    static_assert(sizeof(SoundFile) == 4);
  }

  CUtlSymbol symbol;
  u8 gender;
  u8 available;
};
#pragma pack()

#pragma pack(1)
template <typename T>
struct sound_interval_t {
  T start, range;

  interval_t &ToInterval(interval_t &dest) const {
    dest.start = start;
    dest.range = range;
    return dest;
  }

  void FromInterval(const interval_t &from) {
    start = from.start;
    range = from.range;
  }

  T Random() const {
    return implicit_cast<T>(
        RandomInterval({implicit_cast<f32>(start), implicit_cast<f32>(range)}));
  }
};
#pragma pack()

using volume_interval_t = sound_interval_t<float16_with_assign>;
using soundlevel_interval_t = sound_interval_t<u16>;
using pitch_interval_t = sound_interval_t<u8>;

#pragma pack(1)
struct CSoundParametersInternal {
  CSoundParametersInternal();
  ~CSoundParametersInternal();

  void CopyFrom(const CSoundParametersInternal &src);

  bool operator==(const CSoundParametersInternal &other) const;

  const ch *VolumeToString(void) const;
  const ch *ChannelToString(void) const;
  const ch *SoundLevelToString(void) const;
  const ch *PitchToString(void) const;

  void VolumeFromString(const ch *sz);
  void ChannelFromString(const ch *sz);
  void PitchFromString(const ch *sz);
  void SoundLevelFromString(const ch *sz);

  int GetChannel() const { return channel; }
  const volume_interval_t &GetVolume() const { return volume; }
  const pitch_interval_t &GetPitch() const { return pitch; }
  const soundlevel_interval_t &GetSoundLevel() const { return soundlevel; }
  int GetDelayMsec() const { return delay_msec; }
  bool OnlyPlayToOwner() const { return play_to_owner_only; }
  bool HadMissingWaveFiles() const { return had_missing_wave_files; }
  bool UsesGenderToken() const { return uses_gender_token; }
  bool ShouldPreload() const { return m_bShouldPreload; }

  void SetChannel(int newChannel) { channel = newChannel; }
  void SetVolume(f32 start, f32 range = 0.0) {
    volume.start = start;
    volume.range = range;
  }
  void SetPitch(f32 start, f32 range = 0.0) {
    pitch.start = start;
    pitch.range = range;
  }
  void SetSoundLevel(f32 start, f32 range = 0.0) {
    soundlevel.start = start;
    soundlevel.range = range;
  }
  void SetDelayMsec(int delay) { delay_msec = delay; }
  void SetShouldPreload(bool bShouldPreload) {
    m_bShouldPreload = bShouldPreload;
  }
  void SetOnlyPlayToOwner(bool b) { play_to_owner_only = b; }
  void SetHadMissingWaveFiles(bool b) { had_missing_wave_files = b; }
  void SetUsesGenderToken(bool b) { uses_gender_token = b; }

  void AddSoundName(const SoundFile &soundFile) {
    AddToTail(&m_pSoundNames, &m_nSoundNames, soundFile);
  }
  int NumSoundNames() const { return m_nSoundNames; }
  SoundFile *GetSoundNames() {
    return (m_nSoundNames == 1) ? (SoundFile *)&m_pSoundNames : m_pSoundNames;
  }
  const SoundFile *GetSoundNames() const {
    return (m_nSoundNames == 1) ? (SoundFile *)&m_pSoundNames : m_pSoundNames;
  }

  void AddConvertedName(const SoundFile &soundFile) {
    AddToTail(&m_pConvertedNames, &m_nConvertedNames, soundFile);
  }
  int NumConvertedNames() const { return m_nConvertedNames; }
  SoundFile *GetConvertedNames() {
    return (m_nConvertedNames == 1) ? (SoundFile *)&m_pConvertedNames
                                    : m_pConvertedNames;
  }
  const SoundFile *GetConvertedNames() const {
    return (m_nConvertedNames == 1) ? (SoundFile *)&m_pConvertedNames
                                    : m_pConvertedNames;
  }

 private:
  void operator=(const CSoundParametersInternal &src) = delete;
  CSoundParametersInternal(const CSoundParametersInternal &src);

  void AddToTail(SoundFile **pDest, u16 *pDestCount, const SoundFile &source);

  SoundFile *m_pSoundNames;      // 4
  SoundFile *m_pConvertedNames;  // 8
  u16 m_nSoundNames;             // 10
  u16 m_nConvertedNames;         // 12

  volume_interval_t volume;          // 16
  soundlevel_interval_t soundlevel;  // 20
  pitch_interval_t pitch;            // 22
  u16 channel;                       // 24
  u16 delay_msec;                    // 26

  bool play_to_owner_only : 1;  // For weapon sounds...	// 27
  // Internal use, for warning about missing .wav files
  bool had_missing_wave_files : 1;
  bool uses_gender_token : 1;
  bool m_bShouldPreload : 1;

  u8 reserved;  // 28
};
#pragma pack()

// Base class for sound emitter system handling (can be used by tools).
the_interface ISoundEmitterSystemBase : public IAppSystem {
 public:
  // Init, shutdown called after we know what mod is running
  virtual bool ModInit() = 0;
  virtual void ModShutdown() = 0;

  virtual int GetSoundIndex(const ch *name) const = 0;
  virtual bool IsValidIndex(int idx) = 0;
  virtual int GetSoundCount() = 0;

  virtual const ch *GetSoundName(int idx) = 0;
  virtual bool GetParametersForSound(const ch *sound_name,
                                     CSoundParameters &params, gender_t gender,
                                     bool is_being_emitted = false) = 0;

  virtual const ch *GetWaveName(CUtlSymbol & sym) = 0;
  virtual CUtlSymbol AddWaveName(const ch *name) = 0;

  virtual soundlevel_t LookupSoundLevel(const ch *sound_name) = 0;
  virtual const ch *GetWavFileForSound(const ch *sound_name,
                                       ch const *actor_model) = 0;
  virtual const ch *GetWavFileForSound(const ch *sound_name,
                                       gender_t gender) = 0;
  virtual int CheckForMissingWavFiles(bool is_verbose) = 0;
  virtual const ch *GetSourceFileForSound(int idx) const = 0;

  // Iteration methods
  virtual int First() const = 0;
  virtual int Next(int i) const = 0;
  virtual int InvalidIndex() const = 0;

  virtual CSoundParametersInternal *InternalGetParametersForSound(int idx) = 0;

  // The host application is responsible for dealing with dirty sound scripts,
  // etc.
  virtual bool AddSound(const ch *sound_name, const ch *script_file,
                        const CSoundParametersInternal &params) = 0;
  virtual void RemoveSound(const ch *sound_name) = 0;
  virtual void MoveSound(const ch *sound_name, const ch *newscript) = 0;
  virtual void RenameSound(const ch *sound_name, const ch *newname) = 0;

  virtual void UpdateSoundParameters(
      const ch *sound_name, const CSoundParametersInternal &params) = 0;

  virtual int GetNumSoundScripts() const = 0;
  virtual char const *GetSoundScriptName(int index) const = 0;
  virtual bool IsSoundScriptDirty(int index) const = 0;
  virtual int FindSoundScript(const ch *name) const = 0;
  virtual void SaveChangesToSoundScript(int scriptindex) = 0;

  virtual void ExpandSoundNameMacros(CSoundParametersInternal & params,
                                     char const *wave_name) = 0;
  virtual gender_t GetActorGender(char const *actor_model) = 0;
  virtual void GenderExpandString(char const *actor_model, char const *in,
                                  char *out, int max_size) = 0;
  virtual void GenderExpandString(gender_t gender, char const *in, char *out,
                                  int max_size) = 0;
  virtual bool IsUsingGenderToken(char const *sound_name) = 0;

  // For blowing away caches based on filetimstamps of the manifest, or of any
  // of the
  //  .txt files that are read into the sound emitter system
  virtual u32 GetManifestFileTimeChecksum() = 0;

  // Called from both client and server (single player) or just one (server only
  // in dedicated server and client only if connected to a remote server) Called
  // by LevelInitPreEntity to override sound scripts for the mod with level
  // specific overrides based on custom mapnames, etc..
  virtual void AddSoundOverrides(char const *script_file) = 0;

  // Called by either client or server in LevelShutdown to clear out custom
  // overrides.
  virtual void ClearSoundOverrides() = 0;

  virtual bool GetParametersForSoundEx(
      const ch *sound_name, HSOUNDSCRIPTHANDLE &handle,
      CSoundParameters &params, gender_t gender,
      bool is_being_emitted = false) = 0;
  virtual soundlevel_t LookupSoundLevelByHandle(char const *soundname,
                                                HSOUNDSCRIPTHANDLE &handle) = 0;
};

#endif  // ISOUNDEMITTERSYSTEMBASE_H
