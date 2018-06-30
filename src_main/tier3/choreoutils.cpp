// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Helper methods + classes for file access.

#include "tier3/choreoutils.h"

#include "../game/shared/choreoevent.h"
#include "../game/shared/choreoscene.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "bone_setup.h"
#include "soundchars.h"
#include "studio.h"
#include "tier1/KeyValues.h"
#include "tier3/tier3.h"

// Find sequence by name
static int LookupSequence(CStudioHdr *pStudioHdr, const char *pSequenceName) {
  for (int i = 0; i < pStudioHdr->GetNumSeq(); i++) {
    if (!_stricmp(pSequenceName, pStudioHdr->pSeqdesc(i).pszLabel())) return i;
  }

  return -1;
}

// Returns sequence flags
static int GetSequenceFlags(CStudioHdr *pStudioHdr, int nSequence) {
  if (!pStudioHdr || nSequence < 0 || nSequence >= pStudioHdr->GetNumSeq())
    return 0;

  mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc(nSequence);
  return seqdesc.flags;
}

// Does a sequence loop?
static bool DoesSequenceLoop(CStudioHdr *pStudioHdr, int nSequence) {
  int nFlags = GetSequenceFlags(pStudioHdr, nSequence);
  return (nFlags & STUDIO_LOOPING) ? true : false;
}

bool AutoAddGestureKeys(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                        f32 *pPoseParameters, bool bCheckOnly) {
  int iSequence = LookupSequence(pStudioHdr, e->GetParameters());
  if (iSequence < 0) return false;

  KeyValues *pSeqKeyValues = new KeyValues("");
  if (!pSeqKeyValues->LoadFromBuffer(
          pStudioHdr->name(), Studio_GetKeyValueText(pStudioHdr, iSequence))) {
    pSeqKeyValues->deleteThis();
    return false;  //-V773
  }

  // Do we have a build point section?
  KeyValues *pKVAllFaceposer = pSeqKeyValues->FindKey("faceposer");
  if (!pKVAllFaceposer) {
    pSeqKeyValues->deleteThis();
    return false;  //-V773
  }

  int nMaxFrame = Studio_MaxFrame(pStudioHdr, iSequence, pPoseParameters) - 1;

  // Start grabbing the sounds and slotting them in
  char szStartLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH]{"loop"};
  char szEndLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH]{"end"};
  char szEntry[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH]{"apex"};
  char szExit[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH]{"end"};

  KeyValues *pkvFaceposer;
  for (pkvFaceposer = pKVAllFaceposer->GetFirstSubKey(); pkvFaceposer;
       pkvFaceposer = pkvFaceposer->GetNextKey()) {
    if (!_stricmp(pkvFaceposer->GetName(), "startloop")) {
      strcpy_s(szStartLoop, pkvFaceposer->GetString());
      continue;
    }

    if (!_stricmp(pkvFaceposer->GetName(), "endloop")) {
      strcpy_s(szEndLoop, pkvFaceposer->GetString());
      continue;
    }

    if (!_stricmp(pkvFaceposer->GetName(), "entrytag")) {
      strcpy_s(szEntry, pkvFaceposer->GetString());
      continue;
    }

    if (!_stricmp(pkvFaceposer->GetName(), "exittag")) {
      strcpy_s(szExit, pkvFaceposer->GetString());
      continue;
    }

    if (!_stricmp(pkvFaceposer->GetName(), "tags")) {
      if (nMaxFrame <= 0) continue;

      KeyValues *pkvTags;
      for (pkvTags = pkvFaceposer->GetFirstSubKey(); pkvTags;
           pkvTags = pkvTags->GetNextKey()) {
        f32 flPercentage = (f32)pkvTags->GetInt() / nMaxFrame;

        CEventAbsoluteTag *ptag =
            e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, pkvTags->GetName());
        if (ptag) {
          // reposition tag
          ptag->SetPercentage(flPercentage);
        } else {
          e->AddAbsoluteTag(CChoreoEvent::ORIGINAL, pkvTags->GetName(),
                            flPercentage);
          e->AddAbsoluteTag(CChoreoEvent::PLAYBACK, pkvTags->GetName(),
                            flPercentage);
        }
        // lock the original tags so they can't be edited
        ptag = e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, pkvTags->GetName());
        Assert(ptag);
        ptag->SetLocked(true);
      }
      e->VerifyTagOrder();
      e->PreventTagOverlap();
      continue;
    }
  }

  // TODO(d.rattman): lookup linear tags in sequence data
  {
    CEventAbsoluteTag *ptag;
    ptag = e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, szStartLoop);
    if (ptag) ptag->SetLinear(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::PLAYBACK, szStartLoop);
    if (ptag) ptag->SetLinear(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, szEndLoop);
    if (ptag) ptag->SetLinear(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::PLAYBACK, szEndLoop);
    if (ptag) ptag->SetLinear(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, szEntry);
    if (ptag) ptag->SetEntry(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::PLAYBACK, szEntry);
    if (ptag) ptag->SetEntry(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::ORIGINAL, szExit);
    if (ptag) ptag->SetExit(true);

    ptag = e->FindAbsoluteTag(CChoreoEvent::PLAYBACK, szExit);
    if (ptag) ptag->SetExit(true);
  }

  pSeqKeyValues->deleteThis();
  return true;
}

bool UpdateGestureLength(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                         f32 *pPoseParameters, bool bCheckOnly) {
  Assert(e);
  if (!e) return false;

  if (e->GetType() != CChoreoEvent::GESTURE) return false;

  int iSequence = LookupSequence(pStudioHdr, e->GetParameters());
  if (iSequence < 0) return false;

  bool bChanged = false;
  f32 flSeqDuration = Studio_Duration(pStudioHdr, iSequence, pPoseParameters);
  f32 flCurDuration;
  e->GetGestureSequenceDuration(flCurDuration);
  if (flSeqDuration != 0.0f && flSeqDuration != flCurDuration) {
    bChanged = true;
    if (!bCheckOnly) {
      e->SetGestureSequenceDuration(flSeqDuration);
    }
  }

  return bChanged;
}

bool UpdateSequenceLength(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                          f32 *pPoseParameters, bool bCheckOnly,
                          bool bVerbose) {
  Assert(e);
  if (!e) return false;

  if (e->GetType() != CChoreoEvent::SEQUENCE) {
    if (bVerbose) {
      ConMsg("UpdateSequenceLength:  called on non-SEQUENCE event %s\n",
             e->GetName());
    }
    return false;
  }

  int iSequence = LookupSequence(pStudioHdr, e->GetParameters());
  if (iSequence < 0) return false;

  bool bChanged = false;
  bool bLooping = DoesSequenceLoop(pStudioHdr, iSequence);
  f32 flSeqDuration = Studio_Duration(pStudioHdr, iSequence, pPoseParameters);

  if (bLooping) {
    if (e->IsFixedLength()) {
      if (bCheckOnly) return true;

      if (bVerbose) {
        ConMsg(
            "UpdateSequenceLength:  %s is looping, removing fixed length "
            "flag\n",
            e->GetName());
      }
      bChanged = true;
    }
    e->SetFixedLength(false);

    if (!e->HasEndTime()) {
      if (bCheckOnly) return true;

      if (bVerbose) {
        ConMsg(
            "CheckSequenceLength:  %s is looping, setting default end time\n",
            e->GetName());
      }
      e->SetEndTime(e->GetStartTime() + flSeqDuration);
      bChanged = true;
    }

    return bChanged;
  }

  if (!e->IsFixedLength()) {
    if (bCheckOnly) return true;

    if (bVerbose) {
      ConMsg(
          "CheckSequenceLength:  %s is fixed length, removing looping flag\n",
          e->GetName());
    }
    bChanged = true;
  }
  e->SetFixedLength(true);

  if (e->HasEndTime()) {
    f32 dt = e->GetDuration();
    if (fabs(dt - flSeqDuration) > 0.01f) {
      if (bCheckOnly) return true;
      if (bVerbose) {
        ConMsg(
            "CheckSequenceLength:  %s has wrong duration, changing length from "
            "%f to %f seconds\n",
            e->GetName(), dt, flSeqDuration);
      }
      bChanged = true;
    }
  } else {
    if (bCheckOnly) return true;
    if (bVerbose) {
      ConMsg(
          "CheckSequenceLength:  %s has wrong duration, changing length to %f "
          "seconds\n",
          e->GetName(), flSeqDuration);
    }

    bChanged = true;
  }

  if (!bCheckOnly) e->SetEndTime(e->GetStartTime() + flSeqDuration);

  return bChanged;
}

// Finds sound files associated with events
const char *GetSoundForEvent(CChoreoEvent *pEvent, CStudioHdr *pStudioHdr) {
  const char *pSoundName = pEvent->GetParameters();
  if (Q_stristr(pSoundName, ".wav")) return PSkipSoundChars(pSoundName);

  const char *pFileName = g_pSoundEmitterSystem->GetWavFileForSound(
      pSoundName,
      (pStudioHdr && pStudioHdr->IsValid()) ? pStudioHdr->name() : nullptr);
  return PSkipSoundChars(pFileName);
}
