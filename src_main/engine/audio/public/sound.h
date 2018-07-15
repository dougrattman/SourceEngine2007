// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: client sound i/o functions

#ifndef SOUND_H
#define SOUND_H

#include "base/include/base_types.h"
#include "datamap.h"
#include "engine/SndInfo.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "soundflags.h"
#include "tier0/include/basetypes.h"
#include "tier1/UtlVector.h"
#include "tier1/strtools.h"

#define MAX_SFX 2048

inline constexpr ch AUDIOSOURCE_CACHE_ROOTDIR[]{"maps/soundcache"};

class CSfxTable;
enum soundlevel_t;
struct SoundInfo_t;
struct AudioState_t;
class IFileList;

void S_Init();
void S_Shutdown();
bool S_IsInitted();

void S_StopAllSounds(bool should_clear_buffers);
void S_Update(const AudioState_t *pAudioState);
void S_ExtraUpdate();
void S_ClearBuffer();
void S_BlockSound();
void S_UnblockSound();
f32 S_GetMasterVolume();
void S_SoundFade(f32 percent, f32 holdtime, f32 intime, f32 outtime);
void S_OnLoadScreen(bool value);
void S_EnableThreadedMixing(bool bEnable);
void S_EnableMusic(bool bEnable);

struct StartSoundParams_t {
  StartSoundParams_t()
      : staticsound{false},
        userdata{0},
        soundsource{0},
        entchannel{CHAN_AUTO},
        pSfx{nullptr},
        origin{0.0f, 0.0f, 0.0f},
        direction{0.0f, 0.0f, 0.0f},
        bUpdatePositions{true},
        fvol{1.0f},
        soundlevel{SNDLVL_NORM},
        flags{SND_NOFLAGS},
        pitch{PITCH_NORM},
        fromserver{false},
        delay{0.0f},
        speakerentity{-1},
        suppressrecording{false},
        initialStreamPosition{0} {}

  bool staticsound;
  int userdata;
  int soundsource;
  int entchannel;
  CSfxTable *pSfx;
  Vector origin;
  Vector direction;
  bool bUpdatePositions;
  f32 fvol;
  soundlevel_t soundlevel;
  int flags;
  int pitch;
  bool fromserver;
  f32 delay;
  int speakerentity;
  bool suppressrecording;
  int initialStreamPosition;
};

int S_StartSound(StartSoundParams_t &params);
void S_StopSound(int entnum, int entchannel);

enum clocksync_index_t {
  CLOCK_SYNC_CLIENT = 0,
  CLOCK_SYNC_SERVER,
  NUM_CLOCK_SYNCS
};

extern f32 S_ComputeDelayForSoundtime(f32 soundtime,
                                      clocksync_index_t syncIndex);

void S_StopSoundByGuid(int guid);
f32 S_SoundDurationByGuid(int guid);
int S_GetGuidForLastSoundEmitted();
bool S_IsSoundStillPlaying(int guid);
void S_GetActiveSounds(CUtlVector<SndInfo_t> &sndlist);
void S_SetVolumeByGuid(int guid, f32 fvol);
f32 S_GetElapsedTimeByGuid(int guid);
bool S_IsLoopingSoundByGuid(int guid);
void S_ReloadSound(const ch *pSample);
f32 S_GetMono16Samples(const ch *pszName, CUtlVector<short> &sampleList);

CSfxTable *S_DummySfx(const ch *name);
CSfxTable *S_PrecacheSound(const ch *sample);
void S_PrefetchSound(ch const *name, bool bPlayOnce);
void S_MarkUISound(CSfxTable *pSfx);
void S_ReloadFilesInList(IFileList *pFilesToReload);

f32 S_GetNominalClipDist();

#include "soundchars.h"

// For recording movies
void SND_MovieStart();
void SND_MovieEnd();

int S_GetCurrentStaticSounds(SoundInfo_t *pResult, int nSizeResult,
                             int entchannel);
f32 S_GetGainFromSoundLevel(soundlevel_t soundlevel, f32 dist);

struct musicsave_t {
  DECLARE_SIMPLE_DATADESC();

  ch songname[128];
  int sampleposition;
  short master_volume;
};

void S_GetCurrentlyPlayingMusic(CUtlVector<musicsave_t> &list);
void S_RestartSong(const musicsave_t *song);

#endif  // SOUND_H
