// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Helper methods + classes for choreo.

#ifndef SOURCE_TIER3_CHOREOUTILS_H_
#define SOURCE_TIER3_CHOREOUTILS_H_


// Forward declarations

class CChoreoScene;
class CChoreoEvent;
class CStudioHdr;


// Finds sound files associated with events

const char *GetSoundForEvent(CChoreoEvent *pEvent, CStudioHdr *pStudioHdr);


// Fixes up the duration of a choreo scene based on wav files + animations
// Returns true if a change needed to be made

bool AutoAddGestureKeys(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                        float *pPoseParameters, bool bCheckOnly);
bool UpdateGestureLength(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                         float *pPoseParameters, bool bCheckOnly);
bool UpdateSequenceLength(CChoreoEvent *e, CStudioHdr *pStudioHdr,
                          float *pPoseParameters, bool bCheckOnly,
                          bool bVerbose);

#endif  // SOURCE_TIER3_CHOREOUTILS_H_
