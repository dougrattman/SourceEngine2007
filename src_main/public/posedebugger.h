// Copyright c 1996-2007, Valve Corporation, All rights reserved.

#ifndef POSEDEBUGGER_H
#define POSEDEBUGGER_H

#include "mathlib/vector.h"
#include "tier1/interface.h"

class IClientNetworkable;
class CStudioHdr;
class CIKContext;

the_interface IPoseDebugger {
 public:
  virtual void StartBlending(IClientNetworkable * pEntity,
                             const CStudioHdr *pStudioHdr) = 0;

  virtual void AccumulatePose(
      const CStudioHdr *pStudioHdr, CIKContext *pIKContext, Vector pos[],
      Quaternion q[], int sequence, float cycle, const float poseParameter[],
      int boneMask, float flWeight, float flTime) = 0;
};

extern IPoseDebugger *g_pPoseDebugger;

#endif  // #ifndef POSEDEBUGGER_H
