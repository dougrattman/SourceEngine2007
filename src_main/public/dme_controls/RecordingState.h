// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef RECORDINGSTATE_H
#define RECORDINGSTATE_H

// Animation set editor recording states
enum RecordingState_t : int {
  AS_OFF = 0,
  AS_PREVIEW,
  AS_RECORD,
  AS_PLAYBACK,

  NUM_AS_RECORDING_STATES,
};

#endif  // RECORDINGSTATE_H
