// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose:		Base combat character with no AI

#ifndef HL1_AI_BASENPC_H
#define HL1_AI_BASENPC_H

#include <set>
#include "AI_Motor.h"
#include "ai_basenpc.h"
//=============================================================================
// >> CHL1NPCTalker
//=============================================================================

class CHL1BaseNPC : public CAI_BaseNPC {
  DECLARE_CLASS(CHL1BaseNPC, CAI_BaseNPC);

 public:
  CHL1BaseNPC() {}

  void TraceAttack(const CTakeDamageInfo &info, const Vector &vecDir,
                   trace_t *ptr);
  bool ShouldGib(const CTakeDamageInfo &info);
  bool CorpseGib(const CTakeDamageInfo &info);

  bool HasAlienGibs(void);
  bool HasHumanGibs(void);

  void Precache(void);

  int IRelationPriority(CBaseEntity *pTarget);
  bool NoFriendlyFire(void);

  void EjectShell(const Vector &vecOrigin, const Vector &vecVelocity,
                  float rotation, int iType);

  virtual int SelectDeadSchedule();
};

#endif  // HL1_AI_BASENPC_H