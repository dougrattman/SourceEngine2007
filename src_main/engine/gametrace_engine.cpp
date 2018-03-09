// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "gametrace.h"

#include "eiface.h"
#include "server.h"

 
#include "tier0/include/memdbgon.h"

void CGameTrace::SetEdict(edict_t* pEdict) {
  m_pEnt = serverGameEnts->EdictToBaseEntity(pEdict);
}

edict_t* CGameTrace::GetEdict() const {
  return serverGameEnts->BaseEntityToEdict(m_pEnt);
}
